#include "DevBenchTool.h"

#include "Bridge.h"
#include "DevBench/DevBenchAPI.h"
#include "Model.h"
#include "Strings.h"
#include "UI/Glyphs.h"
#include "UI/UI.h"

#include <nlohmann/json.hpp>

// atelier.control (rule 64 of the fork's pipeline: every interactive surface ships a driving tool). Runs on
// DevBench's listener thread; everything it reads or changes goes through the model lock the UI holds per frame.

namespace RMA::DevBenchTool
{
	namespace
	{
		using json = nlohmann::json;

		const char* ControlName(Control a_control)
		{
			switch (a_control) {
			case Control::Race:
				return "race";
			case Control::Choice:
				return "choice";
			case Control::Slider:
				return "slider";
			case Control::Stepper:
				return "stepper";
			case Control::Sex:
				return "sex";
			case Control::Paint:
				return "paint";
			}
			return "?";
		}

		json State()
		{
			auto&            model = Model::Get();
			std::scoped_lock lock(model.mutex);
			json categories = json::array();
			for (const auto& c : model.categories) {
				categories.push_back({ { "label", c.label }, { "raw", c.raw }, { "flag", c.flag } });
			}
			// choices grouped by their category flag, with the label of the category that shows them
			json choices = json::object();
			int  races = 0;
			for (const auto& e : model.entries) {
				if (e.control == Control::Race) {
					++races;
				}
				if (e.control != Control::Choice) {
					continue;
				}
				std::string group = std::to_string(e.filterFlag);
				for (const auto& c : model.categories) {
					if (c.flag && (static_cast<std::uint64_t>(e.filterFlag) & c.flag) != 0) {
						group = c.label;
						break;
					}
				}
				choices[group].push_back({ { "label", e.label }, { "key", e.callback }, { "swfIndex", e.swfIndex }, { "current", model.IsCurrentChoice(e) } });
			}
			return {
				{ "ok", true },
				{ "mode", static_cast<int>(model.mode) },
				{ "yielding", model.yieldCount },  // menus on top that hold the input (the console, a message box)
				{ "hasData", model.hasData },
				{ "entries", model.entries.size() },
				{ "races", races },
				{ "playerRace", model.player.race },
				{ "picked", model.picked },
				{ "categories", std::move(categories) },
				{ "choices", std::move(choices) },
				{ "status", model.statusEnglish },  // English whatever the game's language (rule 66: a tool's text)
				{ "status_shown", model.status },
				{ "text_language", Strings::Language() },
			};
		}

		// 1.0.2: the preset list as the editor shows it - each file's UTF-8 path, the name shown where it differs (a
		// name RaceMenu saved from typed text), and whether RaceMenu can open it on this Windows
		json Presets()
		{
			auto&            model = Model::Get();
			std::scoped_lock lock(model.mutex);
			json             out = json::array();
			for (const auto& f : model.presetFiles) {
				const auto label = model.presetLabels.find(f);
				out.push_back({ { "file", f }, { "shown", label != model.presetLabels.end() ? label->second : f }, { "loadable", !model.presetUnloadable.contains(f) } });
			}
			return { { "ok", true }, { "loaded", model.presetsLoaded }, { "presets", std::move(out) } };
		}

		json Find(const std::string& a_name, Control a_control)
		{
			auto&            model = Model::Get();
			std::scoped_lock lock(model.mutex);
			const auto       want = Text::Lower(a_name);
			for (auto& e : model.entries) {
				if (e.control != a_control || (Text::Lower(e.label) != want && Text::Lower(e.text) != want && Text::Lower(e.callback) != want)) {
					continue;
				}
				if (a_control == Control::Choice) {
					model.PressChoice(e);
				} else {
					model.ChangeRace(e);
				}
				return { { "ok", true }, { "label", e.label }, { "swfIndex", e.swfIndex }, { "control", ControlName(e.control) } };
			}
			return { { "ok", false }, { "error", "no " + std::string(ControlName(a_control)) + " named '" + a_name + "'" } };
		}

