#include "Model.h"

#include "Bridge.h"

namespace RMA
{
	namespace
	{
		constexpr std::size_t kHistoryCap = 200;
		constexpr double      kLocalEditHold = 1.5;  // seconds a local edit wins over a re-read

		bool IsWordChar(char a_c)
		{
			return std::isalnum(static_cast<unsigned char>(a_c)) || a_c == '_';
		}

		bool IsSeparator(char a_c)
		{
			return a_c == ' ' || a_c == ':' || a_c == '.' || a_c == '_' || a_c == '-';
		}

		bool StartsWithNoCase(std::string_view a_text, std::string_view a_prefix)
		{
			if (a_text.size() < a_prefix.size()) {
				return false;
			}
			for (std::size_t i = 0; i < a_prefix.size(); ++i) {
				if (std::tolower(static_cast<unsigned char>(a_text[i])) != std::tolower(static_cast<unsigned char>(a_prefix[i]))) {
					return false;
				}
			}
			return true;
		}

		void Trim(std::string& a_text)
		{
			const auto first = a_text.find_first_not_of(" \t");
			if (first == std::string::npos) {
				a_text.clear();
				return;
			}
			const auto last = a_text.find_last_not_of(" \t");
			a_text = a_text.substr(first, last - first + 1);
		}

		void SkipSeparators(std::string& a_text)
		{
			std::size_t i = 0;
			while (i < a_text.size() && IsSeparator(a_text[i])) {
				++i;
			}
			a_text.erase(0, i);
		}

		// "rm", "rsm", "racemenu", "ece" optionally followed by "sliders", "morphs", ...
		bool StripFamilyPrefix(std::string& a_text)
		{
			static constexpr std::array families{ "racemenu"sv, "rsm"sv, "rm"sv, "ece"sv };
			static constexpr std::array kinds{ "sliders"sv, "slider"sv, "morphs"sv, "morph"sv, "custom"sv };
			for (const auto family : families) {
				if (!StartsWithNoCase(a_text, family)) {
					continue;
				}
				std::size_t at = family.size();
				if (at < a_text.size() && IsWordChar(a_text[at]) && a_text[at] != '_') {
					continue;  // "Rmfoo" is a word, not a tag
				}
				std::size_t afterSeps = at;
				while (afterSeps < a_text.size() && IsSeparator(a_text[afterSeps])) {
					++afterSeps;
				}
				const std::string_view rest(a_text.data() + afterSeps, a_text.size() - afterSeps);
				for (const auto kind : kinds) {
					if (afterSeps > at && StartsWithNoCase(rest, kind) && (rest.size() == kind.size() || !std::isalnum(static_cast<unsigned char>(rest[kind.size()])))) {
						at = afterSeps + kind.size();
						break;
					}
				}
				a_text.erase(0, at);
				SkipSeparators(a_text);
				return true;
			}
			return false;
		}

		// registration / mod-family tags such as "SPG ECENose" -> "Nose"
		bool StripTechnicalPrefix(std::string& a_text)
		{
			static constexpr std::array tags{
				"completevanillaeyeoverhaul"sv, "racemenu"sv, "expr"sv, "cveo"sv, "rans"sv, "skse"sv,
				"cme"sv, "efm"sv, "nsk"sv, "ran"sv, "spg"sv, "ece"sv, "rsm"sv, "ldd"sv, "rm"sv
			};
			for (const auto tag : tags) {
				if (!StartsWithNoCase(a_text, tag)) {
					continue;
				}
				const auto at = tag.size();
				const bool boundary = at == a_text.size() || IsSeparator(a_text[at]) || std::isupper(static_cast<unsigned char>(a_text[at]));
				if (!boundary) {
					continue;
				}
				a_text.erase(0, at);
				SkipSeparators(a_text);
				return true;
			}
			return false;
		}

		// "Breasts - Size" -> {"Breasts", "Size"}; the group needs two characters
		void SplitGroup(const std::string& a_label, std::string& a_group, std::string& a_rest)
		{
			a_group.clear();
			a_rest = a_label;
			for (std::size_t i = 2; i < a_label.size(); ++i) {
				std::size_t sepLen = 0;
				if (a_label[i] == '-' || a_label[i] == ':') {
					sepLen = 1;
				} else if (a_label.compare(i, 3, "\xE2\x80\x93") == 0) {  // en dash
					sepLen = 3;
				}
				if (!sepLen || i + sepLen >= a_label.size() || a_label[i + sepLen] != ' ') {
					continue;
				}
				std::string group = a_label.substr(0, i);
				Trim(group);
				std::string rest = a_label.substr(i + sepLen);
				Trim(rest);
				if (group.size() >= 2 && !rest.empty()) {
					a_group = std::move(group);
					a_rest = std::move(rest);
				}
				return;
			}
		}

