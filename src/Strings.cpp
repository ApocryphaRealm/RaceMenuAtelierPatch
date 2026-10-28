#include "Strings.h"

#include "Model.h"

namespace RMA::Strings
{
	namespace
	{
		constexpr std::string_view kStem = "RaceMenuAtelier";

		struct Hash
		{
			using is_transparent = void;
			std::size_t operator()(std::string_view a_s) const { return std::hash<std::string_view>{}(a_s); }
		};

		struct Entry
		{
			std::string text;     // the game's language
			std::string english;  // the English file's, else the compiled English
		};

		using Table = std::unordered_map<std::string, std::string, Hash, std::equal_to<>>;

		std::mutex  g_lock;
		bool        g_loaded = false;
		std::string g_language = "english";
		Table       g_localized;
		Table       g_englishFile;
		using Entries = std::unordered_map<std::string, Entry, Hash, std::equal_to<>>;  // node-based: an entry never moves
		Entries                               g_entries;
		std::vector<std::unique_ptr<Entries>> g_retired;  // entries of a language switched away from, kept so their text stays valid
		std::unordered_map<const char*, const char*>                  g_englishOf;  // text handed out -> its English

		// one translation file; empty (and logged) when it is missing or not UTF-16LE with a BOM
		Table ReadFile(std::string_view a_language)
		{
			Table      keys;
			const auto path = std::filesystem::path(L"Data\\Interface\\Translations") /
			                  std::filesystem::path(std::format("{}_{}.txt", kStem, a_language));
			std::ifstream file(path, std::ios::binary);
			if (!file) {
				logger::info("no translation file {}", path.filename().string());
				return keys;
			}
			const std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (raw.size() < 2 || static_cast<unsigned char>(raw[0]) != 0xFF || static_cast<unsigned char>(raw[1]) != 0xFE) {
				logger::warn("translation file {} is not UTF-16LE with a BOM; it was skipped", path.filename().string());
				return keys;
			}
			std::wstring text((raw.size() - 2) / 2, L'\0');
			std::memcpy(text.data(), raw.data() + 2, text.size() * sizeof(wchar_t));

			std::size_t start = 0;
			while (start < text.size()) {
				auto end = text.find(L'\n', start);
				if (end == std::wstring::npos) {
					end = text.size();
				}
				std::wstring line = text.substr(start, end - start);
				start = end + 1;
				if (!line.empty() && line.back() == L'\r') {
					line.pop_back();
				}
				if (line.size() < 2 || line[0] != L'$') {
					continue;
				}
				const auto tab = line.find(L'\t');
				if (tab == std::wstring::npos) {
					continue;
				}
				std::string value = Text::WideToUtf8(std::wstring_view(line).substr(tab + 1));
				// "\n" written in a file is a line break
				for (auto pos = value.find("\\n"); pos != std::string::npos; pos = value.find("\\n", pos + 1)) {
					value.replace(pos, 2, "\n");
				}
				keys[Text::WideToUtf8(std::wstring_view(line).substr(1, tab - 1))] = std::move(value);
			}
			logger::info("read {} translation key(s) from {}", keys.size(), path.filename().string());
			return keys;
		}
	}

	void Load()
	{
		const auto language = Text::GameLanguage();
		auto       english = ReadFile("english");
		auto       localized = language == "english" ? english : ReadFile(language);
		if (english.empty()) {
			logger::warn("the English translation file is missing; the compiled English text is used");
		}
		std::scoped_lock lock(g_lock);
		if (g_loaded) {
			return;
		}
		g_language = language;
		g_englishFile = std::move(english);
		g_localized = std::move(localized);
		g_loaded = true;
		logger::info("text language: {} ({} localized key(s), {} English key(s))", g_language, g_localized.size(), g_englishFile.size());
	}

	const char* Get(std::string_view a_key, const char* a_english)
	{
		std::scoped_lock lock(g_lock);
		if (!g_loaded) {
			return a_english;
		}
		if (const auto it = g_entries.find(a_key); it != g_entries.end()) {
			return it->second.text.c_str();
		}
		Entry entry;
		if (const auto it = g_englishFile.find(a_key); it != g_englishFile.end() && !it->second.empty()) {
			entry.english = it->second;
		} else {
			entry.english = a_english ? a_english : "";
		}
		if (const auto it = g_localized.find(a_key); it != g_localized.end() && !it->second.empty()) {
			entry.text = it->second;
		} else {
			entry.text = entry.english;
		}
		const auto [it, inserted] = g_entries.emplace(std::string(a_key), std::move(entry));
		g_englishOf[it->second.text.c_str()] = it->second.english.c_str();
		return it->second.text.c_str();
	}

	std::size_t SetLanguage(std::string_view a_language)
	{
		std::string language = a_language.empty() ? Text::GameLanguage() : Text::Lower(a_language);
		auto        localized = language == "english" ? Table{} : ReadFile(language);
		std::scoped_lock lock(g_lock);
		if (language == "english") {
			localized = g_englishFile;
		}
		const auto count = localized.size();
		g_retired.push_back(std::make_unique<Entries>(std::move(g_entries)));
		g_entries = Entries{};
		g_language = std::move(language);
		g_localized = std::move(localized);
		g_loaded = true;
		logger::info("text language set to {} by DevBench ({} localized key(s))", g_language, count);
		return count;
	}

	const char* English(const char* a_text)
	{
		std::scoped_lock lock(g_lock);
		const auto it = g_englishOf.find(a_text);
		return it == g_englishOf.end() ? a_text : it->second;
	}

	std::string Language()
	{
		std::scoped_lock lock(g_lock);
		return g_language;
	}

	std::string detail::Fill(std::string_view a_text, const std::vector<std::string>& a_values)
	{
		std::string out;
		out.reserve(a_text.size() + 16);
		for (std::size_t i = 0; i < a_text.size(); ++i) {
			if (a_text[i] == '{') {
				const auto close = a_text.find('}', i + 1);
				if (close != std::string_view::npos && close > i + 1 && close - i <= 3) {
					const auto digits = a_text.substr(i + 1, close - i - 1);
					if (std::ranges::all_of(digits, [](char c) { return c >= '0' && c <= '9'; })) {
						const auto n = static_cast<std::size_t>(std::stoi(std::string(digits)));
						if (n < a_values.size()) {
							out += a_values[n];
							i = close;
							continue;
						}
					}
				}
			}
			out += a_text[i];
		}
		return out;
	}
}
