#include "UI/Glyphs.h"

#include "Model.h"
#include "Strings.h"
#include "UI/Theme.h"

#include <nlohmann/json.hpp>

#include <Windows.h>

// The source is UTF-8 and is compiled with /utf-8, so the string literals below are UTF-8 as written.

namespace RMA::UI::Glyphs
{
	namespace
	{
		namespace im = ImGuiMCP;
		namespace fm = ImGuiMCP::ImFontManger;

		constexpr double kCheckEvery = 0.5;   // seconds between checks
		constexpr double kNoticeAfter = 2.0;  // a character missing this long shows the notice (AMF adds one within a frame)
		constexpr std::size_t kMaxSeen = 4096;  // distinct characters remembered; far beyond any one language's text

		enum Script : std::uint32_t
		{
			kCyrillic = 1u << 0,
			kPolish = 1u << 1,  // Latin Extended (Polish, Czech): the nearest set SKSE Menu Framework offers is "Polish"
			kTurkish = 1u << 2,
			kChinese = 1u << 3,
			kJapanese = 1u << 4,
			kKorean = 1u << 5,
			kThai = 1u << 6,
			kOther = 1u << 7,
		};

		struct ScriptName
		{
			Script      script;
			const char* option;  // SKSE Menu Framework's "Character Glyphs" entry, as it spells it
			const char* key;     // its SKSEMenuFramework.ini key
		};

		constexpr std::array kScripts{
			ScriptName{ kCyrillic, "Cyrillic", "EnableCyrillic" },
			ScriptName{ kPolish, "Polish", "EnablePolish" },
			ScriptName{ kTurkish, "Turkish", "EnableTurkish" },
			ScriptName{ kChinese, "Chinese", "EnableChinese" },
			ScriptName{ kJapanese, "Japanese", "EnableJapanese" },
			ScriptName{ kKorean, "Korean", "EnableKorean" },
			ScriptName{ kThai, "Thai", "EnableThai" },
		};

		Script ScriptOf(char32_t a_c, bool a_japaneseGame)
		{
			if ((a_c >= 0x0400 && a_c <= 0x052F) || (a_c >= 0x1C80 && a_c <= 0x1C8F)) {
				return kCyrillic;
			}
			if (a_c == 0x011E || a_c == 0x011F || a_c == 0x0130 || a_c == 0x0131 || a_c == 0x015E || a_c == 0x015F) {
				return kTurkish;
			}
			if (a_c >= 0x0100 && a_c <= 0x024F) {
				return kPolish;
			}
			if (a_c >= 0x0E00 && a_c <= 0x0E7F) {
				return kThai;
			}
			if ((a_c >= 0x3040 && a_c <= 0x30FF) || (a_c >= 0x31F0 && a_c <= 0x31FF) || (a_c >= 0xFF66 && a_c <= 0xFF9F)) {
				return kJapanese;
			}
			if ((a_c >= 0xAC00 && a_c <= 0xD7AF) || (a_c >= 0x1100 && a_c <= 0x11FF) || (a_c >= 0x3130 && a_c <= 0x318F)) {
				return kKorean;
			}
			if ((a_c >= 0x4E00 && a_c <= 0x9FFF) || (a_c >= 0x3400 && a_c <= 0x4DBF) || (a_c >= 0x3000 && a_c <= 0x303F) || (a_c >= 0xFF00 && a_c <= 0xFF65)) {
				return a_japaneseGame ? kJapanese : kChinese;
			}
			return kOther;
		}

		// 1.1.0: the notice's text is in the translation files with the rest of Atelier's text (RMA_Glyph*), so
		// translation-coverage checks it. {0} = the glyph sets ("Cyrillic"), {1} = their INI keys ("EnableCyrillic"); both
		// stay as SKSE Menu Framework shows and spells them, as do its menu names.

		enum class Framework
		{
			Unknown,
			Smf,
			Amf
		};

		const char* FrameworkName(Framework a_f)
		{
			switch (a_f) {
			case Framework::Smf:
				return "SKSE Menu Framework";
			case Framework::Amf:
				return "Apocrypha Menu Framework";
			default:
				return "unknown";
			}
		}

		Framework DetectFramework()
		{
			if (::GetModuleHandleW(L"!ApocryphaMenuFramework.dll") || ::GetModuleHandleW(L"ApocryphaMenuFramework.dll")) {
				return Framework::Amf;
			}
			if (::GetModuleHandleW(L"SKSEMenuFramework.dll")) {
				return Framework::Smf;
			}
			return Framework::Unknown;
		}