		bool IsSexEntry(const Entry& a_entry)
		{
			if (a_entry.callback == "ChangeSex") {
				return true;
			}
			if (a_entry.min == 0.0 && a_entry.max == 1.0 && a_entry.interval >= 1.0) {
				const auto lower = Text::Lower(a_entry.rawText + " " + a_entry.text);
				return lower.find("sex") != std::string::npos;
			}
			return false;
		}

		bool IsOverlayTint(int a_tintType)
		{
			return a_tintType >= 256 && a_tintType <= 259;
		}

		void Derive(Entry& a_entry)
		{
			a_entry.label = a_entry.text;
			SplitGroup(a_entry.label, a_entry.group, a_entry.shortLabel);
			a_entry.search = Text::Lower(a_entry.label + "\n" + a_entry.rawText + "\n" + a_entry.callback);
			a_entry.custom = a_entry.sliderID >= Defs::kCustomSliderOffset;
			a_entry.hasColor = a_entry.filterFlag > 0 && (a_entry.filterFlag & Defs::kCategoryColor) && a_entry.tintType >= 0;

			// Only custom continuous body morphs may travel past their registered
			// range; vanilla face sliders, weight and tints keep RaceMenu's limits.
			const auto cb = Text::Lower(a_entry.callback);
			const bool bodyCallback = cb.starts_with("changedoublemorph") || cb.starts_with("changemorphcbbe") || cb.starts_with("rsm_bodymorph_himbo");
			a_entry.canOverdrive = a_entry.type == Defs::kTypeSlider && bodyCallback && a_entry.custom && a_entry.interval < 1.0;

			if (a_entry.type == Defs::kTypeRace) {
				a_entry.control = Control::Race;
			} else if (a_entry.type >= Defs::kTypeWarPaint && a_entry.type <= Defs::kTypeFacePaint) {
				a_entry.control = Control::Paint;
			} else if (IsSexEntry(a_entry)) {
				a_entry.control = Control::Sex;
			} else if (!a_entry.hasColor && a_entry.interval >= 1.0 && (a_entry.max - a_entry.min) <= 80.0) {
				a_entry.control = Control::Stepper;
			} else {
				a_entry.control = Control::Slider;
			}
		}
	}

	// ------------------------------------------------------------------
	// text

