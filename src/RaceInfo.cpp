#include "RaceInfo.h"

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
			info.description = OwnDescription(*race, race);
			ReadSpells(*race, info);
			ReadStats(*race, info);
		}
		if (info.description.empty()) {
			info.description = Clean(a_listDescription);
		}
		return info;
	}
}
