#include "RaceInfo.h"

#include "PluginRaces.h"

namespace RMA::RaceData
{
	namespace
	{
		std::string Number(float a_value)
		{
			const auto rounded = std::round(a_value);
			if (std::fabs(a_value - rounded) < 0.05f) {
				return std::format("{}", static_cast<std::int64_t>(rounded));
			}
			return std::format("{:.1f}", a_value);
		}

		bool IsNumber(std::string_view a_text)
		{
			return !a_text.empty() && std::ranges::all_of(a_text, [](char c) { return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+'; });
		}

		// a tag's text: what replaces it
		std::string Resolve(std::string_view a_tag, const RE::Effect* a_effect)
		{
			if (IsNumber(a_tag)) {
				return std::string(a_tag);  // the game's way of highlighting a typed number: <25> shows as 25
			}
			const auto lower = Text::Lower(a_tag);
			if (a_effect) {
				if (lower == "mag") {
					return Number(a_effect->GetMagnitude());
				}
				if (lower == "dur") {
					return std::format("{}", a_effect->GetDuration());
				}
				if (lower == "area") {
					return std::format("{}", a_effect->GetArea());
				}
			}
			if (lower.starts_with("global=")) {
				const std::string id(a_tag.substr(7));
				if (const auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(id)) {
					return Number(global->value);
				}
				return {};
			}
			if (lower == "br" || lower == "br/" || lower == "p") {
				return "\n";
			}
			return {};
		}

		std::string Tidy(const std::string& a_text)
		{
			std::string out;
			out.reserve(a_text.size());
			for (const char c : a_text) {
				if (c == '\r') {
					continue;
				}
				const char now = c == '\t' ? ' ' : c;
				if (now == ' ' && (out.empty() || out.back() == ' ' || out.back() == '\n')) {
					continue;  // the source's double spaces, and spaces a dropped tag left behind
				}
				if (now == '\n' && !out.empty() && out.back() == ' ') {
					out.pop_back();
				}
				out += now;
			}
			while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) {
				out.pop_back();
			}
			return out;
		}

		bool Shown(const RE::Effect* a_effect)
		{
			const auto* base = a_effect ? a_effect->baseEffect : nullptr;
			return base && !base->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kHideInUI);
		}

		std::string OwnDescription(RE::TESDescription& a_component, RE::TESForm* a_parent)
		{
			RE::BSString text;
			a_component.GetDescription(text, a_parent);
			const std::string_view raw = text.c_str() ? text.c_str() : "";
			// what the game hands back for a text its strings file does not hold
			return raw == "LOOKUP FAILED!" ? std::string() : Clean(raw);
		}

		// What the player reads for a spell: the spell's own text when it has one, else what the game's magic menu builds
		// from its effects. The join by hand is only the fallback if the game's call gives nothing.
		std::string SpellDescription(RE::SpellItem& a_spell)
		{
			if (auto own = OwnDescription(a_spell, &a_spell); !own.empty()) {
				return own;
			}
			RE::BSString built;
			RE::MagicSystem::GetMagicItemDescription(built, &a_spell, "", "");
			if (auto text = Clean(built.c_str() ? built.c_str() : ""); !text.empty()) {
				return text;
			}
			std::string joined;
			for (const auto* effect : a_spell.effects) {
				if (!Shown(effect) || effect->baseEffect->magicItemDescription.empty()) {
					continue;
				}
				if (auto line = Clean(effect->baseEffect->magicItemDescription.c_str(), effect); !line.empty()) {
					joined += (joined.empty() ? "" : " ") + line;
				}
			}
			return joined;
		}

		RaceSpellKind KindOf(RE::MagicSystem::SpellType a_type)
		{
			using Type = RE::MagicSystem::SpellType;
			switch (a_type) {
			case Type::kAbility:
				return RaceSpellKind::Passive;
			case Type::kPower:
				return RaceSpellKind::Power;
			case Type::kLesserPower:
				return RaceSpellKind::LesserPower;
			case Type::kVoicePower:
				return RaceSpellKind::Shout;
			case Type::kSpell:
				return RaceSpellKind::Spell;
			default:
				return RaceSpellKind::Other;
			}
		}