		void Tool(void*, const char* a_args, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			json args = json::parse(a_args ? a_args : "{}", nullptr, false);
			if (args.is_discarded() || !args.is_object()) {
				args = json::object();
			}
			const std::string op = args.value("op", "state");
			const std::string name = args.value("name", "");
			json              out;
			if (op == "choose") {
				out = Find(name, Control::Choice);
			} else if (op == "race") {
				out = Find(name, Control::Race);
			} else if (op == "category") {
				out = { { "ok", UI::SelectCategoryNamed(name) }, { "category", UI::CurrentCategory() } };
			} else if (op == "refresh") {
				Bridge::RequestRefresh();
				out = { { "ok", true } };
			} else if (op == "glyphs") {
				// 1.0.2: letters the menu font cannot draw, from the editor's last check (twice a second while it is open)
				out = json::parse(UI::Glyphs::ReportJson(), nullptr, false);
				if (out.is_discarded()) {
					out = { { "ok", false }, { "error", "no report" } };
				}
			} else if (op == "set_text") {
				// 1.0.2: field=name|preset|search|bodyslide text=<UTF-8> - put in on the next frame, as if typed
				const std::string field = args.value("field", "");
				const std::string text = args.value("text", "");
				out = { { "ok", UI::SetText(field, text) }, { "field", field }, { "note", "applied on the next frame - read op=typed" } };
			} else if (op == "typed") {
				const std::string field = args.value("field", "name");
				out = { { "ok", true }, { "field", field }, { "text", UI::TypedText(field) } };
			} else if (op == "presets") {
				Bridge::ListPresets();
				out = Presets();
				out["note"] = "the list is re-read now; call op=presets again for the fresh one";
			} else if (op == "save_preset") {
				// 1.0.2: save the current character as a preset named name=<UTF-8>, as the Save button does
				const bool valid = Bridge::IsSafePresetName(name);
				if (valid) {
					Bridge::SavePreset(name);
				}
				out = { { "ok", valid }, { "name", name } };
			} else if (op == "nudge") {
				// 1.1.0: name=uiscale|editorwidth|rotatespeed dir=1|-1 count=N - open the options, highlight that slider and
				// step it N units as Left/Right on it do (next frame); read the values back with op=options
				const int dir = args.value("dir", 1);
				const int count = args.value("count", 1);
				out = { { "ok", UI::NudgeSlider(name, dir, count) }, { "name", name }, { "note", "applied on the next frame - read op=options" } };
			} else if (op == "options") {
				out = json::parse(UI::Options(), nullptr, false);
				if (out.is_discarded()) {
					out = { { "ok", false } };
				} else {
					out["ok"] = true;
				}
			} else if (op == "language") {
				// 1.1.0: name=<english|japanese|korean|chinese|russian|german|french|spanish|italian|polish|czech> shows
				// Atelier's own text in that language for this session (empty = the game's sLanguage); the game is untouched
				const auto keys = Strings::SetLanguage(name);
				out = { { "ok", keys > 0 }, { "text_language", Strings::Language() }, { "keys", keys }, { "sample", Strings::Get("RMA_DoneTitle", "Finish character creation?") } };
			} else if (op == "pad_done") {
				// the Done confirmation opened the way controller X opens it (applied on the editor's next frame); read
				// done_confirm_open / done_finish_focused with op=state a frame later
				UI::PadDone();
				out = { { "ok", true }, { "note", "opens on the next frame - read op=state" } };
			} else {
				out = State();
				out["done_confirm_open"] = UI::DoneConfirmOpen();
				out["done_finish_focused"] = UI::DoneFinishFocused();
				out["category"] = UI::CurrentCategory();
			}
			a_write(a_sink, out.dump().c_str());
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) {
			return;
		}
		auto* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench) {
			if (a_lastAttempt) {
				logger::info("DevBench not detected; the \"atelier.control\" tool is not registered");
			}
			return;
		}
		constexpr const char* descriptor =
			"{"
			"\"description\":\"RaceMenu Atelier (patched): op=state (default) - mode, categories, races, the choice lists "
			"other mods add (Apprentice's classes and traits) with the current pick, the bottom bar's picked names; "
			"op=choose name=<label or key> - pick a choice through the menu's own item-press handler; op=race name=<race> "
			"- change race as the race tiles do; op=category name=<label or raw, empty = all> - the editor's category; "
			"op=refresh - re-read RaceMenu; op=glyphs - letters the menu font cannot draw (framework, missing characters and "
			"glyph sets, the notice, one probe per script); op=set_text field=name|preset|search|bodyslide text=<UTF-8> - put "
			"text in a field as if typed; op=typed field=<field> - read it back; op=presets - the preset list as shown; "
			"op=save_preset name=<UTF-8> - save the character as a preset; op=pad_done - open the Finish confirmation as "
			"controller X does; op=language name=<language, empty = the game's> - Atelier's own text in another shipped "
			"language for this session; op=nudge name=uiscale|editorwidth|rotatespeed dir=1|-1 count=N - open the options, "
			"highlight that slider and step it N units of 0.01; op=options - the three option values and the last nudge.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},"
			"\"field\":{\"type\":\"string\"},\"text\":{\"type\":\"string\"},\"dir\":{\"type\":\"integer\"},\"count\":{\"type\":\"integer\"}}},"
			"\"readOnly\":false"
			"}";
		if (devBench->RegisterTool("atelier.control", descriptor, &Tool, nullptr)) {
			logger::info("Registered \"atelier.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