	std::string Text::Lower(std::string_view a_text)
	{
		std::string out(a_text);
		for (auto& c : out) {
			if (static_cast<unsigned char>(c) < 0x80) {
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
		}
		return out;
	}

	std::string Text::Humanise(std::string_view a_text, bool a_dropVariant)
	{
		std::string label(a_text);
		Trim(label);
		while (!label.empty() && label.front() == '$') {
			label.erase(0, 1);
		}
		// "[Tag]_ Name" -> "Tag Name"
		if (!label.empty() && label.front() == '[') {
			if (const auto close = label.find(']'); close != std::string::npos) {
				std::string tag = label.substr(1, close - 1);
				std::string rest = label.substr(close + 1);
				SkipSeparators(rest);
				label = tag + " " + rest;
			}
		}
		StripFamilyPrefix(label);
		while (!label.empty() && label.front() == '$') {
			label.erase(0, 1);
		}
		for (int guard = 0; guard < 8 && StripTechnicalPrefix(label); ++guard) {
		}

		// camelCase and separators are implementation syntax, not UI copy
		std::string spaced;
		spaced.reserve(label.size() + 8);
		for (std::size_t i = 0; i < label.size(); ++i) {
			const char c = label[i];
			if (i > 0 && std::isupper(static_cast<unsigned char>(c)) && std::islower(static_cast<unsigned char>(label[i - 1]))) {
				spaced += ' ';
			}
			spaced += (c == '_' || c == '-') ? ' ' : c;
		}
		std::string collapsed;
		collapsed.reserve(spaced.size());
		for (const char c : spaced) {
			if (c == ' ' && (collapsed.empty() || collapsed.back() == ' ')) {
				continue;
			}
			collapsed += c;
		}
		Trim(collapsed);
		label = std::move(collapsed);

		// trailing side / sex markers
		const auto expand = [&](char a_from, std::string_view a_to) {
			if (label.size() >= 2 && std::toupper(static_cast<unsigned char>(label.back())) == a_from && label[label.size() - 2] == ' ') {
				label.pop_back();
				label += a_to;
				return true;
			}
			return false;
		};
		static_cast<void>(expand('L', "Left") || expand('R', "Right") || expand('M', "Male") || expand('F', "Female"));

		if (a_dropVariant) {
			// "Hair 03 Left Female" -> "Hair"
			auto words = std::vector<std::string>{};
			std::size_t start = 0;
			while (start < label.size()) {
				const auto end = label.find(' ', start);
				words.emplace_back(label.substr(start, end == std::string::npos ? std::string::npos : end - start));
				if (end == std::string::npos) {
					break;
				}
				start = end + 1;
			}
			const auto isSex = [](const std::string& w) { return w == "Male" || w == "Female" || w == "M" || w == "F"; };
			const auto isSide = [](const std::string& w) { return w == "Left" || w == "Right"; };
			const auto isNumber = [](const std::string& w) { return !w.empty() && std::all_of(w.begin(), w.end(), [](unsigned char c) { return std::isdigit(c); }); };
			if (words.size() >= 3 && isSex(words.back())) {
				std::size_t n = words.size() - 1;
				if (n >= 2 && isSide(words[n - 1])) {
					--n;
				}
				if (n >= 2 && isNumber(words[n - 1])) {
					--n;
					std::string joined;
					for (std::size_t i = 0; i < n; ++i) {
						joined += (i ? " " : "") + words[i];
					}
					label = joined;
				}
			}
		}
		return label;
	}

	std::string Text::Translate(const std::string& a_text)
	{
		if (a_text.empty()) {
			return a_text;
		}
		if (a_text.front() != '$') {
			// readable names ("Breasts - Size") stay as they are; registration
			// keys such as "ECE_Morphs_BreastSize" are made readable
			const bool technical = a_text.find(' ') == std::string::npos &&
			                       (a_text.find('_') != std::string::npos || std::ranges::any_of(a_text, [](unsigned char c) { return std::isupper(c); }));
			return technical ? Humanise(a_text) : a_text;
		}
		std::string translated;
		if (SKSE::Translation::Translate(a_text, translated) && !translated.empty() && translated != a_text && translated.front() != '$') {
			return translated;
		}
		return Humanise(a_text);
	}

	std::string Text::FileStem(std::string_view a_path)
	{
		const auto slash = a_path.find_last_of("\\/");
		std::string name(slash == std::string_view::npos ? a_path : a_path.substr(slash + 1));
		if (const auto dot = name.rfind('.'); dot != std::string::npos) {
			name.erase(dot);
		}
		return name;
	}

	double Text::Now()
	{
		using clock = std::chrono::steady_clock;
		static const auto start = clock::now();
		return std::chrono::duration<double>(clock::now() - start).count();
	}

	// ------------------------------------------------------------------
	// entries

	bool Entry::IsChanged() const
	{
		if (control == Control::Race) {
			return false;
		}
		if (std::abs(position - initialPosition) > 1e-4) {
			return true;
		}
		if ((hasColor || control == Control::Paint) && fillColor != initialFill) {
			return true;
		}
		return control == Control::Paint && texture != initialTexture;
	}

	Model& Model::Get()
	{
		static Model model;
		return model;
	}

	void Model::ApplySnapshot(Snapshot&& a_snapshot)
	{
		std::unordered_map<std::string, Entry> previous;
		previous.reserve(entries.size());
		for (auto& entry : entries) {
			previous.emplace(entry.key, std::move(entry));
		}

		const double now = Text::Now();
		std::unordered_map<std::string, int> seen;
		byKey.clear();
		for (auto& entry : a_snapshot.entries) {
			entry.text = Text::Translate(entry.rawText);
			if (entry.text.empty()) {
				entry.text = entry.rawText;
			}
			Derive(entry);

			// tintType/tintIndex disambiguate paint slots, which all share the
			// text "default" until a texture is chosen
			auto key = std::format("{}|{}|{}|{}|{}|{}", entry.type, entry.sliderID, entry.callback, entry.rawText, entry.tintType, entry.tintIndex);
			if (const auto n = seen[key]++; n > 0) {
				key += std::format("#{}", n);
			}
			entry.key = std::move(key);

			if (const auto it = previous.find(entry.key); it != previous.end()) {
				const auto& old = it->second;
				entry.initialPosition = old.initialPosition;
				entry.initialFill = old.initialFill;
				entry.initialTexture = old.initialTexture;
				if (now - old.localEditTime < kLocalEditHold) {
					entry.position = old.position;
					entry.fillColor = old.fillColor;
					entry.texture = old.texture;
					entry.localEditTime = old.localEditTime;
				}
			} else {
				entry.initialPosition = entry.position;
				entry.initialFill = entry.fillColor;
				entry.initialTexture = entry.texture;
			}
		}

		entries = std::move(a_snapshot.entries);
		for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
			byKey[entries[i].key] = i;
		}

		categories.clear();
		for (auto& category : a_snapshot.categories) {
			category.label = Text::Translate(category.raw);
			categories.push_back(std::move(category));
		}

		for (std::size_t i = 0; i < makeup.size(); ++i) {
			for (auto& tex : a_snapshot.makeup[i]) {
				tex.label = tex.text.empty() ? Text::FileStem(tex.texture) : Text::Translate(tex.text);
				tex.search = Text::Lower(tex.label + " " + tex.texture);
			}
			makeup[i] = std::move(a_snapshot.makeup[i]);
		}

		hasData = true;
		++dataEpoch;
	}