		// One entry per name and kind: a race mod often splits an ability across spells of one name ("Roots" and its stones)
		void Add(RaceInfo& a_info, RaceSpellKind a_kind, std::string a_name, std::string a_description)
		{
			for (auto& spell : a_info.spells) {
				if (spell.kind == a_kind && spell.name == a_name) {
					if (!a_description.empty() && spell.description != a_description) {
						spell.description += (spell.description.empty() ? "" : "\n") + a_description;
					}
					return;
				}
			}
			a_info.spells.push_back({ a_kind, std::move(a_name), std::move(a_description) });
		}

		void ReadSpells(RE::TESRace& a_race, RaceInfo& a_info)
		{
			const auto* list = a_race.actorEffects;
			if (!list) {
				return;
			}
			if (list->spells) {
				for (std::uint32_t i = 0; i < list->numSpells; ++i) {
					auto* spell = list->spells[i];
					if (!spell) {
						continue;
					}
					const auto* rawName = spell->GetFullName();
					const auto  name = rawName ? Text::ToUtf8(rawName) : std::string();
					auto        description = SpellDescription(*spell);
					const auto  kind = KindOf(spell->data.spellType);
					// A race's helper spells (no name, or a passive with nothing the player is meant to read) stay out, as
					// they do in the game's own magic menu.
					const bool anyShown = std::ranges::any_of(spell->effects, [](const RE::Effect* a_effect) { return Shown(a_effect); });
					if (name.empty() || (kind == RaceSpellKind::Passive && description.empty() && !anyShown)) {
						++a_info.hiddenSpells;
						continue;
					}
					Add(a_info, kind, name, std::move(description));
				}
			}
			if (list->shouts) {
				for (std::uint32_t i = 0; i < list->numShouts; ++i) {
					auto*       shout = list->shouts[i];
					const auto* rawName = shout ? shout->GetFullName() : nullptr;
					if (!rawName || !*rawName) {
						continue;
					}
					Add(a_info, RaceSpellKind::Shout, Text::ToUtf8(rawName), OwnDescription(*shout, shout));
				}
			}
			std::ranges::stable_sort(a_info.spells, {}, [](const RaceSpell& a_spell) { return static_cast<int>(a_spell.kind); });
		}

		std::string ValueName(RE::ActorValue a_value)
		{
			const auto* info = RE::ActorValueList::GetActorValueInfo(a_value);
			const auto* name = info ? info->GetFullName() : nullptr;
			return name ? Text::ToUtf8(name) : std::string();
		}

		void ReadStats(const RE::TESRace& a_race, RaceInfo& a_info)
		{
			constexpr std::string_view kBetween = "  \xC2\xB7  ";
			const auto add = [&](std::string& a_line, const std::string& a_text) {
				if (!a_line.empty()) {
					a_line += kBetween;
				}
				a_line += a_text;
			};
			const auto stat = [&](RE::ActorValue a_value, float a_amount) {
				if (const auto name = ValueName(a_value); !name.empty()) {
					add(a_info.attributes, std::format("{} {}", name, Number(a_amount)));
				}
			};
			stat(RE::ActorValue::kHealth, a_race.data.startingHealth);
			stat(RE::ActorValue::kMagicka, a_race.data.startingMagicka);
			stat(RE::ActorValue::kStamina, a_race.data.startingStamina);
			stat(RE::ActorValue::kCarryWeight, a_race.data.baseCarryWeight);

			for (const auto& boost : a_race.data.skillBoosts) {
				const auto skill = boost.skill.underlying();
				if (boost.bonus == 0 || skill == 0xFF) {
					continue;  // an unused slot
				}
				if (const auto name = ValueName(static_cast<RE::ActorValue>(skill)); !name.empty()) {
					add(a_info.skills, std::format("+{} {}", boost.bonus, name));
				}
			}
		}

		// ------------------------------------------------------------------
		// the race's text, from the plugin whose race is the one the player gets
		//
		// The owner, 2026-11-05: the panel should show the race mod's own text, not whatever plugin happens to win the
		// record. A list can load another plugin over a race mod's races and apply the mod's spells by SkyPatcher, so the
		// winning record's text describes abilities the race no longer has. Nothing is changed in any record. Of the
		// plugins holding this race, the text comes from the last one whose spell list is closest to the live list; when
		// that is the winner, or no plugin shares a spell with the live list, the game's own text stands. No plugin is
		// named: the live spell list alone says which one it is.

		struct LoadedPlugin
		{
			struct Race
			{
				const PluginRaces::Race* record;
				RE::FormID               formID;
				std::set<RE::FormID>     spellList;
			};

