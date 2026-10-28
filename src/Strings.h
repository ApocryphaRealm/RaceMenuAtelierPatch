#pragma once

// Atelier's own text in the eleven shipped languages (rule 66), 1.1.0.
//
// TR("RMA_<Name>", "English text") returns the text for the game's language, read from
//     Data\Interface\Translations\RaceMenuAtelier_<language>.txt
// (UTF-16LE with BOM, one "$key<TAB>text" per line - the SKSE/SkyUI shape our other mods use). A key the language file
// lacks falls back to the English file, then to the compiled English, so a half-finished translation never shows a raw
// key. The language is the game's sLanguage:General.
//
// The pointer TR returns stays valid for the whole session (the text is kept once per key), so it can be handed to
// ImGui as it is. Text with values in it uses numbered places - {0}, {1} - filled by Strings::Format, so a language can
// put them in its own order.
//
// Text only a tool or the log reads stays English (rule 66's exception): Strings::English gives the English form of a
// string TR returned, which is what the log and the DevBench tool report.
namespace RMA::Strings
{
	// Reads the language files once. Call at kDataLoaded, when the game's INI settings are final; TR before that
	// returns the compiled English.
	void Load();

	// the text for a key; a_english is the compiled fallback (a string literal)
	[[nodiscard]] const char* Get(std::string_view a_key, const char* a_english);

	// the English form of a string Get returned (the English file's, else the compiled one); any other text as it is
	[[nodiscard]] const char* English(const char* a_text);

	// the language that was loaded ("english" until Load has run)
	[[nodiscard]] std::string Language();

	// DevBench (atelier.control op=language): show the text in another shipped language for this session without changing
	// the game's ("" = back to the game's sLanguage). Text handed out before stays valid. Returns the keys read.
	std::size_t SetLanguage(std::string_view a_language);

	namespace detail
	{
		inline std::string ToText(const std::string& a_v) { return a_v; }
		inline std::string ToText(std::string_view a_v) { return std::string(a_v); }
		inline std::string ToText(const char* a_v) { return a_v ? std::string(a_v) : std::string(); }
		inline std::string ToText(char* a_v) { return a_v ? std::string(a_v) : std::string(); }
		template <class T>
			requires std::is_arithmetic_v<T>
		std::string ToText(T a_v)
		{
			return std::format("{}", a_v);
		}

		std::string Fill(std::string_view a_text, const std::vector<std::string>& a_values);
	}

	// fills {0}, {1} ... with the values; a place with no value stays as written (a broken translation never throws)
	template <class... Args>
	[[nodiscard]] std::string Format(const char* a_text, const Args&... a_values)
	{
		return detail::Fill(a_text ? std::string_view(a_text) : std::string_view(), { detail::ToText(a_values)... });
	}
}

#define TR(key, english) ::RMA::Strings::Get(key, english)