	void Model::ApplyPlayer(PlayerInfo&& a_player)
	{
		player = std::move(a_player);
	}

	void Model::ResetSession()
	{
		categories.clear();
		entries.clear();
		for (auto& list : makeup) {
			list.clear();
		}
		byKey.clear();
		hasData = false;
		++dataEpoch;
		past.clear();
		future.clear();
		presetFiles.clear();
		presetsLoaded = false;
		presetInfo.clear();
		partNames.clear();
		headParts.clear();
		headPartsLoaded = false;
		headPartSlider = -1.0;
		frozen = false;
		undressed = false;
		zoomFace = true;
		lightOn = true;
		yieldCount = 0;
		status.clear();
	}

	void Model::SetStatus(std::string a_text)
	{
		status = std::move(a_text);
		statusTime = Text::Now();
	}

	Entry* Model::Find(const std::string& a_key)
	{
		const auto it = byKey.find(a_key);
		return it == byKey.end() ? nullptr : &entries[it->second];
	}

	const Entry* Model::FindSlider(double a_sliderID) const
	{
		for (const auto& entry : entries) {
			if (entry.type == Defs::kTypeSlider && entry.sliderID == a_sliderID) {
				return &entry;
			}
		}
		return nullptr;
	}

	int Model::TintIndexFor(const Entry& a_entry) const
	{
		if (a_entry.type >= Defs::kTypeWarPaint) {
			return a_entry.tintIndex;
		}
		// colour sliders target the preset picked by their static slider
		const auto bySlider = [&](double a_id) {
			const auto* entry = FindSlider(a_id);
			return entry ? static_cast<int>(std::lround(entry->position)) : 0;
		};
		if (a_entry.tintType == Defs::kTintWarpaint) {
			return bySlider(7.0);
		}
		if (a_entry.tintType == Defs::kTintDirt) {
			return bySlider(4.0);
		}
		return 0;
	}

	bool Model::IsCurrentRace(const Entry& a_entry) const
	{
		return !player.race.empty() && (a_entry.label == player.race || a_entry.text == player.race);
	}

	void Model::SetValue(Entry& a_entry, double a_value, bool a_send)
	{
		if (!std::isfinite(a_value)) {
			return;
		}
		a_entry.position = a_value;
		a_entry.localEditTime = Text::Now();
		++valueEpoch;
		if (a_send) {
			Bridge::SetValue(a_entry.Ref(), a_value);
			if (a_entry.callback == "ChangeHeadPart" || a_entry.callback == "ChangeDoubleMorph") {
				Bridge::QuerySliderInfo(a_entry.sliderID, a_value);
			}
		}
	}

	void Model::SetColor(Entry& a_entry, std::uint32_t a_argb, bool a_send)
	{
		a_entry.fillColor = a_argb;
		a_entry.localEditTime = Text::Now();
		++valueEpoch;
		if (!a_send) {
			return;
		}
		const int  index = TintIndexFor(a_entry);
		const auto kind = a_entry.tintType == Defs::kTintHair ? Bridge::ColorKind::Hair :
		                  IsOverlayTint(a_entry.tintType)      ? Bridge::ColorKind::Overlay :
		                                                         Bridge::ColorKind::Tint;
		Bridge::SetColor(a_entry.Ref(), kind, a_entry.tintType * 1000 + index, a_argb);
	}