		// decodes UTF-8 (the text is made UTF-8 where it enters the model); a broken sequence yields U+FFFD
		template <class F>
		void ForEachCodepoint(std::string_view a_text, F&& a_fn)
		{
			std::size_t i = 0;
			while (i < a_text.size()) {
				const auto c = static_cast<unsigned char>(a_text[i]);
				if (c < 0x80) {
					++i;
					continue;
				}
				std::size_t len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 :
				                                       (c & 0xF8) == 0xF0       ? 4 :
				                                                                  0;
				char32_t cp = len == 2 ? (c & 0x1F) : len == 3 ? (c & 0x0F) :
				                                                 (c & 0x07);
				bool ok = len != 0 && i + len <= a_text.size();
				for (std::size_t k = 1; ok && k < len; ++k) {
					const auto t = static_cast<unsigned char>(a_text[i + k]);
					ok = (t & 0xC0) == 0x80;
					cp = (cp << 6) | (t & 0x3F);
				}
				a_fn(ok ? cp : char32_t{ 0xFFFD });
				i += ok ? len : 1;
			}
		}

		// render thread
		std::vector<char32_t>   g_pending;          // characters drawn in the frame being checked
		std::vector<bool>       g_seenFlag(0x10000, false);
		std::vector<char32_t>   g_seen;             // the non-ASCII characters the last checked frame drew
		std::vector<char32_t>   g_missing;          // ... that the font has no glyph for
		double                  g_lastCheck = -1.0;
		bool                    g_due = false;      // this frame is checked
		double                  g_missingSince = -1.0;
		std::uint32_t           g_noticeScripts = 0;  // the scripts the shown notice names
		std::string             g_notice;
		Framework               g_framework = Framework::Unknown;
		bool                    g_frameworkRead = false;
		bool                    g_broken = false;   // a broken UTF-8 sequence was seen (it draws as '?')

		// shared with DevBench
		std::mutex  g_reportLock;
		std::string g_report = R"({"ok":true,"checked":false})";

		bool HasGlyph(im::ImFont* a_font, char32_t a_c)
		{
			return a_font && a_c <= 0xFFFF && fm::FindGlyphNoFallback(a_font, static_cast<im::ImWchar>(a_c)) != nullptr;
		}

		bool CanDraw(im::ImFont* a_font, std::string_view a_text)
		{
			bool all = true;
			ForEachCodepoint(a_text, [&](char32_t c) { all = all && HasGlyph(a_font, c); });
			return all;
		}

		void BuildNotice(im::ImFont* a_font, std::uint32_t a_scripts)
		{
			std::string sets, keys;
			for (const auto& s : kScripts) {
				if (a_scripts & s.script) {
					sets += (sets.empty() ? "" : ", ") + std::string(s.option);
					keys += (keys.empty() ? "" : ", ") + std::string(s.key);
				}
			}
			const char* missing = TR("RMA_GlyphMissing", "Some letters cannot be drawn: the menu font has no {0} letters.");
			const char* smf = TR("RMA_GlyphSmf", "In SKSE Menu Framework choose Options > Open Settings > Character Glyphs > {0}, or set {1} = true in SKSEMenuFramework.ini and restart the game.");
			const char* amf = TR("RMA_GlyphAmf", "Apocrypha Menu Framework adds missing letters by itself; if this message stays, please report it with RaceMenuAtelier.log.");
			const auto  compose = [&](const char* a_missing, const char* a_smf, const char* a_amf) {
				std::string text = Strings::Format(a_missing, sets, keys);
				if (g_framework == Framework::Smf && !keys.empty()) {
					text += "\n" + Strings::Format(a_smf, sets, keys);
				} else if (g_framework == Framework::Amf) {
					text += "\n" + Strings::Format(a_amf, sets, keys);
				}
				return text;
			};
			g_notice = compose(missing, smf, amf);
			if (Strings::Language() != "english" && !CanDraw(a_font, g_notice)) {
				// the font cannot draw the player's language either: English can be read
				g_notice = compose(Strings::English(missing), Strings::English(smf), Strings::English(amf));
			}
		}

