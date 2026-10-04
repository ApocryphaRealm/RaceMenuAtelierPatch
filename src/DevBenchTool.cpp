#include "DevBenchTool.h"

#include "Bridge.h"
#include "DevBench/DevBenchAPI.h"
#include "Model.h"
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
				{ "hasData", model.hasData },
				{ "entries", model.entries.size() },
				{ "races", races },
				{ "playerRace", model.player.race },
				{ "picked", model.picked },
				{ "categories", std::move(categories) },
				{ "choices", std::move(choices) },
				{ "status", model.status },
			};
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
			} else {
				out = State();
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
			"op=refresh - re-read RaceMenu.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"}}},"
			"\"readOnly\":false"
			"}";
		if (devBench->RegisterTool("atelier.control", descriptor, &Tool, nullptr)) {
			logger::info("Registered \"atelier.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
