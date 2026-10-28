#include "UI/Glyphs.h"

#include "Model.h"
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

		// The notice in the eleven languages: what is missing (any framework), then what to do in SKSE Menu Framework
		// or in Apocrypha Menu Framework. {0} = the glyph sets ("Cyrillic"), {1} = their INI keys ("EnableCyrillic");
		// both stay as SKSE Menu Framework shows and spells them, as do its menu names.
		struct NoticeText
		{
			const char* language;  // sLanguage
			const char* missing;
			const char* smf;
			const char* amf;
		};

		constexpr std::array kNotices{
			NoticeText{ "english",
				"Some letters cannot be drawn: the menu font has no {0} letters.",
				"In SKSE Menu Framework choose Options > Open Settings > Character Glyphs > {0}, or set {1} = true in SKSEMenuFramework.ini and restart the game.",
				"Apocrypha Menu Framework adds missing letters by itself; if this message stays, please report it with RaceMenuAtelier.log." },
			NoticeText{ "japanese",
				"一部の文字を表示できません: メニューのフォントに {0} の文字がありません。",
				"SKSE Menu Framework で Options > Open Settings > Character Glyphs > {0} を選ぶか、SKSEMenuFramework.ini で {1} = true にしてゲームを再起動してください。",
				"Apocrypha Menu Framework は足りない文字を自動で追加します。このメッセージが消えない場合は RaceMenuAtelier.log を添えて報告してください。" },
			NoticeText{ "korean",
				"일부 글자를 표시할 수 없습니다: 메뉴 글꼴에 {0} 문자가 없습니다.",
				"SKSE Menu Framework에서 Options > Open Settings > Character Glyphs > {0}을(를) 선택하거나, SKSEMenuFramework.ini에서 {1} = true로 설정한 뒤 게임을 다시 시작하세요.",
				"Apocrypha Menu Framework는 빠진 글자를 스스로 추가합니다. 이 메시지가 계속 보이면 RaceMenuAtelier.log와 함께 알려 주세요." },
			NoticeText{ "chinese",
				"部分文字无法显示：菜单字体缺少 {0} 字符。",
				"请在 SKSE Menu Framework 中选择 Options > Open Settings > Character Glyphs > {0}，或在 SKSEMenuFramework.ini 中设置 {1} = true 并重启游戏。",
				"Apocrypha Menu Framework 会自动补充缺少的文字；如果此消息一直存在，请附上 RaceMenuAtelier.log 报告。" },
			NoticeText{ "russian",
				"Некоторые буквы не отображаются: в шрифте меню нет букв набора {0}.",
				"В SKSE Menu Framework выберите Options > Open Settings > Character Glyphs > {0} или укажите {1} = true в SKSEMenuFramework.ini и перезапустите игру.",
				"Apocrypha Menu Framework добавляет недостающие буквы сам; если это сообщение не исчезает, сообщите об этом и приложите RaceMenuAtelier.log." },
			NoticeText{ "german",
				"Einige Buchstaben können nicht angezeigt werden: Der Menüschrift fehlen die Zeichen für {0}.",
				"Wähle in SKSE Menu Framework Options > Open Settings > Character Glyphs > {0} oder setze {1} = true in SKSEMenuFramework.ini und starte das Spiel neu.",
				"Apocrypha Menu Framework ergänzt fehlende Buchstaben selbst; bleibt diese Meldung, melde es bitte mit RaceMenuAtelier.log." },
			NoticeText{ "french",
				"Certaines lettres ne peuvent pas être affichées : la police du menu n'a pas les caractères {0}.",
				"Dans SKSE Menu Framework, choisissez Options > Open Settings > Character Glyphs > {0}, ou mettez {1} = true dans SKSEMenuFramework.ini et relancez le jeu.",
				"Apocrypha Menu Framework ajoute lui-même les lettres manquantes ; si ce message reste, signalez-le avec RaceMenuAtelier.log." },
			NoticeText{ "spanish",
				"Algunas letras no se pueden mostrar: la fuente del menú no tiene los caracteres {0}.",
				"En SKSE Menu Framework elige Options > Open Settings > Character Glyphs > {0}, o pon {1} = true en SKSEMenuFramework.ini y reinicia el juego.",
				"Apocrypha Menu Framework añade por sí mismo las letras que faltan; si este mensaje sigue, infórmalo junto con RaceMenuAtelier.log." },
			NoticeText{ "italian",
				"Alcune lettere non possono essere mostrate: il font del menu non ha i caratteri {0}.",
				"In SKSE Menu Framework scegli Options > Open Settings > Character Glyphs > {0}, oppure imposta {1} = true in SKSEMenuFramework.ini e riavvia il gioco.",
				"Apocrypha Menu Framework aggiunge da sé le lettere mancanti; se questo messaggio resta, segnalalo allegando RaceMenuAtelier.log." },
			NoticeText{ "polish",
				"Niektórych liter nie można wyświetlić: czcionka menu nie ma znaków {0}.",
				"W SKSE Menu Framework wybierz Options > Open Settings > Character Glyphs > {0} albo ustaw {1} = true w SKSEMenuFramework.ini i uruchom grę ponownie.",
				"Apocrypha Menu Framework sam dodaje brakujące litery; jeśli ten komunikat nie znika, zgłoś to i dołącz RaceMenuAtelier.log." },
			NoticeText{ "czech",
				"Některá písmena nelze zobrazit: písmo nabídky nemá znaky {0}.",
				"V SKSE Menu Framework zvolte Options > Open Settings > Character Glyphs > {0}, nebo nastavte {1} = true v SKSEMenuFramework.ini a restartujte hru.",
				"Apocrypha Menu Framework chybějící písmena doplňuje sám; pokud tato zpráva nezmizí, nahlaste to a přiložte RaceMenuAtelier.log." },
		};

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

		std::string Format(std::string a_text, const std::string& a_0, const std::string& a_1)
		{
			for (const auto& [token, value] : { std::pair{ std::string_view{ "{0}" }, &a_0 }, std::pair{ std::string_view{ "{1}" }, &a_1 } }) {
				for (auto pos = a_text.find(token); pos != std::string::npos; pos = a_text.find(token, pos + value->size())) {
					a_text.replace(pos, token.size(), *value);
				}
			}
			return a_text;
		}

		// render thread
		std::vector<char32_t>   g_pending;          // characters noted this frame
		std::vector<bool>       g_seenFlag(0x10000, false);
		std::vector<char32_t>   g_seen;             // every non-ASCII character the editor has shown
		std::vector<char32_t>   g_missing;          // ... that the font has no glyph for
		const void*             g_font = nullptr;   // the font the last check used
		double                  g_lastCheck = -1.0;
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
			const auto lang = Text::GameLanguage();
			const NoticeText* chosen = &kNotices[0];
			for (const auto& n : kNotices) {
				if (lang == n.language) {
					chosen = &n;
					break;
				}
			}
			const auto compose = [&](const NoticeText& n) {
				std::string text = Format(n.missing, sets, keys);
				if (g_framework == Framework::Smf && !keys.empty()) {
					text += "\n" + Format(n.smf, sets, keys);
				} else if (g_framework == Framework::Amf) {
					text += "\n" + Format(n.amf, sets, keys);
				}
				return text;
			};
			g_notice = compose(*chosen);
			if (chosen != &kNotices[0] && !CanDraw(a_font, g_notice)) {
				g_notice = compose(kNotices[0]);  // the font cannot draw the player's language either: English can be read
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
		ForEachCodepoint(a_text, [](char32_t c) {
			if (c == 0xFFFD) {
				if (!g_broken) {
					g_broken = true;
					logger::warn("the editor was handed text that is not UTF-8; it draws as '?'");
				}
				return;
			}
			if (c <= 0xFFFF && !g_seenFlag[c] && g_seen.size() + g_pending.size() < kMaxSeen) {
				g_seenFlag[c] = true;  // claimed now so a frame notes it once; checked in Check()
				g_pending.push_back(c);
			}
		});
	}

	bool Due() { return Text::Now() - g_lastCheck >= kCheckEvery; }

	void Check()
	{
		const double now = Text::Now();
		if (!g_frameworkRead) {
			g_frameworkRead = true;
			g_framework = DetectFramework();
			logger::info("menu framework hosting the editor: {}", FrameworkName(g_framework));
		}
		if (now - g_lastCheck < kCheckEvery) {
			return;
		}
		g_lastCheck = now;
		auto* font = im::GetFont();
		if (!font) {
			return;
		}
		if (font != g_font) {
			// a new font (a font or size change in the framework): every character is checked again
			g_font = font;
			g_missing.clear();
			g_pending.insert(g_pending.end(), g_seen.begin(), g_seen.end());
			g_seen.clear();
		}
		// characters missing earlier may have been added since (AMF adds them on demand)
		const auto before = g_missing.size();
		std::erase_if(g_missing, [&](char32_t c) { return HasGlyph(font, c); });
		const bool recovered = g_missing.size() < before;
		const bool japanese = Text::GameLanguage() == "japanese";
		std::uint32_t newlyMissing = 0;
		for (const auto c : g_pending) {
			g_seen.push_back(c);
			if (!HasGlyph(font, c)) {
				g_missing.push_back(c);
				newlyMissing |= ScriptOf(c, japanese);
			}
		}
		const bool changed = !g_pending.empty() || recovered;
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