			PluginRaces::File file;
			std::vector<Race> races;
		};

		// a form id as a plugin writes it -> the running game's
		RE::FormID RuntimeID(std::uint32_t a_written, const PluginRaces::File& a_file, std::string_view a_fileName)
		{
			auto*             data = RE::TESDataHandler::GetSingleton();
			const std::size_t index = a_written >> 24;
			const std::string_view owner = index < a_file.masters.size() ? std::string_view(a_file.masters[index]) : a_fileName;
			const auto*       mod = data->LookupModByName(owner);
			if (!mod) {
				return 0;
			}
			return data->LookupFormID(a_written & (mod->IsLight() ? 0xFFFu : 0xFFFFFFu), owner);
		}

		// Each file once per game session: what a plugin holds does not change while the game runs. Game thread only.
		const LoadedPlugin& Load(std::string_view a_fileName)
		{
			static std::unordered_map<std::string, std::unique_ptr<LoadedPlugin>> cache;
			const std::string name(a_fileName);
			auto&             slot = cache[name];
			if (slot) {
				return *slot;
			}
			slot = std::make_unique<LoadedPlugin>();
			slot->file = PluginRaces::Read("Data\\" + name);  // relative, as the game opens it: a mod manager's folders apply
			if (!slot->file.readable) {
				logger::info("race panel: could not read {}", name);
			}
			slot->races.reserve(slot->file.races.size());
			for (const auto& record : slot->file.races) {
				LoadedPlugin::Race race{ &record, RuntimeID(record.formID, slot->file, name), {} };
				for (const auto written : record.spellList) {
					if (const auto id = RuntimeID(written, slot->file, name)) {
						race.spellList.insert(id);
					}
				}
				slot->races.push_back(std::move(race));
			}
			return *slot;
		}

		std::set<RE::FormID> LiveSpellList(const RE::TESRace& a_race)
		{
			std::set<RE::FormID> out;
			const auto*          list = a_race.actorEffects;
			if (!list) {
				return out;
			}
			const auto add = [&](const auto* a_forms, std::uint32_t a_count) {
				for (std::uint32_t i = 0; a_forms && i < a_count; ++i) {
					if (a_forms[i]) {
						out.insert(a_forms[i]->GetFormID());
					}
				}
			};
			add(list->spells, list->numSpells);
			add(list->levSpells, list->numlevSpells);
			add(list->shouts, list->numShouts);
			return out;
		}

		// shared spells over all spells of the two lists: 1 is the same list
		double Closeness(const std::set<RE::FormID>& a_lhs, const std::set<RE::FormID>& a_rhs)
		{
			std::size_t shared = 0;
			for (const auto id : a_lhs) {
				shared += a_rhs.contains(id) ? 1 : 0;
			}
			const std::size_t all = a_lhs.size() + a_rhs.size() - shared;
			return all ? static_cast<double>(shared) / static_cast<double>(all) : 0.0;
		}

		// a localized plugin's text: its strings file for the game's language, loose or in an archive, as the game finds it
		std::string LocalizedText(std::string_view a_fileName, std::uint32_t a_id)
		{
			const auto dot = a_fileName.rfind('.');
			const auto path = std::format("Strings\\{}_{}.DLSTRINGS", a_fileName.substr(0, dot), Text::GameLanguage());
			RE::BSResourceNiBinaryStream stream(path);
			if (!stream.good()) {
				return {};
			}
			std::array<char, 8> head{};
			if (!stream.read(head.data(), 8)) {
				return {};
			}
			std::uint32_t count = 0;
			std::uint32_t size = 0;
			std::memcpy(&count, head.data(), 4);
			std::memcpy(&size, head.data() + 4, 4);
			const std::uint64_t rest = static_cast<std::uint64_t>(count) * 8 + size;
			if (rest > 128u * 1024u * 1024u) {
				return {};
			}
			std::string data(8 + static_cast<std::size_t>(rest), '\0');
			std::memcpy(data.data(), head.data(), 8);
			if (!stream.read(data.data() + 8, static_cast<std::uint32_t>(rest))) {
				return {};
			}
			return PluginRaces::LookupDLString(data, a_id);
		}