		void WriteReport(im::ImFont* a_font)
		{
			using json = nlohmann::json;
			json missing = json::array();
			std::uint32_t scripts = 0;
			const bool    japanese = Text::GameLanguage() == "japanese";
			for (std::size_t i = 0; i < g_missing.size(); ++i) {
				scripts |= ScriptOf(g_missing[i], japanese);
				if (i < 24) {
					missing.push_back(std::format("U+{:04X}", static_cast<std::uint32_t>(g_missing[i])));
				}
			}
			json sets = json::array();
			for (const auto& s : kScripts) {
				if (scripts & s.script) {
					sets.push_back(s.option);
				}
			}
			if (scripts & kOther) {
				sets.push_back("other");
			}
			json probes = json::object();
			for (const auto& [name, c] : { std::pair{ "cyrillic_U+0416", char32_t{ 0x0416 } }, std::pair{ "polish_U+0141", char32_t{ 0x0141 } },
					 std::pair{ "czech_U+0159", char32_t{ 0x0159 } }, std::pair{ "kana_U+3042", char32_t{ 0x3042 } },
					 std::pair{ "hangul_U+D55C", char32_t{ 0xD55C } }, std::pair{ "hanzi_U+6C34", char32_t{ 0x6C34 } } }) {
				probes[name] = HasGlyph(a_font, c);
			}
			const char* fontName = a_font ? fm::GetDebugName(a_font) : nullptr;
			json        out = {
                { "ok", true },
                { "checked", true },
                { "framework", FrameworkName(g_framework) },
                { "font", fontName ? fontName : "" },
                { "game_language", Text::GameLanguage() },
                { "seen", g_seen.size() },
                { "missing_count", g_missing.size() },
                { "missing", std::move(missing) },
                { "missing_sets", std::move(sets) },
                { "broken_utf8", g_broken },
                { "notice_shown", !g_notice.empty() },
                { "notice", g_notice },
                { "probes", std::move(probes) },
			};
			std::scoped_lock lock(g_reportLock);
			g_report = out.dump();
		}
	}

	void Note(std::string_view a_text)
	{
		if (!g_due) {
			return;
		}
		ForEachCodepoint(a_text, [](char32_t c) {
			if (c == 0xFFFD) {
				if (!g_broken) {
					g_broken = true;
					logger::warn("the editor was handed text that is not UTF-8; it draws as '?'");
				}
				return;
			}
			if (c <= 0xFFFF && !g_seenFlag[c] && g_pending.size() < kMaxSeen) {
				g_seenFlag[c] = true;  // claimed now so a frame notes it once; checked in Check()
				g_pending.push_back(c);
			}
		});
	}

	void BeginFrame() { g_due = Text::Now() - g_lastCheck >= kCheckEvery; }

	void Check()
	{
		const double now = Text::Now();
		if (!g_frameworkRead) {
			g_frameworkRead = true;
			g_framework = DetectFramework();
			logger::info("menu framework hosting the editor: {}", FrameworkName(g_framework));
		}
		if (!g_due) {
			return;
		}
		g_due = false;
		g_lastCheck = now;
		auto* font = im::GetFont();
		if (!font) {
			for (const auto c : g_pending) {
				g_seenFlag[c] = false;
			}
			g_pending.clear();
			return;
		}
		// this frame's characters only: what an earlier frame drew and this one does not is no longer counted
		const bool japanese = Text::GameLanguage() == "japanese";
		std::uint32_t newlyMissing = 0;
		g_seen.clear();
		g_missing.clear();
		for (const auto c : g_pending) {
			g_seenFlag[c] = false;
			g_seen.push_back(c);
			if (!HasGlyph(font, c)) {
				g_missing.push_back(c);
				newlyMissing |= ScriptOf(c, japanese);
			}
		}
		const bool changed = !g_pending.empty();
		g_pending.clear();
		if (newlyMissing) {
			static std::uint32_t logged = 0;
			if ((newlyMissing & ~logged) != 0) {
				logged |= newlyMissing;
				logger::warn("the menu font ({}) has no glyph for {} characters the editor shows (first U+{:04X}); they draw as '?'",
					FrameworkName(g_framework), g_missing.size(), g_missing.empty() ? 0u : static_cast<std::uint32_t>(g_missing.front()));
			}
		}

		// a character of no glyph set a framework offers is logged and reported, but names nothing to switch on
		std::uint32_t scripts = 0;
		for (const auto c : g_missing) {
			scripts |= ScriptOf(c, japanese);
		}
		scripts &= ~static_cast<std::uint32_t>(kOther);
		if (scripts == 0) {
			if (!g_notice.empty()) {
				logger::info("every character the editor shows now has a glyph; the notice is gone");
			}
			g_missingSince = -1.0;
			g_notice.clear();
			g_noticeScripts = 0;
		} else {
			if (g_missingSince < 0.0) {
				g_missingSince = now;
			}
			if (now - g_missingSince >= kNoticeAfter && (g_notice.empty() || scripts != g_noticeScripts)) {
				g_noticeScripts = scripts;
				BuildNotice(font, scripts);
				logger::info("missing-letter notice shown: {}", g_notice);
			}
		}
		static_cast<void>(changed);
		WriteReport(font);
	}

	const std::string& Notice() { return g_notice; }

	std::string ReportJson()
	{
		std::scoped_lock lock(g_reportLock);
		return g_report;
	}
}
