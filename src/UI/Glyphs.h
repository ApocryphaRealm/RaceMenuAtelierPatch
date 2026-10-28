#pragma once

#include <string>
#include <string_view>
#include <vector>

// 1.0.2 - letters the menu font cannot draw (letoposlednee, Nexus, 2026-10-27: "Does not support Cyrillic alphabet.
// Instead of letters - ?????").
//
// The editor draws all its text in the font of the menu framework hosting it, and owns no font atlas of its own.
// SKSE Menu Framework 3 ships with every non-Latin glyph set switched off ([Fonts] EnableCyrillic, EnableChinese,
// EnableJapanese, EnableKorean, EnablePolish ... = false in SKSEMenuFramework.ini), so a Russian slider name, race
// name, preset name or typed name has no glyph in its atlas and Dear ImGui draws its fallback, '?', for every letter.
// Apocrypha Menu Framework adds a missing character to its atlas the first time a page draws it (2.2.0).
//
// So the editor checks the text it shows against the font (ImFont::FindGlyphNoFallback) twice a second; a character
// still missing after two seconds - long enough for AMF to have added it - shows a notice above the categories that
// names the glyph set and tells the player where to switch it on in their framework. The notice is drawn in the
// game's language when the font can draw that translation, in English otherwise (an English notice can always be
// read; a Russian one about missing Cyrillic could not). Render thread, inside the editor window; the state the
// DevBench tool reads is copied under a lock.
namespace RMA::UI::Glyphs
{
	// 1.1.0 (combined run 52ee, 2026-10-27): only text DRAWN in the checked frame counts. The draw helpers (Widgets, and
	// the Drawn wrappers in UI.cpp) note what they draw; a text no frame draws - a dialog that is not open, a category not
	// shown - is never counted, and nothing carries over from one check to the next (so a language switched away from
	// leaves nothing behind).

	// at the start of the editor's frame: decides whether this frame is checked (twice a second)
	void BeginFrame();

	// a text drawn this frame (ignored unless the frame is checked)
	void Note(std::string_view a_text);

	// at the end of the editor's frame: checks the characters drawn this frame against the menu font
	void Check();

	// the notice to draw above the categories ("" = none)
	[[nodiscard]] const std::string& Notice();

	// DevBench (atelier.control op=glyphs): a JSON object - framework, font, characters seen and missing, the glyph sets
	// they belong to, whether the notice shows, and one probe character per script (Cyrillic ZHE, Polish L WITH STROKE,
	// Czech R WITH CARON, hiragana A, hangul HAN, the hanzi for water). Any thread.
	[[nodiscard]] std::string ReportJson();
}