		void ReadDescription(RE::TESRace& a_race, RaceInfo& a_info)
		{
			a_info.description = OwnDescription(a_race, &a_race);
			const auto* files = a_race.sourceFiles.array;
			const auto* winner = a_race.GetFile();
			if (!files || !winner) {
				return;
			}
			a_info.textSource = winner->GetFilename();
			if (files->size() < 2) {
				return;  // no plugin but the one that made it
			}
			const auto  live = LiveSpellList(a_race);
			double      best = 0.0;
			double      winnerCloseness = 0.0;
			const RE::TESFile*       bestFile = nullptr;
			const PluginRaces::Race* bestRecord = nullptr;
			for (const auto* file : *files) {
				if (!file) {
					continue;
				}
				const auto&  plugin = Load(file->GetFilename());
				const auto   it = std::ranges::find_if(plugin.races, [&](const LoadedPlugin::Race& a_entry) { return a_entry.formID == a_race.GetFormID(); });
				if (it == plugin.races.end()) {
					continue;
				}
				const double closeness = Closeness(live, it->spellList);
				if (file == winner) {
					winnerCloseness = closeness;
				}
				const bool hasText = plugin.file.localized ? it->record->stringID != 0 : !it->record->description.empty();
				if (hasText && closeness > 0.0 && closeness >= best) {
					best = closeness;
					bestFile = file;
					bestRecord = it->record;
				}
			}
			a_info.textCloseness = winnerCloseness;
			const char* editorID = a_race.GetFormEditorID() ? a_race.GetFormEditorID() : "";
			if (bestFile && bestFile != winner) {
				const auto& plugin = Load(bestFile->GetFilename());
				auto        text = Clean(plugin.file.localized ? LocalizedText(bestFile->GetFilename(), bestRecord->stringID) : bestRecord->description);
				if (!text.empty()) {
					a_info.description = std::move(text);
					a_info.textSource = bestFile->GetFilename();
					a_info.textCloseness = best;
				}
			}
			logger::info("race panel: {} text from {} (spell list closeness {:.2f}; the winner {} {:.2f})", editorID, a_info.textSource, a_info.textCloseness, winner->GetFilename(), winnerCloseness);
		}

		// The race behind a name on RaceMenu's list. The list carries only the name, and several records can share one: the
		// playable race beats a helper record, the living race beats its vampire form, and the oldest form id beats a
		// quest's copy (DLC1NordRace, DA13AfflictedRace).
		RE::TESRace* Find(std::string_view a_name)
		{
			auto* data = RE::TESDataHandler::GetSingleton();
			if (!data || a_name.empty()) {
				return nullptr;
			}
			const auto   wanted = Text::ToUtf8(a_name);
			RE::TESRace* best = nullptr;
			int          bestScore = -1;
			for (auto* race : data->GetFormArray<RE::TESRace>()) {
				const auto* name = race ? race->GetFullName() : nullptr;
				if (!name || !*name || Text::ToUtf8(name) != wanted) {
					continue;
				}
				const int score = (race->GetPlayable() ? 2 : 0) + (race->HasKeywordString("Vampire") ? 0 : 1);
				if (score > bestScore || (score == bestScore && best && race->GetFormID() < best->GetFormID())) {
					best = race;
					bestScore = score;
				}
			}
			return best;
		}
	}

	std::string Clean(std::string_view a_text, const RE::Effect* a_effect)
	{
		std::string out;
		out.reserve(a_text.size());
		std::size_t at = 0;
		while (at < a_text.size()) {
			const auto open = a_text.find('<', at);
			if (open == std::string_view::npos) {
				out += a_text.substr(at);
				break;
			}
			out += a_text.substr(at, open - at);
			const auto close = a_text.find('>', open + 1);
			if (close == std::string_view::npos) {
				out += a_text.substr(open);  // a lone '<' is just a character
				break;
			}
			out += Resolve(a_text.substr(open + 1, close - open - 1), a_effect);
			at = close + 1;
		}
		return Tidy(Text::ToUtf8(out));
	}

	RaceInfo Read(std::string_view a_name, std::string_view a_listDescription)
	{
		RaceInfo info;
		if (auto* race = Find(a_name)) {
			info.found = true;
			info.editorID = race->GetFormEditorID() ? race->GetFormEditorID() : "";
			info.formID = race->GetFormID();
			ReadDescription(*race, info);
			ReadSpells(*race, info);
			ReadStats(*race, info);
		}
		if (info.description.empty()) {
			info.description = Clean(a_listDescription);
		}
		return info;
	}
}