	void Model::SetTexture(Entry& a_entry, const std::string& a_texture)
	{
		a_entry.texture = a_texture;
		a_entry.localEditTime = Text::Now();
		++valueEpoch;
		Bridge::SetTexture(a_entry.Ref(), IsOverlayTint(a_entry.tintType), a_entry.tintType * 1000 + a_entry.tintIndex, a_texture);
	}

	void Model::ChangeRace(const Entry& a_entry)
	{
		Bridge::ChangeRace(a_entry.raceID);
		SetStatus("Changing race to " + a_entry.label + "...");
	}

	void Model::Record(HistoryItem a_item)
	{
		if (a_item.empty()) {
			return;
		}
		past.push_back(std::move(a_item));
		while (past.size() > kHistoryCap) {
			past.pop_front();
		}
		future.clear();
	}

	void Model::Apply(const HistoryStep& a_step, bool a_forward)
	{
		auto* entry = Find(a_step.key);
		if (!entry) {
			return;
		}
		switch (a_step.kind) {
		case HistoryStep::Kind::Value:
			SetValue(*entry, a_forward ? a_step.to : a_step.from);
			break;
		case HistoryStep::Kind::Color:
			SetColor(*entry, a_forward ? a_step.toColor : a_step.fromColor);
			break;
		case HistoryStep::Kind::Texture:
			SetTexture(*entry, a_forward ? a_step.toTexture : a_step.fromTexture);
			break;
		}
	}

	void Model::Undo()
	{
		if (past.empty()) {
			return;
		}
		auto item = std::move(past.back());
		past.pop_back();
		for (auto it = item.rbegin(); it != item.rend(); ++it) {
			Apply(*it, false);
		}
		future.push_back(std::move(item));
	}

	void Model::Redo()
	{
		if (future.empty()) {
			return;
		}
		auto item = std::move(future.back());
		future.pop_back();
		for (const auto& step : item) {
			Apply(step, true);
		}
		past.push_back(std::move(item));
	}

	void Model::ResetEntry(Entry& a_entry)
	{
		HistoryItem item;
		if (a_entry.control != Control::Paint && a_entry.control != Control::Race && std::abs(a_entry.position - a_entry.initialPosition) > 1e-9) {
			item.push_back({ .key = a_entry.key, .kind = HistoryStep::Kind::Value, .from = a_entry.position, .to = a_entry.initialPosition });
		}
		if ((a_entry.hasColor || a_entry.control == Control::Paint) && a_entry.fillColor != a_entry.initialFill) {
			item.push_back({ .key = a_entry.key, .kind = HistoryStep::Kind::Color, .fromColor = a_entry.fillColor, .toColor = a_entry.initialFill });
		}
		if (a_entry.control == Control::Paint && a_entry.texture != a_entry.initialTexture && !a_entry.initialTexture.empty()) {
			item.push_back({ .key = a_entry.key, .kind = HistoryStep::Kind::Texture, .fromTexture = a_entry.texture, .toTexture = a_entry.initialTexture });
		}
		for (const auto& step : item) {
			Apply(step, true);
		}
		Record(std::move(item));
	}

	void Model::ResetAll()
	{
		// sex is left alone: flipping it rebuilds the whole character
		HistoryItem item;
		for (auto& entry : entries) {
			if (!entry.enabled || entry.control == Control::Sex || entry.control == Control::Race) {
				continue;
			}
			if (entry.control != Control::Paint && std::abs(entry.position - entry.initialPosition) > 1e-9) {
				item.push_back({ .key = entry.key, .kind = HistoryStep::Kind::Value, .from = entry.position, .to = entry.initialPosition });
			}
			if ((entry.hasColor || entry.control == Control::Paint) && entry.fillColor != entry.initialFill) {
				item.push_back({ .key = entry.key, .kind = HistoryStep::Kind::Color, .fromColor = entry.fillColor, .toColor = entry.initialFill });
			}
			if (entry.control == Control::Paint && entry.texture != entry.initialTexture && !entry.initialTexture.empty()) {
				item.push_back({ .key = entry.key, .kind = HistoryStep::Kind::Texture, .fromTexture = entry.texture, .toTexture = entry.initialTexture });
			}
		}
		for (const auto& step : item) {
			Apply(step, true);
		}
		Record(std::move(item));
	}
}
