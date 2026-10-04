#include "UI/UI.h"

#include "Bridge.h"
#include "Model.h"
#include "Preview.h"
#include "Settings.h"
#include "UI/Theme.h"
#include "UI/Widgets.h"

namespace RMA::UI
{
	namespace
	{
		namespace im = ImGuiMCP;
		namespace dl = ImGuiMCP::ImDrawListManager;
		namespace W = Widgets;

		constexpr auto kEditorWindow = "RaceMenu Atelier##RMA.Editor";
		constexpr auto kSideWindow = "Scene##RMA.Side";
		constexpr auto kSideTabWindow = "SceneTab##RMA.SideTab";

		constexpr std::array<std::pair<const char*, const char*>, 8> kPoses{ {
			{ "IdleWave", "Wave" },
			{ "IdleApplaud2", "Applaud" },
			{ "IdleLaugh", "Laugh" },
			{ "IdleCower", "Cower" },
			{ "IdleCiceroDance1", "Dance" },
			{ "IdleHandsBehindBack", "Hands behind back" },
			{ "IdleGetAttention", "Get attention" },
			{ "IdleBook_Reading", "Read a book" },
		} };

		// RaceMenu's colour palette plus skin and hair tones
		constexpr std::array<std::uint32_t, 16> kPalette{
			0x1A1A1A, 0x4A4A4A, 0x8A8A8A, 0xE8E8E8,
			0xF5D7B8, 0xD9A678, 0xA5673F, 0x6B3E26,
			0x241A10, 0x4A2F1B, 0x7A4B22, 0xB58143,
			0xB02A2A, 0xC9A227, 0x2F8A4A, 0x2A6DB0
		};

		struct Item
		{
			enum class Kind
			{
				Header,
				Row,
				Races,
				Choices  // another mod's list (Apprentice's classes or traits), as tiles of its own
			};

			Kind        kind{ Kind::Row };
			int         entry{ -1 };
			std::string group;
			bool        shortLabel{ false };
			ImU32       hue{ Color::Hues[0] };
			int         stripe{ 0 };  // alternates the row tone inside a run
			std::vector<std::size_t> members;  // Choices: the entries, in list order
		};

		struct State
		{
			SKSEMenuFramework::Model::WindowInterface* window{ nullptr };
			bool                                       registered{ false };
			bool                                       session{ false };

			// lock compensation (see CompensateLock)
			bool wasLocked{ false };
			bool blurTaken{ false };

			// filtering
			char        search[128]{};
			bool        focusSearch{ false };
			std::string categoryRaw;  // "" = all
			std::uint32_t categoryFlag{ 0 };
			bool        changedOnly{ false };
			bool        scrollTop{ false };
			bool        categoryChosen{ false };

			std::vector<Item>        items;
			std::vector<int>         categoryChanged;  // per category: changed entries
			std::vector<int>         categoryCount;
			int                      visibleRows{ 0 };
			int                      changedTotal{ 0 };
			std::unordered_set<double> partRequested;

			// value typing
			std::string editKey;
			double      editValue{ 0.0 };
			int         editGrace{ 0 };  // frames before losing focus commits

			// slider drags
			std::string dragKey;
			double      dragFrom{ 0.0 };
			double      dragLastSend{ 0.0 };

			// popups
			std::string contextKey;
			bool        openContext{ false };
			std::string colorKey;
			std::uint32_t colorFrom{ 0 };
			float       colorEdit[4]{};
			double      colorLastSend{ 0.0 };
			bool        colorPending{ false };
			bool        openColor{ false };
			std::string textureKey;
			char        textureSearch[96]{};  // kept between openings
			bool        openTexture{ false };
			std::string textureFrom;          // texture when the picker opened
			bool        textureScroll{ false };
			float       editorRight{ 0.0f };
			std::string headPartKey;
			char        headPartSearch[96]{};
			std::string headPartPlugin;
			bool        openHeadParts{ false };
			bool        openDone{ false };
			bool        openResetAll{ false };
			std::string loadPreset;
			bool        openLoadPreset{ false };

			// character
			char        name[64]{};
			bool        nameEdited{ false };
			std::string generatedName;
			int         nameSex{ -2 };

			// side panel
			bool        sceneOpen{ true };
			bool        presetsOpen{ false };
			bool        bodySlideOpen{ false };
			bool        toolsOpen{ false };
			char        presetName[80]{};
			std::string presetInfoOpen;
			char        bodySlideName[80]{};
			bool        bodySlideHimbo{ false };

			// scene
			bool rotating{ false };
		};

		State s;

		// ------------------------------------------------------------------
		// helpers

		bool IsAllCategory(const Category& a_category)
		{
			const auto lower = Text::Lower(a_category.label);
			return a_category.raw.empty() || a_category.flag == Defs::kCategoryAll || lower == "all" || lower == "$all";
		}

		// CategoryFilter.entryMatchesFilter: an entry without filterFlag matches
		// every category; otherwise by flag, then by text filter
		bool MatchesCategory(const Entry& a_entry, const Category& a_category)
		{
			if (a_entry.filterFlag < 0) {
				return true;
			}
			if (a_category.flag && (static_cast<std::uint64_t>(a_entry.filterFlag) & a_category.flag) != 0) {
				return true;
			}
			if (!a_category.textFilter.empty()) {
				return std::ranges::find(a_entry.textFilters, a_category.textFilter) != a_entry.textFilters.end();
			}
			return false;
		}

		bool ContainsWord(const std::string& a_text, std::string_view a_word)
		{
			std::size_t at = 0;
			while ((at = a_text.find(a_word, at)) != std::string::npos) {
				const bool left = at == 0 || !std::isalnum(static_cast<unsigned char>(a_text[at - 1]));
				const auto end = at + a_word.size();
				const bool right = end >= a_text.size() || !std::isalnum(static_cast<unsigned char>(a_text[end]));
				if (left && right) {
					return true;
				}
				at = end;
			}
			return false;
		}

		// keyword heuristic: hides sliders that clearly belong to the other body type
		bool SexFiltered(const Entry& a_entry, const Model& a_model)
		{
			if (!Settings::Get().sexFilter || a_entry.type != Defs::kTypeSlider) {
				return false;
			}
			const auto& t = a_entry.search;
			if (a_model.player.sex == 1 && ContainsWord(t, "himbo")) {
				return true;
			}
			if (a_model.player.sex == 0) {
				for (const auto word : { "3ba"sv, "cbbe"sv, "uunp"sv, "bhunp"sv, "tbd"sv }) {
					if (ContainsWord(t, word)) {
						return true;
					}
				}
			}
			// body morph sliders by the category key their script registered them
			// under: UBE morphs only exist on UBE races, CBBE 3BA / HIMBO only off them
			const bool ubeRace = Text::Lower(a_model.player.race).find("ube") != std::string::npos;
			for (const auto& filter : a_entry.textFilters) {
				const auto key = Text::Lower(filter);
				if (key.starts_with("rsm_bodymorph_ube")) {
					return !ubeRace;
				}
				if (key == "rsm_bodymorph_cbbe" || key == "changemorphhimbo") {
					return ubeRace;
				}
			}
			return !ubeRace && ContainsWord(t, "ube");
		}

		const char* CategoryIcon(const Category& a_category)
		{
			switch (a_category.flag) {
			case 2:
				return Icon::Users;
			case 4:
				return Icon::Person;
			case 8:
				return Icon::User;
			case 16:
				return Icon::Face;
			case 32:
				return Icon::Eye;
			case 64:
				return Icon::Brow;
			case 128:
				return Icon::Mouth;
			case 256:
				return Icon::Scissors;
			case 8192:
				return Icon::Palette;
			case 16384:
				return Icon::Brush;
			case 32768:
				return Icon::SprayCan;
			case 65536:
				return Icon::Hand;
			case 131072:
				return Icon::Feet;
			case 262144:
				return Icon::Mask;
			default:
				break;
			}
			const auto t = Text::Lower(a_category.label + " " + a_category.raw);
			if (t.find("body") != std::string::npos || t.find("cbbe") != std::string::npos || t.find("himbo") != std::string::npos || t.find("morph") != std::string::npos) {
				return Icon::Person;
			}
			if (t.find("hair") != std::string::npos) {
				return Icon::Scissors;
			}
			if (t.find("eye") != std::string::npos) {
				return Icon::Eye;
			}
			if (t.find("paint") != std::string::npos || t.find("makeup") != std::string::npos || t.find("tint") != std::string::npos) {
				return Icon::Palette;
			}
			if (t.find("overlay") != std::string::npos || t.find("tattoo") != std::string::npos) {
				return Icon::Layers;
			}
			return Icon::Sliders;
		}

		ImU32 CategoryHue(int a_index)
		{
			return a_index < 0 ? Color::Gold : Color::Hues[static_cast<std::size_t>(a_index) % Color::Hues.size()];
		}

		ImU32 GroupHue(const std::string& a_group)
		{
			return Color::Hues[std::hash<std::string>{}(Text::Lower(a_group)) % Color::Hues.size()];
		}

		std::string FormatValue(double a_value, double a_interval)
		{
			if (a_interval >= 1.0) {
				return std::format("{}", static_cast<long long>(std::llround(a_value)));
			}
			if (a_interval > 0.0 && a_interval < 0.01) {
				return std::format("{:.3f}", a_value);
			}
			return std::format("{:.2f}", a_value);
		}

		double Snap(const Entry& a_entry, double a_value)
		{
			const double step = a_entry.interval > 0.0 ? a_entry.interval : 0.01;
			const double snapped = a_entry.min + std::round((a_value - a_entry.min) / step) * step;
			return std::round(snapped * 1.0e6) / 1.0e6;
		}

		std::string KeyName(std::uint32_t a_scanCode)
		{
			if (a_scanCode >= 0x3B && a_scanCode <= 0x44) {
				return std::format("F{}", a_scanCode - 0x3A);
			}
			if (a_scanCode == 0x57) {
				return "F11";
			}
			if (a_scanCode == 0x58) {
				return "F12";
			}
			return std::format("key 0x{:X}", a_scanCode);
		}

		const char* DisplayLabel(const Entry& a_entry, bool a_short)
		{
			if (Settings::Get().showTechnicalNames) {
				return a_entry.rawText.c_str();
			}
			return a_short && !a_entry.shortLabel.empty() ? a_entry.shortLabel.c_str() : a_entry.label.c_str();
		}

		bool AnyPopupOpen()
		{
			return im::IsPopupOpen("", ImGuiMCP::ImGuiPopupFlags_AnyPopupId | ImGuiMCP::ImGuiPopupFlags_AnyPopupLevel);
		}

		void PushHistoryValue(Model& a_model, const Entry& a_entry, double a_from, double a_to)
		{
			if (std::abs(a_from - a_to) > 1e-9) {
				a_model.Record({ HistoryStep{ .key = a_entry.key, .kind = HistoryStep::Kind::Value, .from = a_from, .to = a_to } });
			}
		}

		// one undoable step, sent at once
		void CommitValue(Model& a_model, Entry& a_entry, double a_value)
		{
			const double from = a_entry.position;
			a_model.SetValue(a_entry, a_value);
			PushHistoryValue(a_model, a_entry, from, a_value);
		}

		// ------------------------------------------------------------------
		// SKSE Menu Framework pauses and blurs the game while a window takes
		// the input (when the user enabled that in its settings). Character
		// creation needs a live, sharp preview, so both are undone while the
		// editor is open and the blur reference is handed back on close.

		void CompensateLock()
		{
			const bool locked = SKSEMenuFramework::IsAnyBlockingWindowOpened();
			if (locked && !s.wasLocked) {
				if (const auto blur = RE::UIBlurManager::GetSingleton(); blur && blur->blurCount > 0) {
					blur->DecrementBlurCount();
					s.blurTaken = true;
				}
				if (const auto main = RE::Main::GetSingleton()) {
					main->GetRuntimeData().freezeTime = false;
				}
			}
			s.wasLocked = locked;
		}

		void ReleaseLock()
		{
			if (s.blurTaken) {
				if (const auto blur = RE::UIBlurManager::GetSingleton()) {
					blur->IncrementBlurCount();
				}
				s.blurTaken = false;
			}
			s.wasLocked = false;
		}

		// ------------------------------------------------------------------
		// session

		void StartSession()
		{
			s.session = true;
			s.search[0] = '\0';
			s.changedOnly = false;
			s.editKey.clear();
			s.dragKey.clear();
			s.colorKey.clear();
			s.textureKey.clear();
			s.headPartKey.clear();
			s.presetInfoOpen.clear();
			s.partRequested.clear();
			s.name[0] = '\0';
			s.nameEdited = false;
			s.generatedName.clear();
			s.nameSex = -2;
			s.categoryChosen = false;
			s.rotating = false;
		}

		void EndSession()
		{
			s.session = false;
			ReleaseLock();
			Preview::Clear();
		}

		void SyncName(const Model& a_model)
		{
			if (s.nameEdited || a_model.player.name.empty()) {
				return;
			}
			std::string name = a_model.player.name;
			if (Text::Lower(name) == "prisoner") {
				if (s.generatedName.empty() || s.nameSex != a_model.player.sex) {
					static constexpr std::array female{ "Aela", "Elisif", "Freya", "Ilyra", "Runa", "Sigrid", "Talia", "Veyra" };
					static constexpr std::array male{ "Aren", "Corvin", "Darian", "Hakon", "Kael", "Rurik", "Soren", "Tavren" };
					static std::mt19937 rng{ std::random_device{}() };
					const auto& pool = a_model.player.sex == 1 ? female : male;
					s.generatedName = pool[std::uniform_int_distribution<std::size_t>(0, pool.size() - 1)(rng)];
				}
				name = s.generatedName;
			}
			s.nameSex = a_model.player.sex;
			if (name != s.name) {
				strncpy_s(s.name, name.c_str(), _TRUNCATE);
			}
		}

		// ------------------------------------------------------------------
		// filtering

		int ActiveCategory(const Model& a_model)
		{
			if (s.categoryRaw.empty()) {
				return -1;
			}
			for (int i = 0; i < static_cast<int>(a_model.categories.size()); ++i) {
				const auto& c = a_model.categories[i];
				if (c.raw == s.categoryRaw && c.flag == s.categoryFlag) {
					return i;
				}
			}
			return -1;
		}

		void SelectCategory(const Category* a_category)
		{
			s.categoryRaw = a_category ? a_category->raw : std::string{};
			s.categoryFlag = a_category ? a_category->flag : 0;
			s.scrollTop = true;
			s.categoryChosen = true;
		}

		void BuildItems(Model& a_model)
		{
			s.items.clear();
			s.visibleRows = 0;
			s.changedTotal = 0;
			s.categoryChanged.assign(a_model.categories.size(), 0);
			s.categoryCount.assign(a_model.categories.size(), 0);

			if (!s.categoryChosen && !a_model.categories.empty()) {
				// start on the first real category, like RaceMenu
				for (const auto& c : a_model.categories) {
					if (!IsAllCategory(c)) {
						SelectCategory(&c);
						break;
					}
				}
				s.scrollTop = true;
			}

			const int   active = ActiveCategory(a_model);
			const auto  query = Text::Lower(s.search);
			const bool  searching = !query.empty();
			std::vector<int> visible;
			visible.reserve(a_model.entries.size());

			for (int i = 0; i < static_cast<int>(a_model.entries.size()); ++i) {
				const auto& e = a_model.entries[i];
				const bool  filteredBySex = (e.control == Control::Slider || e.control == Control::Stepper) && SexFiltered(e, a_model);
				const bool  changed = e.IsChanged();
				if (changed) {
					++s.changedTotal;
				}
				for (std::size_t c = 0; c < a_model.categories.size(); ++c) {
					if (!filteredBySex && MatchesCategory(e, a_model.categories[c])) {
						++s.categoryCount[c];
						if (changed) {
							++s.categoryChanged[c];
						}
					}
				}
				if (filteredBySex) {
					continue;
				}
				// a search looks through every category
				if (searching) {
					if (e.search.find(query) == std::string::npos) {
						continue;
					}
				} else if (active >= 0 && !MatchesCategory(e, a_model.categories[active])) {
					continue;
				}
				if (s.changedOnly && !changed) {
					continue;
				}
				visible.push_back(i);
			}

			bool racesPlaced = false;
			int  choiceItem = -1;
			for (std::size_t v = 0; v < visible.size(); ++v) {
				const auto& e = a_model.entries[visible[v]];
				if (e.control == Control::Choice) {
					if (choiceItem < 0) {
						choiceItem = static_cast<int>(s.items.size());
						s.items.push_back({ Item::Kind::Choices });
					}
					s.items[choiceItem].members.push_back(visible[v]);
					continue;
				}
				if (e.control == Control::Race) {
					if (!racesPlaced) {
						s.items.push_back({ Item::Kind::Races });
						racesPlaced = true;
					}
					continue;
				}
				++s.visibleRows;
				const bool groupable = Settings::Get().groupHeaders && !e.group.empty() && (e.control == Control::Slider || e.control == Control::Stepper);
				if (!groupable) {
					s.items.push_back({ Item::Kind::Row, visible[v] });
					continue;
				}
				// a header for runs of two or more rows of the same group
				const bool startsRun = s.items.empty() || s.items.back().kind != Item::Kind::Row || s.items.back().group != e.group;
				if (startsRun) {
					std::size_t run = 1;
					while (v + run < visible.size()) {
						const auto& next = a_model.entries[visible[v + run]];
						if (next.group != e.group || next.control == Control::Race || next.control == Control::Choice) {
							break;
						}
						++run;
					}
					if (run < 2) {
						s.items.push_back({ Item::Kind::Row, visible[v] });
						continue;
					}
					s.items.push_back({ Item::Kind::Header, -1, e.group });
				}
				s.items.push_back({ Item::Kind::Row, visible[v], e.group, true });
			}

			// colour: the slider family, else the first category the entry belongs to
			int stripe = 0;
			for (auto& item : s.items) {
				if (item.kind == Item::Kind::Header) {
					item.hue = GroupHue(item.group);
					stripe = 0;
					continue;
				}
				if (item.kind != Item::Kind::Row) {
					continue;
				}
				const auto& e = a_model.entries[item.entry];
				item.stripe = stripe++;
				if (item.shortLabel) {
					item.hue = GroupHue(item.group);
					continue;
				}
				int primary = -1;
				for (int c = 0; c < static_cast<int>(a_model.categories.size()) && e.filterFlag >= 0; ++c) {
					const auto& cat = a_model.categories[c];
					if (!IsAllCategory(cat) && cat.flag && (static_cast<std::uint64_t>(e.filterFlag) & cat.flag)) {
						primary = c;
						break;
					}
				}
				item.hue = CategoryHue(primary >= 0 ? primary : active);
			}
		}

		// ------------------------------------------------------------------
		// rows

		struct RowGeometry
		{
			float u, padX, padY, line, gap, ctrl, height;
		};

		RowGeometry Geometry()
		{
			const float u = U();
			RowGeometry g{};
			g.u = u;
			g.padX = u * 0.6f;
			g.padY = u * 0.3f;
			g.line = im::GetTextLineHeight();
			g.gap = u * 0.2f;
			g.ctrl = std::floor(u * 1.35f);
			g.height = std::ceil(g.padY * 2.0f + g.line + g.gap + g.ctrl);
			return g;
		}

		void RequestPartName(Model& a_model, const Entry& a_entry)
		{
			if (a_entry.callback != "ChangeHeadPart" || a_entry.sliderID < 0.0) {
				return;
			}
			if (!a_model.partNames.contains(a_entry.sliderID) && s.partRequested.insert(a_entry.sliderID).second) {
				Bridge::QuerySliderInfo(a_entry.sliderID, a_entry.position);
			}
		}

		void OpenHeadPartBrowser(const Entry& a_entry)
		{
			s.headPartKey = a_entry.key;
			s.headPartSearch[0] = '\0';
			s.headPartPlugin.clear();
			s.openHeadParts = true;
			Bridge::QueryHeadParts(a_entry.sliderID);
		}

		// label line: name, "custom" tag, optional right-hand widgets drawn by the caller
		void DrawLabel(ImGuiMCP::ImDrawList* a_list, const Entry& a_entry, const Item& a_item, ImVec2 a_pos, float a_maxWidth, bool a_changed)
		{
			const auto  text = W::Ellipsize(DisplayLabel(a_entry, a_item.shortLabel), a_maxWidth);
			const auto  size = im::CalcTextSize(text.c_str());
			dl::AddText(a_list, a_pos, a_changed ? Color::GoldBright : Color::Text, text.c_str());
			if (a_entry.custom && size.x + U() * 3.5f < a_maxWidth && !Settings::Get().showTechnicalNames) {
				W::SmallText(a_list, a_pos + ImVec2{ size.x + U() * 0.45f, U() * 0.14f }, Color::TextFaint, "custom", 0.72f);
			}
		}

		void RowSlider(Model& a_model, Entry& a_entry, const Item& a_item, const RowGeometry& g, ImVec2 a_pos, float a_width, bool a_changed)
		{
			auto*       list = im::GetWindowDrawList();
			const float right = a_pos.x + a_width - g.padX;
			const float lineY = a_pos.y + g.padY;
			float       labelRight = right;

			// value (click to type it)
			const bool editing = s.editKey == a_entry.key;
			const auto valueText = FormatValue(a_entry.position, a_entry.interval);
			const auto valueSize = im::CalcTextSize(valueText.c_str());
			const float valueW = editing ? g.u * 5.0f : valueSize.x;
			const float valueX = right - valueW;
			labelRight = valueX - g.u * 0.5f;
			if (editing) {
				im::SetCursorScreenPos({ valueX, lineY - (im::GetFrameHeight() - g.line) * 0.5f });
				im::SetNextItemWidth(valueW);
				if (s.editGrace > 0) {
					im::SetKeyboardFocusHere();
				}
				const bool entered = im::InputDouble("##value", &s.editValue, 0.0, 0.0, "%.4f",
					ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue | ImGuiMCP::ImGuiInputTextFlags_AutoSelectAll);
				const bool active = im::IsItemActive();
				const bool cancel = active && im::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false);
				if (active) {
					s.editGrace = 0;
				} else if (s.editGrace > 0) {
					--s.editGrace;
				}
				const bool blurred = !active && s.editGrace == 0;
				if (cancel) {
					s.editKey.clear();
				} else if (entered || blurred) {
					double v = s.editValue;
					const bool overdrive = Settings::Get().overdrive && a_entry.canOverdrive;
					if (!overdrive) {
						v = std::clamp(v, a_entry.min, a_entry.max);
					}
					if (std::isfinite(v) && std::abs(v - a_entry.position) > 1e-9) {
						CommitValue(a_model, a_entry, v);
					}
					s.editKey.clear();
				}
			} else {
				im::SetCursorScreenPos({ valueX, lineY });
				if (im::InvisibleButton("##valuebtn", { valueW, g.line })) {
					s.editKey = a_entry.key;
					s.editValue = a_entry.position;
					s.editGrace = 4;
				}
				const bool hovered = im::IsItemHovered();
				const bool outside = a_entry.position < a_entry.min - 1e-9 || a_entry.position > a_entry.max + 1e-9;
				dl::AddText(list, { valueX, lineY }, outside ? Color::Danger : hovered ? Color::GoldBright : a_changed ? Color::Gold : Color::Value, valueText.c_str());
				if (hovered) {
					W::Tooltip(a_entry.canOverdrive && Settings::Get().overdrive ? "Click to type a value (overdrive: may exceed the range)" : "Click to type a value");
				}
			}

			// colour swatch of colour sliders
			if (a_entry.hasColor) {
				const ImVec2 sw{ g.u * 1.7f, g.line * 0.8f };
				labelRight -= sw.x + g.u * 0.4f;
				im::SetCursorScreenPos({ labelRight + g.u * 0.4f, lineY + (g.line - sw.y) * 0.5f });
				if (W::Swatch("##swatch", a_entry.fillColor, sw, "Edit colour")) {
					s.colorKey = a_entry.key;
					s.openColor = true;
				}
			}

			// head part sliders can be searched by name
			if (a_entry.callback == "ChangeHeadPart") {
				const float bw = g.line;
				labelRight -= bw + g.u * 0.3f;
				im::SetCursorScreenPos({ labelRight + g.u * 0.3f, lineY });
				if (W::IconButton("##browse", Icon::Search, "Browse every available part", false, bw)) {
					OpenHeadPartBrowser(a_entry);
				}
			}

			DrawLabel(list, a_entry, a_item, { a_pos.x + g.padX, lineY }, labelRight - (a_pos.x + g.padX), a_changed);

			// step buttons and track
			const float ctrlY = lineY + g.line + g.gap;
			im::SetCursorScreenPos({ a_pos.x + g.padX, ctrlY });
			const double step = a_entry.interval > 0.0 ? a_entry.interval : 0.01;
			if (W::IconButton("##down", Icon::ChevronLeft, nullptr, false, g.ctrl)) {
				const double v = std::clamp(Snap(a_entry, a_entry.position - step), a_entry.min, a_entry.max);
				CommitValue(a_model, a_entry, v);
			}
			const float trackX = a_pos.x + g.padX + g.ctrl + g.u * 0.3f;
			const float trackW = right - g.ctrl - g.u * 0.3f - trackX;
			im::SetCursorScreenPos({ trackX, ctrlY });
			double     value = a_entry.position;
			const auto st = W::ValueTrack("##track", value, a_entry.min, a_entry.max, a_entry.interval, a_entry.initialPosition, { trackW, g.ctrl }, a_item.hue);
			if (st.activated) {
				s.dragKey = a_entry.key;
				s.dragFrom = a_entry.position;
				s.dragLastSend = 0.0;
			}
			if (st.changed) {
				// throttle the game calls while dragging; the last value always goes out
				const double now = Text::Now();
				const bool   send = now - s.dragLastSend > 0.05;
				if (send) {
					s.dragLastSend = now;
				}
				a_model.SetValue(a_entry, value, send);
			}
			if (st.deactivated && s.dragKey == a_entry.key) {
				a_model.SetValue(a_entry, a_entry.position, true);
				PushHistoryValue(a_model, a_entry, s.dragFrom, a_entry.position);
				s.dragKey.clear();
			}
			im::SetCursorScreenPos({ right - g.ctrl, ctrlY });
			if (W::IconButton("##up", Icon::ChevronRight, nullptr, false, g.ctrl)) {
				const double v = std::clamp(Snap(a_entry, a_entry.position + step), a_entry.min, a_entry.max);
				CommitValue(a_model, a_entry, v);
			}
		}

		void RowStepper(Model& a_model, Entry& a_entry, const Item& a_item, const RowGeometry& g, ImVec2 a_pos, float a_width, bool a_changed)
		{
			auto*       list = im::GetWindowDrawList();
			const float right = a_pos.x + a_width - g.padX;
			const float lineY = a_pos.y + g.padY;
			const bool  headPart = a_entry.callback == "ChangeHeadPart";
			float       labelRight = right;

			if (headPart) {
				RequestPartName(a_model, a_entry);
				const float bw = g.line;
				labelRight -= bw;
				im::SetCursorScreenPos({ labelRight, lineY });
				if (W::IconButton("##browse", Icon::Search, "Browse every available part", false, bw)) {
					OpenHeadPartBrowser(a_entry);
				}
				labelRight -= g.u * 0.4f;
			}
			DrawLabel(list, a_entry, a_item, { a_pos.x + g.padX, lineY }, labelRight - (a_pos.x + g.padX), a_changed);

			const float ctrlY = lineY + g.line + g.gap;
			const int   minV = static_cast<int>(std::lround(a_entry.min));
			const int   maxV = static_cast<int>(std::lround(a_entry.max));
			const int   cur = static_cast<int>(std::lround(a_entry.position));

			im::SetCursorScreenPos({ a_pos.x + g.padX, ctrlY });
			im::BeginDisabled(cur <= minV);
			if (W::IconButton("##prev", Icon::ChevronLeft, nullptr, false, g.ctrl)) {
				CommitValue(a_model, a_entry, static_cast<double>(std::max(minV, cur - 1)));
			}
			im::EndDisabled();

			const float boxX = a_pos.x + g.padX + g.ctrl + g.u * 0.3f;
			const float boxW = right - g.ctrl - g.u * 0.3f - boxX;
			im::SetCursorScreenPos({ boxX, ctrlY });
			const bool clicked = im::InvisibleButton("##readout", { boxW, g.ctrl });
			const bool hovered = im::IsItemHovered();
			dl::AddRectFilled(list, { boxX, ctrlY }, { boxX + boxW, ctrlY + g.ctrl }, hovered && headPart ? Color::FrameHover : Color::Track, Round(g.u * 0.25f), 0);

			const auto counter = std::format("{} / {}", cur, maxV);
			const auto counterSize = W::SmallTextSize(counter.c_str());
			W::SmallText(list, { boxX + boxW - counterSize.x - g.u * 0.5f, ctrlY + (g.ctrl - counterSize.y) * 0.5f }, Color::TextFaint, counter.c_str());

			std::string main;
			if (headPart) {
				if (const auto it = a_model.partNames.find(a_entry.sliderID); it != a_model.partNames.end() && !it->second.empty()) {
					main = Text::Humanise(it->second, true);
				}
			}
			if (main.empty()) {
				main = std::format("Option {}", cur);
			}
			main = W::Ellipsize(main, boxW - counterSize.x - g.u * 1.5f);
			const auto mainSize = im::CalcTextSize(main.c_str());
			dl::AddText(list, { boxX + g.u * 0.6f, ctrlY + (g.ctrl - mainSize.y) * 0.5f }, a_changed ? Color::GoldBright : Color::Text, main.c_str());
			if (clicked && headPart) {
				OpenHeadPartBrowser(a_entry);
			}
			if (hovered && headPart) {
				W::Tooltip("Click to browse every available part");
			}

			im::SetCursorScreenPos({ right - g.ctrl, ctrlY });
			im::BeginDisabled(cur >= maxV);
			if (W::IconButton("##next", Icon::ChevronRight, nullptr, false, g.ctrl)) {
				CommitValue(a_model, a_entry, static_cast<double>(std::min(maxV, cur + 1)));
			}
			im::EndDisabled();
		}

		void RowSex(Model& a_model, Entry& a_entry, const Item& a_item, const RowGeometry& g, ImVec2 a_pos, float a_width, bool a_changed)
		{
			auto*       list = im::GetWindowDrawList();
			const float lineY = a_pos.y + g.padY;
			DrawLabel(list, a_entry, a_item, { a_pos.x + g.padX, lineY }, a_width - g.padX * 2.0f, a_changed);
			const float ctrlY = lineY + g.line + g.gap;
			const float half = (a_width - g.padX * 2.0f - g.u * 0.4f) * 0.5f;
			const int   cur = static_cast<int>(std::lround(a_entry.position));
			im::SetCursorScreenPos({ a_pos.x + g.padX, ctrlY });
			if (W::Button("##male", Icon::Mars, "Male", { half, g.ctrl }, false, cur == 0) && cur != 0) {
				CommitValue(a_model, a_entry, 0.0);
			}
			im::SetCursorScreenPos({ a_pos.x + g.padX + half + g.u * 0.4f, ctrlY });
			if (W::Button("##female", Icon::Venus, "Female", { half, g.ctrl }, false, cur == 1) && cur != 1) {
				CommitValue(a_model, a_entry, 1.0);
			}
		}

		void RowPaint(Model& a_model, Entry& a_entry, const Item& a_item, const RowGeometry& g, ImVec2 a_pos, float a_width, bool a_changed)
		{
			auto*       list = im::GetWindowDrawList();
			const float right = a_pos.x + a_width - g.padX;
			const float lineY = a_pos.y + g.padY;
			const ImVec2 sw{ g.u * 1.9f, g.line * 0.8f };
			im::SetCursorScreenPos({ right - sw.x, lineY + (g.line - sw.y) * 0.5f });
			if (W::Swatch("##swatch", a_entry.fillColor, sw, "Edit colour")) {
				s.colorKey = a_entry.key;
				s.openColor = true;
			}
			DrawLabel(list, a_entry, a_item, { a_pos.x + g.padX, lineY }, right - sw.x - g.u * 0.5f - (a_pos.x + g.padX), a_changed);

			const float ctrlY = lineY + g.line + g.gap;
			const auto  stem = a_entry.texture.empty() ? std::string("Default") : Text::FileStem(a_entry.texture);
			im::SetCursorScreenPos({ a_pos.x + g.padX, ctrlY });
			const float  width = right - (a_pos.x + g.padX);
			const ImVec2 pos{ a_pos.x + g.padX, ctrlY };
			const bool   clicked = im::InvisibleButton("##texture", { width, g.ctrl });
			const bool   hovered = im::IsItemHovered();
			dl::AddRectFilled(list, pos, pos + ImVec2{ width, g.ctrl }, hovered ? Color::FrameHover : Color::Frame, Round(g.u * 0.25f), 0);
			DrawIcon(list, pos + ImVec2{ g.u * 0.8f, g.ctrl * 0.5f }, Icon::Brush, Color::Gold);
			const auto text = W::Ellipsize(stem, width - g.u * 3.5f);
			const auto ts = im::CalcTextSize(text.c_str());
			dl::AddText(list, pos + ImVec2{ g.u * 1.6f, (g.ctrl - ts.y) * 0.5f }, a_changed ? Color::GoldBright : Color::Text, text.c_str());
			DrawIcon(list, pos + ImVec2{ width - g.u * 0.8f, g.ctrl * 0.5f }, Icon::ChevronDown, Color::TextFaint);
			if (clicked) {
				s.textureKey = a_entry.key;
				s.openTexture = true;
			}
		}

		void RenderRow(Model& a_model, Entry& a_entry, const Item& a_item, const RowGeometry& g, ImVec2 a_pos, float a_width)
		{
			im::PushID(a_entry.key.c_str());
			auto*        list = im::GetWindowDrawList();
			const ImVec2 max = a_pos + ImVec2{ a_width, g.height };
			const bool   hovered = im::IsMouseHoveringRect(a_pos, max) && im::IsWindowHovered();
			const bool   changed = a_entry.IsChanged();
			const float rounding = Round(g.u * 0.3f);
			// card: alternating tone, hairline border, a colour edge for the
			// slider family; a gold outline marks what changed this session
			dl::AddRectFilled(list, a_pos, max, hovered ? Color::RowHover : (a_item.stripe % 2 ? Color::RowAlt : Color::Row), rounding, 0);
			dl::AddRectFilled(list, a_pos, { a_pos.x + std::max(3.0f, std::floor(g.u * 0.16f)), max.y }, a_item.hue, rounding,
				ImGuiMCP::ImDrawFlags_RoundCornersLeft);
			dl::AddRect(list, a_pos, max, changed ? WithAlpha(Color::Gold, 0.75f) : Color::RowBorder, rounding, 0, changed ? 1.5f : 1.0f);

			switch (a_entry.control) {
			case Control::Slider:
				RowSlider(a_model, a_entry, a_item, g, a_pos, a_width, changed);
				break;
			case Control::Stepper:
				RowStepper(a_model, a_entry, a_item, g, a_pos, a_width, changed);
				break;
			case Control::Sex:
				RowSex(a_model, a_entry, a_item, g, a_pos, a_width, changed);
				break;
			case Control::Paint:
				RowPaint(a_model, a_entry, a_item, g, a_pos, a_width, changed);
				break;
			default:
				break;
			}

			// double-click the label to reset, right-click anywhere for more
			const ImVec2 labelMax{ max.x, a_pos.y + g.padY + g.line };
			if (hovered && im::IsMouseHoveringRect(a_pos, labelMax) && !im::IsAnyItemHovered() && im::IsMouseDoubleClicked(0) && changed) {
				a_model.ResetEntry(a_entry);
			}
			if (hovered && im::IsMouseClicked(1)) {
				s.contextKey = a_entry.key;
				s.openContext = true;
			}
			im::PopID();
		}

		// compact name tiles; neighbours alternate between two tints so the
		// eye can tell them apart, the current race is outlined in gold
		void RenderRaces(Model& a_model, float a_width, float a_cardH, float a_gap, int a_columns)
		{
			auto*       list = im::GetWindowDrawList();
			const float u = U();
			const float cardW = std::floor((a_width - a_gap * (a_columns - 1)) / a_columns);
			const auto  origin = im::GetCursorScreenPos();
			const auto  query = Text::Lower(s.search);
			int         index = 0;
			for (auto& e : a_model.entries) {
				if (e.control != Control::Race || (!query.empty() && e.search.find(query) == std::string::npos)) {
					continue;
				}
				const int    col = index % a_columns;
				const int    row = index / a_columns;
				const ImVec2 pos = origin + ImVec2{ col * (cardW + a_gap), row * (a_cardH + a_gap) };
				const ImVec2 max = pos + ImVec2{ cardW, a_cardH };
				const ImU32  tint = (row + col) % 2 ? Color::Hues[0] : Color::Hues[2];
				++index;

				im::PushID(e.key.c_str());
				im::SetCursorScreenPos(pos);
				const bool  clicked = im::InvisibleButton("##race", { cardW, a_cardH });
				const bool  hovered = im::IsItemHovered();
				const bool  current = a_model.IsCurrentRace(e);
				const float rounding = Round(u * 0.3f);

				dl::AddRectFilled(list, pos, max, hovered ? Color::RowHover : Color::Row, rounding, 0);
				dl::AddRectFilled(list, pos, max, current ? Color::GoldWashStrong : WithAlpha(tint, hovered ? 0.22f : 0.12f), rounding, 0);
				dl::AddRectFilled(list, pos, { pos.x + std::max(3.0f, std::floor(u * 0.16f)), max.y }, current ? Color::Gold : tint, rounding, ImGuiMCP::ImDrawFlags_RoundCornersLeft);
				dl::AddRect(list, pos, max, current ? Color::BorderStrong : Color::RowBorder, rounding, 0, current ? 1.5f : 1.0f);

				float textX = pos.x + u * 0.65f;
				if (current) {
					DrawIcon(list, { textX + u * 0.35f, pos.y + a_cardH * 0.5f }, Icon::Check, Color::GoldBright);
					textX += u * 1.1f;
				}
				const auto name = W::Ellipsize(e.label, max.x - textX - u * 0.4f);
				const auto ns = im::CalcTextSize(name.c_str());
				dl::AddText(list, { textX, pos.y + (a_cardH - ns.y) * 0.5f }, current ? Color::GoldBright : hovered ? Color::Text : Color::TileText, name.c_str());
				if (hovered) {
					const auto tip = current ? std::string("Current race") : "Change the race to " + e.label;
					W::Tooltip(tip.c_str());
				}
				if (clicked && !current) {
					a_model.ChangeRace(e);
				}
				im::PopID();
			}
			im::SetCursorScreenPos(origin + ImVec2{ 0.0f, ((index + a_columns - 1) / a_columns) * (a_cardH + a_gap) });
		}

		// another mod's list on the race list (Apprentice's classes or traits): the same tiles as the races, the
		// entry's own description as the tooltip, the menu's own item-press handler on a click
		void RenderChoices(Model& a_model, const std::vector<std::size_t>& a_members, float a_width, float a_cardH, float a_gap, int a_columns)
		{
			auto*       list = im::GetWindowDrawList();
			const float u = U();
			const float cardW = std::floor((a_width - a_gap * (a_columns - 1)) / a_columns);
			const auto  origin = im::GetCursorScreenPos();
			int         index = 0;
			for (const auto i : a_members) {
				if (i >= a_model.entries.size()) {
					continue;
				}
				auto&        e = a_model.entries[i];
				const int    col = index % a_columns;
				const int    row = index / a_columns;
				const ImVec2 pos = origin + ImVec2{ col * (cardW + a_gap), row * (a_cardH + a_gap) };
				const ImVec2 max = pos + ImVec2{ cardW, a_cardH };
				const ImU32  tint = (row + col) % 2 ? Color::Hues[3] : Color::Hues[1];
				++index;

				im::PushID(e.key.c_str());
				im::SetCursorScreenPos(pos);
				const bool  clicked = im::InvisibleButton("##choice", { cardW, a_cardH });
				const bool  hovered = im::IsItemHovered();
				const bool  current = a_model.IsCurrentChoice(e);
				const float rounding = Round(u * 0.3f);

				dl::AddRectFilled(list, pos, max, hovered ? Color::RowHover : Color::Row, rounding, 0);
				dl::AddRectFilled(list, pos, max, current ? Color::GoldWashStrong : WithAlpha(tint, hovered ? 0.22f : 0.12f), rounding, 0);
				dl::AddRectFilled(list, pos, { pos.x + std::max(3.0f, std::floor(u * 0.16f)), max.y }, current ? Color::Gold : tint, rounding, ImGuiMCP::ImDrawFlags_RoundCornersLeft);
				dl::AddRect(list, pos, max, current ? Color::BorderStrong : Color::RowBorder, rounding, 0, current ? 1.5f : 1.0f);

				float textX = pos.x + u * 0.65f;
				if (current) {
					DrawIcon(list, { textX + u * 0.35f, pos.y + a_cardH * 0.5f }, Icon::Check, Color::GoldBright);
					textX += u * 1.1f;
				}
				const auto name = W::Ellipsize(e.label, max.x - textX - u * 0.4f);
				const auto ns = im::CalcTextSize(name.c_str());
				dl::AddText(list, { textX, pos.y + (a_cardH - ns.y) * 0.5f }, current ? Color::GoldBright : hovered ? Color::Text : Color::TileText, name.c_str());
				if (hovered) {
					const auto tip = e.description.empty() ? (current ? std::string("Chosen") : "Choose " + e.label) : e.label + "\n\n" + e.description;
					W::Tooltip(tip.c_str());
				}
				if (clicked && !current) {
					a_model.PressChoice(e);
				}
				im::PopID();
			}
			im::SetCursorScreenPos(origin + ImVec2{ 0.0f, ((index + a_columns - 1) / a_columns) * (a_cardH + a_gap) });
		}

		int RaceCount(const Model& a_model)
		{
			const auto query = Text::Lower(s.search);
			int        count = 0;
			for (const auto& e : a_model.entries) {
				if (e.control == Control::Race && (query.empty() || e.search.find(query) != std::string::npos)) {
					++count;
				}
			}
			return count;
		}

		// ------------------------------------------------------------------
		// popups of the list

		void ContextPopup(Model& a_model)
		{
			if (s.openContext) {
				im::OpenPopup("##rowctx");
				s.openContext = false;
			}
			if (!im::BeginPopup("##rowctx")) {
				return;
			}
			ApplyFont();
			auto* e = a_model.Find(s.contextKey);
			if (!e) {
				im::CloseCurrentPopup();
				im::EndPopup();
				return;
			}
			im::TextColored(ToVec4(Color::Gold), "%s", e->label.c_str());
			im::Separator();
			im::BeginDisabled(!e->IsChanged());
			if (im::Selectable("Reset to the starting value")) {
				a_model.ResetEntry(*e);
			}
			im::EndDisabled();
			if (e->control == Control::Slider && im::Selectable("Type a value")) {
				s.editKey = e->key;
				s.editValue = e->position;
				s.editGrace = 4;
			}
			if (e->control == Control::Slider && e->min < 0.0 && e->max > 0.0 && im::Selectable("Set to zero")) {
				CommitValue(a_model, *e, 0.0);
			}
			if (e->hasColor || e->control == Control::Paint) {
				if (im::Selectable("Edit colour")) {
					s.colorKey = e->key;
					s.openColor = true;
				}
			}
			if (e->callback == "ChangeHeadPart" && im::Selectable("Browse parts")) {
				OpenHeadPartBrowser(*e);
			}
			im::Separator();
			W::SmallText(im::GetWindowDrawList(), im::GetCursorScreenPos(), Color::TextFaint,
				std::format("{} / id {} / flag {}", e->callback, e->sliderID, e->filterFlag).c_str(), 0.72f);
			im::Dummy({ U() * 12.0f, U() * 0.8f });
			if (e->rawText != e->label) {
				W::SmallText(im::GetWindowDrawList(), im::GetCursorScreenPos(), Color::TextFaint, e->rawText.c_str(), 0.72f);
				im::Dummy({ U() * 12.0f, U() * 0.8f });
			}
			im::EndPopup();
		}

		void FinishColorEdit(Model& a_model)
		{
			auto* e = a_model.Find(s.colorKey);
			if (e) {
				if (s.colorPending) {
					a_model.SetColor(*e, e->fillColor, true);
				}
				if (e->fillColor != s.colorFrom) {
					a_model.Record({ HistoryStep{ .key = e->key, .kind = HistoryStep::Kind::Color, .fromColor = s.colorFrom, .toColor = e->fillColor } });
					Settings::Get().AddRecentColor(e->fillColor);
					Settings::Get().Save();
				}
			}
			s.colorPending = false;
			s.colorKey.clear();
		}

		void ColorPopup(Model& a_model)
		{
			if (s.openColor) {
				if (auto* e = a_model.Find(s.colorKey)) {
					s.colorFrom = e->fillColor;
					const auto argb = e->fillColor;
					std::uint32_t alpha = (argb >> 24) & 0xFF;
					if (alpha == 0) {
						alpha = e->tintType == Defs::kTintHair ? 255 : 128;
					}
					s.colorEdit[0] = ((argb >> 16) & 0xFF) / 255.0f;
					s.colorEdit[1] = ((argb >> 8) & 0xFF) / 255.0f;
					s.colorEdit[2] = (argb & 0xFF) / 255.0f;
					s.colorEdit[3] = alpha / 255.0f;
					s.colorPending = false;
					im::OpenPopup("##color");
				}
				s.openColor = false;
			}
			if (!im::BeginPopup("##color")) {
				if (!s.colorKey.empty() && !s.openColor) {
					FinishColorEdit(a_model);
				}
				return;
			}
			ApplyFont();
			auto* e = a_model.Find(s.colorKey);
			if (!e) {
				im::CloseCurrentPopup();
				im::EndPopup();
				return;
			}
			const float u = U();
			im::TextColored(ToVec4(Color::Gold), "%s", e->label.c_str());

			const auto toArgb = [](const float c[4]) {
				const auto ch = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
				return (ch(c[3]) << 24) | (ch(c[0]) << 16) | (ch(c[1]) << 8) | ch(c[2]);
			};
			bool changed = false;
			im::SetNextItemWidth(u * 15.0f);
			if (im::ColorPicker4("##picker", s.colorEdit,
					ImGuiMCP::ImGuiColorEditFlags_AlphaBar | ImGuiMCP::ImGuiColorEditFlags_NoSidePreview |
						ImGuiMCP::ImGuiColorEditFlags_DisplayHex | ImGuiMCP::ImGuiColorEditFlags_AlphaPreviewHalf)) {
				changed = true;
			}

			const auto paletteRow = [&](const char* a_id, auto&& a_colors, bool a_keepAlpha) {
				im::PushID(a_id);
				int n = 0;
				for (const std::uint32_t c : a_colors) {
					if (n % 8 != 0) {
						im::SameLine(0.0f, u * 0.25f);
					}
					im::PushID(n++);
					const std::uint32_t shown = a_keepAlpha ? c : (0xFF000000u | c);
					if (W::Swatch("##p", shown, { u * 1.55f, u * 1.1f })) {
						s.colorEdit[0] = ((c >> 16) & 0xFF) / 255.0f;
						s.colorEdit[1] = ((c >> 8) & 0xFF) / 255.0f;
						s.colorEdit[2] = (c & 0xFF) / 255.0f;
						if (a_keepAlpha && (c >> 24)) {
							s.colorEdit[3] = (c >> 24) / 255.0f;
						}
						changed = true;
					}
					im::PopID();
				}
				im::PopID();
			};
			W::SectionTitle("PALETTE");
			paletteRow("palette", kPalette, false);
			if (!Settings::Get().recentColors.empty()) {
				W::SectionTitle("RECENT");
				paletteRow("recent", Settings::Get().recentColors, true);
			}

			if (changed) {
				const auto   argb = toArgb(s.colorEdit);
				const double now = Text::Now();
				const bool   send = now - s.colorLastSend > 0.1;
				if (send) {
					s.colorLastSend = now;
				}
				a_model.SetColor(*e, argb, send);
				s.colorPending = !send;
			} else if (s.colorPending && Text::Now() - s.colorLastSend > 0.1) {
				a_model.SetColor(*e, e->fillColor, true);
				s.colorLastSend = Text::Now();
				s.colorPending = false;
			}

			im::Spacing();
			if (W::Button("##revert", Icon::Undo, "Revert", { u * 6.0f, 0.0f })) {
				a_model.SetColor(*e, s.colorFrom, true);
				s.colorPending = false;
				const auto c = s.colorFrom;
				s.colorEdit[0] = ((c >> 16) & 0xFF) / 255.0f;
				s.colorEdit[1] = ((c >> 8) & 0xFF) / 255.0f;
				s.colorEdit[2] = (c & 0xFF) / 255.0f;
				s.colorEdit[3] = ((c >> 24) & 0xFF) / 255.0f;
			}
			im::SameLine();
			if (W::Button("##ok", Icon::Check, "Done", { u * 6.0f, 0.0f }, true)) {
				im::CloseCurrentPopup();
			}
			im::EndPopup();
		}

		// Like RaceMenu's own list: whatever is picked goes onto the character at
		// once and the list stays open, so textures can be compared in place.
		// The arrow keys step through the (filtered) list the same way. The
		// whole visit is one undo step, recorded when the picker closes.
		void ApplyTexture(Model& a_model, Entry& a_entry, const std::string& a_texture)
		{
			if (Text::Lower(a_texture) != Text::Lower(a_entry.texture)) {
				a_model.SetTexture(a_entry, a_texture);
			}
		}

		void FinishTextureEdit(Model& a_model)
		{
			if (auto* e = a_model.Find(s.textureKey); e && Text::Lower(e->texture) != Text::Lower(s.textureFrom)) {
				a_model.Record({ HistoryStep{ .key = e->key, .kind = HistoryStep::Kind::Texture, .fromTexture = s.textureFrom, .toTexture = e->texture } });
			}
			s.textureKey.clear();
		}

		void TexturePopup(Model& a_model)
		{
			const float u = U();
			const auto* io = im::GetIO();
			if (s.openTexture) {
				if (auto* e = a_model.Find(s.textureKey)) {
					s.textureFrom = e->texture;
					s.textureScroll = true;
					im::OpenPopup("##texture");
					// beside the editor, so the character stays in view
					const float x = std::min(s.editorRight + u * 0.6f, io->DisplaySize.x - u * 25.0f);
					const float y = std::clamp(im::GetMousePos().y - u * 6.0f, u, io->DisplaySize.y - u * 34.0f);
					im::SetNextWindowPos({ x, y });
				}
				s.openTexture = false;
			}
			im::SetNextWindowSize({ u * 24.0f, u * 33.0f });
			if (!im::BeginPopup("##texture")) {
				if (!s.textureKey.empty()) {
					FinishTextureEdit(a_model);
				}
				return;
			}
			ApplyFont();
			auto* e = a_model.Find(s.textureKey);
			if (!e || e->listType < 0 || e->listType >= static_cast<int>(a_model.makeup.size())) {
				im::TextColored(ToVec4(Color::TextDim), "No textures are registered for this slot.");
				im::EndPopup();
				return;
			}
			const auto& catalog = a_model.makeup[e->listType];
			im::TextColored(ToVec4(Color::GoldBright), "%s", e->label.c_str());
			W::SmallText(im::GetWindowDrawList(), im::GetCursorScreenPos(), Color::TextFaint, "Click or use the arrow keys - the character updates at once", 0.75f);
			im::Dummy({ 1.0f, U() * 0.85f });

			if (im::IsWindowAppearing()) {
				im::SetKeyboardFocusHere();
			}
			im::SetNextItemWidth(-1.0f);
			if (im::InputTextWithHint("##search", "Search textures (kept between openings)", s.textureSearch, sizeof(s.textureSearch))) {
				s.textureScroll = true;
			}
			const auto query = Text::Lower(s.textureSearch);

			std::vector<int> shown;
			int              currentRow = -1;
			const auto       currentLower = Text::Lower(e->texture);
			for (int i = 0; i < static_cast<int>(catalog.size()); ++i) {
				if (query.empty() || catalog[i].search.find(query) != std::string::npos) {
					if (Text::Lower(catalog[i].texture) == currentLower) {
						currentRow = static_cast<int>(shown.size());
					}
					shown.push_back(i);
				}
			}

			// arrow keys walk the list and apply, like scrolling RaceMenu's list
			if (!shown.empty() && (im::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow, true) || im::IsKeyPressed(ImGuiMCP::ImGuiKey_UpArrow, true))) {
				const int dir = im::IsKeyPressed(ImGuiMCP::ImGuiKey_DownArrow, true) ? 1 : -1;
				const int next = currentRow < 0 ? (dir > 0 ? 0 : static_cast<int>(shown.size()) - 1) :
				                                  std::clamp(currentRow + dir, 0, static_cast<int>(shown.size()) - 1);
				ApplyTexture(a_model, *e, catalog[shown[next]].texture);
				currentRow = next;
				s.textureScroll = true;
			}

			const float footerH = im::GetFrameHeight() + im::GetStyle()->ItemSpacing.y;
			im::BeginChild("##list", { 0.0f, -footerH });
			ApplyFont();
			if (shown.empty()) {
				im::TextColored(ToVec4(Color::TextFaint), "%s", catalog.empty() ? "No textures are registered for this slot." : "No matching textures.");
			}
			const float rowH = im::GetTextLineHeightWithSpacing();
			auto*       clipper = ImGuiMCP::ImGuiListClipperManager::Create();
			ImGuiMCP::ImGuiListClipperManager::Begin(clipper, static_cast<int>(shown.size()), rowH);
			if (s.textureScroll && currentRow >= 0) {
				ImGuiMCP::ImGuiListClipperManager::IncludeItemByIndex(clipper, currentRow);
			}
			while (ImGuiMCP::ImGuiListClipperManager::Step(clipper)) {
				for (int r = clipper->DisplayStart; r < clipper->DisplayEnd; ++r) {
					const auto& tex = catalog[shown[r]];
					im::PushID(shown[r]);
					const bool current = r == currentRow;
					if (current) {
						const auto pos = im::GetCursorScreenPos();
						dl::AddRectFilled(im::GetWindowDrawList(), pos, pos + ImVec2{ std::max(3.0f, std::floor(U() * 0.16f)), rowH - im::GetStyle()->ItemSpacing.y }, Color::Gold, 0.0f, 0);
					}
					if (im::Selectable(tex.label.c_str(), current, ImGuiMCP::ImGuiSelectableFlags_DontClosePopups)) {
						ApplyTexture(a_model, *e, tex.texture);
					}
					if (current && s.textureScroll) {
						im::SetScrollHereY(0.4f);
						s.textureScroll = false;
					}
					if (im::IsItemHovered()) {
						if (const auto* image = Preview::Get(tex.texture); image && im::BeginTooltip()) {
							ApplyFont();
							W::TextureImage(tex.texture, { U() * 9.0f, U() * 9.0f });
							im::EndTooltip();
						}
					}
					im::PopID();
				}
			}
			ImGuiMCP::ImGuiListClipperManager::End(clipper);
			ImGuiMCP::ImGuiListClipperManager::Destroy(clipper);
			if (currentRow < 0) {
				s.textureScroll = false;
			}
			im::EndChild();

			const float half = (im::GetContentRegionAvail().x - u * 0.3f) * 0.5f;
			const bool  changed = Text::Lower(e->texture) != Text::Lower(s.textureFrom);
			im::BeginDisabled(!changed);
			if (W::Button("##revert", Icon::Undo, "Revert", { half, 0.0f })) {
				ApplyTexture(a_model, *e, s.textureFrom);
				s.textureScroll = true;
			}
			im::EndDisabled();
			im::SameLine(0.0f, u * 0.3f);
			if (W::Button("##close", Icon::Check, "Done", { half, 0.0f }, true) || im::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false)) {
				im::CloseCurrentPopup();
			}
			im::EndPopup();
		}

		void HeadPartBrowser(Model& a_model)
		{
			if (s.openHeadParts) {
				im::OpenPopup("Choose a part##headparts");
				s.openHeadParts = false;
			}
			const auto* io = im::GetIO();
			const float u = U();
			im::SetNextWindowPos({ io->DisplaySize.x * 0.5f, io->DisplaySize.y * 0.5f }, ImGuiMCP::ImGuiCond_Appearing, { 0.5f, 0.5f });
			im::SetNextWindowSize({ std::min(io->DisplaySize.x * 0.6f, u * 40.0f), io->DisplaySize.y * 0.72f }, ImGuiMCP::ImGuiCond_Appearing);
			if (!im::BeginPopupModal("Choose a part##headparts", nullptr, ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}
			ApplyFont();
			auto* e = a_model.Find(s.headPartKey);
			const float uu = U();
			if (!e) {
				im::CloseCurrentPopup();
				im::EndPopup();
				return;
			}
			im::TextColored(ToVec4(Color::GoldBright), "%s", e->label.c_str());
			im::SameLine(im::GetContentRegionMax().x - im::GetFrameHeight());
			if (W::IconButton("##close", Icon::Close, "Close") || im::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false)) {
				im::CloseCurrentPopup();
			}

			std::vector<std::string> plugins;
			for (const auto& part : a_model.headParts) {
				if (std::ranges::find(plugins, part.plugin) == plugins.end()) {
					plugins.push_back(part.plugin);
				}
			}
			std::ranges::sort(plugins, [](const std::string& a, const std::string& b) { return Text::Lower(a) < Text::Lower(b); });

			if (im::IsWindowAppearing()) {
				im::SetKeyboardFocusHere();
			}
			im::SetNextItemWidth(im::GetContentRegionAvail().x * 0.55f);
			im::InputTextWithHint("##search", "Search by name, number or plugin", s.headPartSearch, sizeof(s.headPartSearch));
			im::SameLine();
			im::SetNextItemWidth(-1.0f);
			const auto pluginLabel = s.headPartPlugin.empty() ? std::format("All plugins ({})", plugins.size()) : s.headPartPlugin;
			if (im::BeginCombo("##plugin", pluginLabel.c_str())) {
				ApplyFont();
				if (im::Selectable("All plugins", s.headPartPlugin.empty())) {
					s.headPartPlugin.clear();
				}
				for (const auto& plugin : plugins) {
					if (im::Selectable(plugin.c_str(), plugin == s.headPartPlugin)) {
						s.headPartPlugin = plugin;
					}
				}
				im::EndCombo();
			}

			const auto query = Text::Lower(s.headPartSearch);
			std::vector<const HeadPart*> shown;
			for (const auto& part : a_model.headParts) {
				if ((s.headPartPlugin.empty() || part.plugin == s.headPartPlugin) && (query.empty() || part.search.find(query) != std::string::npos)) {
					shown.push_back(&part);
				}
			}
			if (!a_model.headPartsLoaded) {
				im::TextColored(ToVec4(Color::TextDim), "Reading the parts from RaceMenu...");
			} else {
				im::TextColored(ToVec4(Color::TextFaint), "%zu of %zu parts", shown.size(), a_model.headParts.size());
			}

			im::BeginChild("##parts", { 0.0f, 0.0f }, ImGuiMCP::ImGuiChildFlags_None);
			ApplyFont();
			const int  current = static_cast<int>(std::lround(e->position));
			const float rowH = im::GetTextLineHeightWithSpacing() * 1.25f;
			auto*      clipper = ImGuiMCP::ImGuiListClipperManager::Create();
			ImGuiMCP::ImGuiListClipperManager::Begin(clipper, static_cast<int>(shown.size()), rowH);
			while (ImGuiMCP::ImGuiListClipperManager::Step(clipper)) {
				for (int r = clipper->DisplayStart; r < clipper->DisplayEnd; ++r) {
					const auto& part = *shown[r];
					im::PushID(r);
					const auto  pos = im::GetCursorScreenPos();
					const float width = im::GetContentRegionAvail().x;
					const bool  isCurrent = part.index == current;
					if (im::Selectable("##part", isCurrent, 0, { width, rowH - im::GetStyle()->ItemSpacing.y })) {
						if (!isCurrent) {
							CommitValue(a_model, *e, static_cast<double>(part.index));
						}
						im::CloseCurrentPopup();
					}
					auto* list = im::GetWindowDrawList();
					const float ty = pos.y + (rowH - im::GetStyle()->ItemSpacing.y - uu) * 0.5f;
					W::SmallText(list, { pos.x + uu * 0.4f, ty + uu * 0.1f }, Color::TextFaint, std::format("#{}", part.index).c_str(), 0.8f);
					const auto pluginSize = W::SmallTextSize(part.plugin.c_str(), 0.8f);
					const auto label = W::Ellipsize(part.label.empty() ? part.name : part.label, width - pluginSize.x - uu * 5.0f);
					dl::AddText(list, { pos.x + uu * 3.0f, ty }, isCurrent ? Color::GoldBright : Color::Text, label.c_str());
					W::SmallText(list, { pos.x + width - pluginSize.x - uu * 0.5f, ty + uu * 0.1f }, Color::TextFaint, part.plugin.c_str(), 0.8f);
					if (im::IsItemHovered() && part.label != part.name) {
						W::Tooltip(part.name.c_str());
					}
					im::PopID();
				}
			}
			ImGuiMCP::ImGuiListClipperManager::End(clipper);
			ImGuiMCP::ImGuiListClipperManager::Destroy(clipper);
			im::EndChild();
			im::EndPopup();
		}

		void ConfirmModal(const char* a_id, bool& a_open, const char* a_title, const std::string& a_text, const char* a_confirm, const std::function<void()>& a_onConfirm)
		{
			if (a_open) {
				im::OpenPopup(a_id);
				a_open = false;
			}
			const auto* io = im::GetIO();
			im::SetNextWindowPos({ io->DisplaySize.x * 0.5f, io->DisplaySize.y * 0.5f }, ImGuiMCP::ImGuiCond_Appearing, { 0.5f, 0.5f });
			if (!im::BeginPopupModal(a_id, nullptr, ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings)) {
				return;
			}
			ApplyFont();
			const float u = U();
			im::TextColored(ToVec4(Color::GoldBright), "%s", a_title);
			im::Spacing();
			im::PushTextWrapPos(u * 24.0f);
			im::TextColored(ToVec4(Color::TextDim), "%s", a_text.c_str());
			im::PopTextWrapPos();
			im::Spacing();
			im::Spacing();
			if (W::Button("##confirm", Icon::Check, a_confirm, { u * 9.0f, u * 1.8f }, true) || im::IsKeyPressed(ImGuiMCP::ImGuiKey_Enter, false)) {
				a_onConfirm();
				im::CloseCurrentPopup();
			}
			im::SameLine();
			if (W::Button("##cancel", Icon::Close, "Cancel", { u * 9.0f, u * 1.8f }) || im::IsKeyPressed(ImGuiMCP::ImGuiKey_Escape, false)) {
				im::CloseCurrentPopup();
			}
			im::EndPopup();
		}

		void OptionsPopup()
		{
			if (!im::BeginPopup("##options")) {
				return;
			}
			ApplyFont();
			auto&       st = Settings::Get();
			const float u = U();
			bool        save = false;
			W::SectionTitle("SLIDERS");
			save |= im::Checkbox("Hide body sliders of the other body type", &st.sexFilter);
			save |= im::Checkbox("Group related sliders under headers", &st.groupHeaders);
			save |= im::Checkbox("Show RaceMenu's technical slider names", &st.showTechnicalNames);
			save |= im::Checkbox("Typed values may exceed custom body morph ranges", &st.overdrive);
			W::SectionTitle("INTERFACE");
			save |= im::Checkbox("Ask before finishing the character", &st.confirmDone);
			save |= im::Checkbox("Show the scene and presets panel", &st.sidePanel);
			im::SetNextItemWidth(u * 10.0f);
			im::SliderFloat("Interface size", &st.uiScale, 0.7f, 1.6f, "%.2f");
			save |= im::IsItemDeactivatedAfterEdit();
			im::SetNextItemWidth(u * 10.0f);
			im::SliderFloat("Editor width", &st.editorWidth, 0.22f, 0.45f, "%.2f");
			save |= im::IsItemDeactivatedAfterEdit();
			im::SetNextItemWidth(u * 10.0f);
			im::SliderFloat("Drag rotation speed", &st.rotateSpeed, 0.1f, 1.0f, "%.2f");
			save |= im::IsItemDeactivatedAfterEdit();
			im::Spacing();
			W::SmallText(im::GetWindowDrawList(), im::GetCursorScreenPos(), Color::TextFaint,
				std::format("{} switches to RaceMenu's own interface. Settings: Data\\SKSE\\Plugins\\RaceMenuAtelier.ini", KeyName(st.toggleKey)).c_str(), 0.75f);
			im::Dummy({ u * 20.0f, u * 0.9f });
			if (save) {
				st.Save();
			}
			im::EndPopup();
		}

		// ------------------------------------------------------------------
		// editor window

		void RenderHeader(Model& a_model)
		{
			const float u = U();
			auto*       list = im::GetWindowDrawList();
			const auto  pos = im::GetCursorScreenPos();
			const float width = im::GetContentRegionAvail().x;

			W::SmallText(list, pos, Color::Gold, "RACEMENU ATELIER", 0.85f);
			std::string who = a_model.player.race;
			if (a_model.player.sex >= 0) {
				who += who.empty() ? "" : "  \xC2\xB7  ";
				who += a_model.player.sex == 1 ? "Female" : "Male";
			}
			const auto whoSize = W::SmallTextSize(who.c_str(), 0.85f);
			W::SmallText(list, { pos.x + width - whoSize.x, pos.y }, Color::TextDim, who.c_str(), 0.85f);
			im::Dummy({ width, whoSize.y + u * 0.35f });

			// name and Done
			const float doneW = u * 6.5f;
			const float h = std::floor(u * 1.9f);
			im::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImVec2{ u * 0.6f, (h - im::GetTextLineHeight()) * 0.5f });
			im::SetNextItemWidth(width - doneW - u * 0.5f);
			if (im::InputTextWithHint("##name", "Character name", s.name, sizeof(s.name))) {
				s.nameEdited = true;
			}
			im::PopStyleVar();
			im::SameLine(0.0f, u * 0.5f);
			if (W::Button("##done", Icon::Check, "Done", { doneW, h }, true)) {
				if (Settings::Get().confirmDone) {
					s.openDone = true;
				} else {
					Bridge::Done(s.name);
				}
			}
			if (im::IsItemHovered()) {
				W::Tooltip("Finish character creation (R)");
			}
		}

		void RenderToolbar(Model& a_model)
		{
			const float u = U();
			const float b = im::GetFrameHeight();
			const float width = im::GetContentRegionAvail().x;

			const auto undoTip = a_model.past.empty() ? std::string("Nothing to undo") : std::format("Undo ({} steps) - Ctrl+Z", a_model.past.size());
			im::BeginDisabled(a_model.past.empty());
			if (W::IconButton("##undo", Icon::Undo, undoTip.c_str())) {
				a_model.Undo();
			}
			im::EndDisabled();
			im::SameLine(0.0f, u * 0.25f);
			const auto redoTip = a_model.future.empty() ? std::string("Nothing to redo") : std::format("Redo ({} steps) - Ctrl+Y", a_model.future.size());
			im::BeginDisabled(a_model.future.empty());
			if (W::IconButton("##redo", Icon::Redo, redoTip.c_str())) {
				a_model.Redo();
			}
			im::EndDisabled();
			im::SameLine(0.0f, u * 0.5f);

			// search with a leading glyph
			const float searchW = width - (b * 2.0f + u * 0.75f) - (b * 3.0f + u * 1.0f);
			const auto  searchPos = im::GetCursorScreenPos();
			if (s.focusSearch) {
				im::SetKeyboardFocusHere();
				s.focusSearch = false;
			}
			im::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding, ImVec2{ u * 1.9f, im::GetStyle()->FramePadding.y });
			im::SetNextItemWidth(searchW);
			im::SetNextItemAllowOverlap();
			if (im::InputTextWithHint("##search", "Search every slider (Ctrl+F)", s.search, sizeof(s.search))) {
				s.scrollTop = true;
			}
			im::PopStyleVar();
			DrawIcon(im::GetWindowDrawList(), searchPos + ImVec2{ u * 0.95f, b * 0.5f }, Icon::Search, Color::TextFaint);
			if (s.search[0]) {
				im::SameLine(0.0f, 0.0f);
				const auto after = im::GetCursorScreenPos();
				im::SetCursorScreenPos({ searchPos.x + searchW - b, searchPos.y });
				if (W::IconButton("##clear", Icon::Close, "Clear the search")) {
					s.search[0] = '\0';
					s.scrollTop = true;
				}
				im::SetCursorScreenPos(after);
			}
			im::SameLine(searchPos.x - im::GetWindowPos().x + searchW + u * 0.5f);
			if (W::IconButton("##changed", Icon::Filter, s.changedOnly ? "Showing changed sliders only" : "Show changed sliders only", s.changedOnly)) {
				s.changedOnly = !s.changedOnly;
				s.scrollTop = true;
			}
			im::SameLine(0.0f, u * 0.25f);
			if (W::IconButton("##side", Icon::Columns, Settings::Get().sidePanel ? "Hide the scene panel" : "Show the scene panel", Settings::Get().sidePanel)) {
				Settings::Get().sidePanel = !Settings::Get().sidePanel;
				Settings::Get().Save();
			}
			im::SameLine(0.0f, u * 0.25f);
			if (W::IconButton("##options", Icon::Gear, "Options")) {
				im::OpenPopup("##options");
			}
			OptionsPopup();
		}

		void RenderCategories(Model& a_model, ImVec2 a_size)
		{
			im::BeginChild("##categories", a_size, ImGuiMCP::ImGuiChildFlags_None);
			ApplyFont();
			const float u = U();
			const float h = std::floor(u * 1.85f);
			const float width = im::GetContentRegionAvail().x;
			auto*       list = im::GetWindowDrawList();
			const int   active = s.search[0] ? -2 : ActiveCategory(a_model);

			const auto entry = [&](const char* a_id, const char* a_icon, const std::string& a_label, bool a_selected, int a_changed, ImU32 a_hue) {
				const auto pos = im::GetCursorScreenPos();
				const bool clicked = im::InvisibleButton(a_id, { width, h });
				const bool hovered = im::IsItemHovered();
				if (a_selected || hovered) {
					dl::AddRectFilled(list, pos, pos + ImVec2{ width, h }, a_selected ? WithAlpha(a_hue, 0.2f) : Color::RowHover, Round(u * 0.3f), 0);
				}
				if (a_selected) {
					dl::AddRectFilled(list, pos, pos + ImVec2{ std::max(3.0f, std::floor(u * 0.16f)), h }, a_hue, Round(u * 0.3f), ImGuiMCP::ImDrawFlags_RoundCornersLeft);
				}
				DrawIcon(list, pos + ImVec2{ u * 0.95f, h * 0.5f }, a_icon, a_hue);
				const float dotSpace = a_changed > 0 ? u * 0.8f : 0.0f;
				const auto  label = W::Ellipsize(a_label, width - u * 2.2f - dotSpace);
				const auto  ls = im::CalcTextSize(label.c_str());
				dl::AddText(list, pos + ImVec2{ u * 1.85f, (h - ls.y) * 0.5f }, a_selected ? Color::Text : hovered ? Color::Text : Color::TextDim, label.c_str());
				if (a_changed > 0) {
					dl::AddCircleFilled(list, pos + ImVec2{ width - u * 0.45f, h * 0.5f }, u * 0.16f, Color::Gold, 12);
				}
				if (hovered && label != a_label) {
					W::Tooltip(a_label.c_str());
				}
				return clicked;
			};

			if (entry("##all", Icon::Grid, "All", active == -1, s.changedTotal, Color::Gold)) {
				s.search[0] = '\0';
				SelectCategory(nullptr);
			}
			for (int i = 0; i < static_cast<int>(a_model.categories.size()); ++i) {
				const auto& c = a_model.categories[i];
				if (IsAllCategory(c) || (i < static_cast<int>(s.categoryCount.size()) && s.categoryCount[i] == 0)) {
					continue;
				}
				im::PushID(i);
				if (entry("##cat", CategoryIcon(c), c.label, active == i, i < static_cast<int>(s.categoryChanged.size()) ? s.categoryChanged[i] : 0, CategoryHue(i))) {
					s.search[0] = '\0';
					SelectCategory(&c);
				}
				im::PopID();
			}
			im::EndChild();
		}

		void RenderList(Model& a_model, ImVec2 a_size)
		{
			im::BeginChild("##list", a_size, ImGuiMCP::ImGuiChildFlags_None);
			ApplyFont();
			if (s.scrollTop) {
				im::SetScrollY(0.0f);
				s.scrollTop = false;
			}
			const auto g = Geometry();
			const float width = im::GetContentRegionAvail().x;
			const float gap = std::floor(g.u * 0.4f);
			const float headerH = std::floor(g.u * 1.7f);
			const float cardH = std::floor(g.u * 1.75f);
			const int   raceColumns = width >= g.u * 22.0f ? 3 : 2;

			if (!a_model.hasData || s.items.empty()) {
				const char* text = !a_model.hasData ? "Reading RaceMenu..." :
				                   s.search[0]      ? "Nothing matches the search." :
				                   s.changedOnly    ? "Nothing has been changed yet." :
				                                      "This category is empty.";
				im::TextColored(ToVec4(Color::TextDim), "%s", text);
			}

			const float scroll = im::GetScrollY();
			const float view = im::GetWindowHeight();
			const auto  origin = im::GetCursorScreenPos();
			float       y = 0.0f;
			auto*       list = im::GetWindowDrawList();
			const int   races = RaceCount(a_model);

			for (const auto& item : s.items) {
				float h = g.height;
				if (item.kind == Item::Kind::Header) {
					h = headerH;
				} else if (item.kind == Item::Kind::Races) {
					h = ((races + raceColumns - 1) / raceColumns) * (cardH + gap) - gap;
				} else if (item.kind == Item::Kind::Choices) {
					const int n = static_cast<int>(item.members.size());
					h = ((n + raceColumns - 1) / raceColumns) * (cardH + gap) - gap;
				}
				const bool inView = y + h >= scroll - g.height && y <= scroll + view + g.height;
				if (inView) {
					const ImVec2 pos = origin + ImVec2{ 0.0f, y };
					im::SetCursorScreenPos(pos);
					switch (item.kind) {
					case Item::Kind::Header:
						{
							// a tinted tab in the family's colour, then a rule
							const auto   ts = im::CalcTextSize(item.group.c_str());
							const float  tabH = ts.y + g.u * 0.45f;
							const ImVec2 tab{ pos.x, pos.y + headerH - tabH - g.u * 0.05f };
							dl::AddRectFilled(list, tab, tab + ImVec2{ ts.x + g.u * 1.2f, tabH }, WithAlpha(item.hue, 0.18f), Round(g.u * 0.25f), 0);
							dl::AddRectFilled(list, tab, tab + ImVec2{ std::max(3.0f, std::floor(g.u * 0.16f)), tabH }, item.hue, Round(g.u * 0.25f), ImGuiMCP::ImDrawFlags_RoundCornersLeft);
							dl::AddText(list, tab + ImVec2{ g.u * 0.65f, (tabH - ts.y) * 0.5f }, item.hue, item.group.c_str());
							const float ly = std::floor(tab.y + tabH * 0.5f);
							dl::AddLine(list, { tab.x + ts.x + g.u * 1.6f, ly }, { pos.x + width, ly }, WithAlpha(item.hue, 0.35f), 1.0f);
							break;
						}
					case Item::Kind::Races:
						RenderRaces(a_model, width, cardH, gap, raceColumns);
						break;
					case Item::Kind::Choices:
						RenderChoices(a_model, item.members, width, cardH, gap, raceColumns);
						break;
					case Item::Kind::Row:
						RenderRow(a_model, a_model.entries[item.entry], item, g, pos, width);
						break;
					}
				}
				y += h + gap;
			}
			im::SetCursorScreenPos(origin + ImVec2{ 0.0f, y });
			im::Dummy({ 1.0f, 1.0f });

			ContextPopup(a_model);
			ColorPopup(a_model);
			TexturePopup(a_model);
			im::EndChild();
		}

		void RenderFooter(Model& a_model)
		{
			const float u = U();
			auto*       list = im::GetWindowDrawList();
			const auto  pos = im::GetCursorScreenPos();
			const float width = im::GetContentRegionAvail().x;
			const float b = im::GetFrameHeight();

			const double age = Text::Now() - a_model.statusTime;
			if (!a_model.status.empty() && age < 6.0) {
				const float alpha = age > 5.0 ? static_cast<float>(6.0 - age) : 1.0f;
				const auto  text = W::Ellipsize(a_model.status, width - b * 1.5f);
				W::SmallText(list, { pos.x, pos.y + (b - u * 0.85f) * 0.5f }, WithAlpha(Color::GoldBright, alpha), text.c_str(), 0.85f);
			} else {
				const auto text = std::format("{} shown  \xC2\xB7  {} changed", s.visibleRows, s.changedTotal);
				W::SmallText(list, { pos.x, pos.y + (b - u * 0.85f) * 0.5f }, Color::TextFaint, text.c_str(), 0.85f);
			}
			im::SetCursorScreenPos({ pos.x + width - b, pos.y });
			im::BeginDisabled(s.changedTotal == 0);
			if (W::IconButton("##resetall", Icon::Refresh, "Reset every changed slider of this session")) {
				s.openResetAll = true;
			}
			im::EndDisabled();
		}

		void RenderEditor(Model& a_model)
		{
			const auto* io = im::GetIO();
			const float target = TargetFontSize();
			const float margin = std::floor(target * 0.8f);
			const float width = std::floor(io->DisplaySize.x * std::clamp(Settings::Get().editorWidth, 0.2f, 0.5f));

			im::SetNextWindowPos({ margin, margin });
			im::SetNextWindowSize({ width, io->DisplaySize.y - margin * 2.0f });
			if (!im::Begin(kEditorWindow, nullptr,
					ImGuiMCP::ImGuiWindowFlags_NoDecoration | ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings |
						ImGuiMCP::ImGuiWindowFlags_NoScrollWithMouse)) {
				im::End();
				return;
			}
			ApplyFont();
			const float u = U();
			s.editorRight = margin + width;

			RenderHeader(a_model);
			im::Dummy({ 1.0f, u * 0.15f });
			RenderToolbar(a_model);
			im::Dummy({ 1.0f, u * 0.1f });

			const float footerH = im::GetFrameHeight() + im::GetStyle()->ItemSpacing.y;
			const auto  avail = im::GetContentRegionAvail();
			const float railW = std::floor(std::clamp(avail.x * 0.3f, u * 6.5f, u * 10.0f));
			RenderCategories(a_model, { railW, avail.y - footerH });
			im::SameLine(0.0f, u * 0.5f);
			RenderList(a_model, { 0.0f, avail.y - footerH });
			RenderFooter(a_model);

			HeadPartBrowser(a_model);
			ConfirmModal("##confirmdone", s.openDone, "Finish character creation?",
				s.name[0] ? std::format("Your character will be named {}. RaceMenu can be opened again later with showracemenu.", s.name) :
							"RaceMenu can be opened again later with showracemenu.",
				"Finish", [] { Bridge::Done(s.name); });
			ConfirmModal("##confirmreset", s.openResetAll, "Reset every change?",
				std::format("{} changed sliders, colours and textures return to how they were when RaceMenu opened. Race and sex stay as they are. This can be undone.", s.changedTotal),
				"Reset", [&a_model] { a_model.ResetAll(); });
			im::End();
		}

		// ------------------------------------------------------------------
		// side panel

		void SceneSection(Model& a_model)
		{
			const float u = U();
			const float b = std::floor(u * 2.0f);
			const float dt = im::GetIO()->DeltaTime;

			const auto holdRotate = [&](const char* a_id, const char* a_icon, const char* a_tip, float a_dir) {
				W::IconButton(a_id, a_icon, a_tip, false, b);
				if (im::IsItemActive()) {
					Bridge::Rotate(a_dir * 110.0f * dt);
				}
			};
			holdRotate("##rotl", Icon::RotateLeft, "Turn left (hold, or Q)", -1.0f);
			im::SameLine(0.0f, u * 0.3f);
			holdRotate("##rotr", Icon::RotateRight, "Turn right (hold, or E)", 1.0f);
			im::SameLine(0.0f, u * 0.3f);
			if (W::IconButton("##zoom", a_model.zoomFace ? Icon::ZoomOut : Icon::ZoomIn, a_model.zoomFace ? "Show the whole body (Z, wheel)" : "Close-up on the face (Z, wheel)", false, b)) {
				Bridge::Zoom(!a_model.zoomFace);
			}
			im::SameLine(0.0f, u * 0.3f);
			if (W::IconButton("##light", a_model.lightOn ? Icon::Sun : Icon::Moon, a_model.lightOn ? "Turn RaceMenu's light off (L)" : "Turn RaceMenu's light on (L)", a_model.lightOn, b)) {
				Bridge::SetLight(!a_model.lightOn);
			}
			im::SameLine(0.0f, u * 0.3f);
			if (W::IconButton("##undress", Icon::Shirt, a_model.undressed ? "Put the equipment back on" : "Take the equipment off", a_model.undressed, b)) {
				Bridge::SetUndressed(!a_model.undressed);
			}
			im::SameLine(0.0f, u * 0.3f);
			if (W::IconButton("##freeze", Icon::Snowflake, a_model.frozen ? "Let the character move again" : "Freeze the pose and the face", a_model.frozen, b)) {
				Bridge::ToggleFreeze();
			}

			im::SetNextItemWidth(im::GetContentRegionAvail().x - b - u * 0.3f);
			if (im::BeginCombo("##pose", "Play a pose")) {
				ApplyFont();
				for (const auto& [event, label] : kPoses) {
					if (im::Selectable(label)) {
						Bridge::PlayPose(event);
					}
				}
				im::EndCombo();
			}
			im::SameLine(0.0f, u * 0.3f);
			if (W::IconButton("##stoppose", Icon::Stop, "Back to the standing pose")) {
				Bridge::PlayPose("IdleForceDefaultState");
			}
			W::SmallText(im::GetWindowDrawList(), im::GetCursorScreenPos(), Color::TextFaint, "Drag empty space to turn, mouse wheel to zoom", 0.75f);
			im::Dummy({ 1.0f, u * 0.85f });
		}

		void PresetsSection(Model& a_model)
		{
			const float u = U();
			if (!a_model.presetsLoaded) {
				a_model.presetsLoaded = true;  // one request per session
				Bridge::ListPresets();
			}
			const float saveW = u * 5.5f;
			im::SetNextItemWidth(im::GetContentRegionAvail().x - saveW - u * 0.3f);
			const bool enter = im::InputTextWithHint("##presetname", "New preset name", s.presetName, sizeof(s.presetName), ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
			im::SameLine(0.0f, u * 0.3f);
			const bool valid = Bridge::IsSafePresetName(s.presetName);
			im::BeginDisabled(!valid);
			if (W::Button("##savepreset", Icon::Save, "Save", { saveW, 0.0f }) || (enter && valid)) {
				Bridge::SavePreset(s.presetName);
			}
			im::EndDisabled();

			const auto  count = a_model.presetFiles.size();
			const float rowH = im::GetFrameHeight() + u * 0.2f;
			float       listH = std::min<float>(static_cast<float>(std::max<std::size_t>(count, 1)), 9.0f) * rowH + u * 0.4f;
			if (!s.presetInfoOpen.empty()) {
				listH += u * 9.0f;
			}
			im::BeginChild("##presets", { 0.0f, listH }, ImGuiMCP::ImGuiChildFlags_None);
			ApplyFont();
			if (a_model.presetFiles.empty()) {
				im::TextColored(ToVec4(Color::TextFaint), "No presets in SKSE\\Plugins\\CharGen\\Presets");
			}
			const float b = im::GetFrameHeight();
			for (const auto& file : a_model.presetFiles) {
				im::PushID(file.c_str());
				auto*       list = im::GetWindowDrawList();
				const auto  pos = im::GetCursorScreenPos();
				const float width = im::GetContentRegionAvail().x;
				const auto  slash = file.find_last_of('/');
				const auto  folder = slash == std::string::npos ? std::string{} : file.substr(0, slash + 1);
				const auto  stem = Text::FileStem(file);
				const bool  hovered = im::IsMouseHoveringRect(pos, pos + ImVec2{ width, b });
				if (hovered) {
					dl::AddRectFilled(list, pos, pos + ImVec2{ width, b }, Color::RowHover, Round(u * 0.25f), 0);
				}
				const float textW = width - b * 2.0f - u * 1.2f;
				float       x = pos.x + u * 0.3f;
				if (!folder.empty()) {
					const auto fs = W::SmallTextSize(folder.c_str(), 0.8f);
					W::SmallText(list, { x, pos.y + (b - fs.y) * 0.5f }, Color::TextFaint, folder.c_str(), 0.8f);
					x += std::min(fs.x, textW * 0.4f);
				}
				const auto name = W::Ellipsize(stem, pos.x + u * 0.3f + textW - x);
				dl::AddText(list, { x, pos.y + (b - im::GetTextLineHeight()) * 0.5f }, Color::Text, name.c_str());
				im::SetCursorScreenPos({ pos.x + width - b * 2.0f - u * 0.2f, pos.y });
				const bool infoOpen = s.presetInfoOpen == file;
				if (W::IconButton("##info", Icon::Info, "What this preset uses", infoOpen)) {
					s.presetInfoOpen = infoOpen ? std::string{} : file;
					if (!infoOpen && !a_model.presetInfo.contains(file)) {
						Bridge::ReadPresetInfo(file);
					}
				}
				im::SameLine(0.0f, u * 0.2f);
				if (W::IconButton("##load", Icon::Folder, "Load this preset")) {
					s.loadPreset = file;
					s.openLoadPreset = true;
				}
				if (infoOpen) {
					const auto it = a_model.presetInfo.find(file);
					im::Indent(u * 0.6f);
					if (it == a_model.presetInfo.end() || !it->second.loaded) {
						im::TextColored(ToVec4(Color::TextDim), "Reading...");
					} else if (!it->second.readable) {
						im::TextColored(ToVec4(Color::TextDim), "Only .jslot presets can be inspected.");
					} else {
						const auto& info = it->second;
						W::SectionTitle(std::format("MODS ({})", info.mods.size()).c_str());
						for (const auto& mod : info.mods) {
							const bool missing = std::ranges::find(info.missing, mod) != info.missing.end();
							im::TextColored(ToVec4(missing ? Color::Danger : Color::TextDim), "%s%s", mod.c_str(), missing ? "  (missing)" : "");
						}
						if (!info.headParts.empty()) {
							W::SectionTitle(std::format("HEAD PARTS ({})", info.headParts.size()).c_str());
							for (const auto& part : info.headParts) {
								im::TextColored(ToVec4(Color::TextDim), "%s", part.c_str());
							}
						}
						if (!info.tints.empty()) {
							W::SectionTitle(std::format("TINT TEXTURES ({})", info.tints.size()).c_str());
							for (const auto& tint : info.tints) {
								im::TextColored(ToVec4(Color::TextDim), "%s", tint.empty() ? "(default)" : Text::FileStem(tint).c_str());
								if (!tint.empty() && im::IsItemHovered() && im::BeginTooltip()) {
									ApplyFont();
									W::TextureImage(tint, { U() * 8.0f, U() * 8.0f });
									im::EndTooltip();
								}
							}
						}
					}
					im::Unindent(u * 0.6f);
				}
				im::PopID();
			}
			im::EndChild();
			if (W::Button("##refreshpresets", Icon::Refresh, "Refresh the list", { 0.0f, 0.0f })) {
				Bridge::ListPresets();
			}
			ConfirmModal("##confirmload", s.openLoadPreset, "Load this preset?",
				std::format("{} replaces the current character. Loading a preset cannot be undone, so save the current look first if you want to keep it.", Text::FileStem(s.loadPreset)),
				"Load", [] {
					Bridge::LoadPreset(s.loadPreset);
					auto&             model = Model::Get();
					std::scoped_lock lock(model.mutex);
					model.past.clear();
					model.future.clear();
				});
		}

		void BodySlideSection(Model& a_model)
		{
			const float u = U();
			const float half = (im::GetContentRegionAvail().x - u * 0.3f) * 0.5f;
			if (W::Button("##cbbe", nullptr, "CBBE 3BA", { half, 0.0f }, false, !s.bodySlideHimbo)) {
				s.bodySlideHimbo = false;
			}
			im::SameLine(0.0f, u * 0.3f);
			if (W::Button("##himbo", nullptr, "HIMBO", { half, 0.0f }, false, s.bodySlideHimbo)) {
				s.bodySlideHimbo = true;
			}
			const float exportW = u * 6.0f;
			im::SetNextItemWidth(im::GetContentRegionAvail().x - exportW - u * 0.3f);
			im::InputTextWithHint("##bsname", "BodySlide preset name", s.bodySlideName, sizeof(s.bodySlideName));
			im::SameLine(0.0f, u * 0.3f);
			const bool valid = Bridge::IsSafePresetName(s.bodySlideName);
			im::BeginDisabled(!valid);
			if (W::Button("##export", Icon::Export, "Export", { exportW, 0.0f })) {
				// RaceMenu's morph is BodySlide's zero-based value: 0 is the
				// zeroed slider, 1 is 100%. Written to both small and big.
				const std::string prefix = s.bodySlideHimbo ? "rsm_bodymorph_himbo" : "changemorphcbbe";
				std::vector<std::pair<std::string, double>> sliders;
				for (const auto& e : a_model.entries) {
					const auto cb = Text::Lower(e.callback);
					if (e.type == Defs::kTypeSlider && e.interval < 1.0 && cb.starts_with(prefix)) {
						sliders.emplace_back(e.callback.substr(prefix.size()), e.position);
					}
				}
				Bridge::ExportBodySlide(s.bodySlideHimbo, s.bodySlideName, std::move(sliders));
			}
			im::EndDisabled();
			W::SmallText(im::GetWindowDrawList(), im::GetCursorScreenPos(), Color::TextFaint, "Saved to CalienteTools\\BodySlide\\SliderPresets (MO2: Overwrite)", 0.75f);
			im::Dummy({ 1.0f, u * 0.85f });
		}

		void ToolsSection()
		{
			const float u = U();
			const float w = im::GetContentRegionAvail().x;
			if (W::Button("##native", Icon::Wrench, std::format("RaceMenu's own interface ({})", KeyName(Settings::Get().toggleKey)).c_str(), { w, 0.0f })) {
				Bridge::EnterNative();
			}
			if (W::Button("##sculpt", Icon::Wand, "Sculpt the mesh", { w, 0.0f })) {
				Bridge::EnterSculpt();
			}
			if (W::Button("##reread", Icon::Refresh, "Read the sliders again", { w, 0.0f })) {
				Bridge::RequestRefresh(0);
			}
			if (W::Button("##diag", Icon::Bug, "Write a diagnostic report", { w, 0.0f })) {
				Bridge::WriteDiagnostics();
			}
			im::Dummy({ 1.0f, u * 0.2f });
		}

		void RenderSidePanel(Model& a_model)
		{
			const auto* io = im::GetIO();
			const float target = TargetFontSize();
			const float margin = std::floor(target * 0.8f);
			if (!Settings::Get().sidePanel) {
				const float b = std::floor(target * 2.0f) + target * 1.0f;
				im::SetNextWindowPos({ io->DisplaySize.x - margin - b, margin });
				im::SetNextWindowSize({ b, b });
				if (im::Begin(kSideTabWindow, nullptr, ImGuiMCP::ImGuiWindowFlags_NoDecoration | ImGuiMCP::ImGuiWindowFlags_NoMove | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings)) {
					ApplyFont();
					if (W::IconButton("##open", Icon::Columns, "Show the scene and presets panel", false, std::floor(U() * 2.0f))) {
						Settings::Get().sidePanel = true;
						Settings::Get().Save();
					}
				}
				im::End();
				return;
			}

			const float width = std::floor(std::clamp(io->DisplaySize.x * 0.2f, target * 15.0f, target * 22.0f));
			im::SetNextWindowPos({ io->DisplaySize.x - margin - width, margin });
			im::SetNextWindowSizeConstraints({ width, 0.0f }, { width, io->DisplaySize.y - margin * 2.0f });
			if (!im::Begin(kSideWindow, nullptr,
					ImGuiMCP::ImGuiWindowFlags_NoTitleBar | ImGuiMCP::ImGuiWindowFlags_NoResize | ImGuiMCP::ImGuiWindowFlags_NoMove |
						ImGuiMCP::ImGuiWindowFlags_NoCollapse | ImGuiMCP::ImGuiWindowFlags_NoSavedSettings | ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
				im::End();
				return;
			}
			ApplyFont();
			const float u = U();
			const auto  pos = im::GetCursorScreenPos();
			W::SmallText(im::GetWindowDrawList(), pos, Color::Gold, "SCENE & PRESETS", 0.85f);
			im::SetCursorScreenPos({ pos.x + im::GetContentRegionAvail().x - u * 1.2f, pos.y - u * 0.15f });
			if (W::IconButton("##hide", Icon::Close, "Hide this panel", false, u * 1.2f)) {
				Settings::Get().sidePanel = false;
				Settings::Get().Save();
			}
			im::SetCursorScreenPos({ pos.x, pos.y + u * 1.2f });

			if (W::Collapsible("##scene", Icon::Person, "Scene", s.sceneOpen)) {
				SceneSection(a_model);
			}
			if (W::Collapsible("##presets", Icon::Folder, "Presets", s.presetsOpen)) {
				PresetsSection(a_model);
			}
			if (W::Collapsible("##bodyslide", Icon::Export, "BodySlide export", s.bodySlideOpen)) {
				BodySlideSection(a_model);
			}
			if (W::Collapsible("##tools", Icon::Wrench, "RaceMenu tools", s.toolsOpen)) {
				ToolsSection();
			}
			im::End();
		}

		// ------------------------------------------------------------------
		// input that is not tied to a widget

		void HandleHotkeys(Model& a_model)
		{
			const auto* io = im::GetIO();
			if (io->WantTextInput) {
				return;
			}
			const bool popup = AnyPopupOpen();
			if (io->KeyCtrl) {
				if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_Z, true)) {
					io->KeyShift ? a_model.Redo() : a_model.Undo();
				} else if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_Y, true)) {
					a_model.Redo();
				} else if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_F, false)) {
					s.focusSearch = true;
				}
				return;
			}
			if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_GraveAccent, false)) {
				Bridge::OpenConsole();
				return;
			}
			if (popup) {
				return;
			}
			if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_R, false)) {
				if (Settings::Get().confirmDone) {
					s.openDone = true;
				} else {
					Bridge::Done(s.name);
				}
			} else if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_Z, false)) {
				Bridge::Zoom(!a_model.zoomFace);
			} else if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_L, false)) {
				Bridge::SetLight(!a_model.lightOn);
			}
			const float dt = io->DeltaTime;
			if (im::IsKeyDown(ImGuiMCP::ImGuiKey_Q)) {
				Bridge::Rotate(-110.0f * dt);
			}
			if (im::IsKeyDown(ImGuiMCP::ImGuiKey_E)) {
				Bridge::Rotate(110.0f * dt);
			}
			// controller: triggers step through the categories
			if (im::IsKeyPressed(ImGuiMCP::ImGuiKey_GamepadL2, false) || im::IsKeyPressed(ImGuiMCP::ImGuiKey_GamepadR2, false)) {
				std::vector<const Category*> cats{ nullptr };
				for (std::size_t i = 0; i < a_model.categories.size(); ++i) {
					if (!IsAllCategory(a_model.categories[i]) && (i >= s.categoryCount.size() || s.categoryCount[i] > 0)) {
						cats.push_back(&a_model.categories[i]);
					}
				}
				const int active = ActiveCategory(a_model);
				int       at = 0;
				for (int i = 0; i < static_cast<int>(cats.size()); ++i) {
					if ((cats[i] == nullptr && active < 0) || (cats[i] && active >= 0 && cats[i] == &a_model.categories[active])) {
						at = i;
					}
				}
				const int dir = im::IsKeyPressed(ImGuiMCP::ImGuiKey_GamepadR2, false) ? 1 : -1;
				at = (at + dir + static_cast<int>(cats.size())) % static_cast<int>(cats.size());
				s.search[0] = '\0';
				SelectCategory(cats[at]);
			}
		}

		// dragging on empty space turns the character, the wheel zooms
		void HandleSceneMouse(Model& a_model)
		{
			const auto* io = im::GetIO();
			const bool  overUI = im::IsWindowHovered(ImGuiMCP::ImGuiHoveredFlags_AnyWindow) || im::IsAnyItemActive() || AnyPopupOpen();
			if (!s.rotating && !overUI && im::IsMouseClicked(0)) {
				s.rotating = true;
			}
			if (s.rotating) {
				if (im::IsMouseDown(0)) {
					if (io->MouseDelta.x != 0.0f) {
						Bridge::Rotate(io->MouseDelta.x * Settings::Get().rotateSpeed);
					}
				} else {
					s.rotating = false;
				}
			}
			if (!overUI && io->MouseWheel != 0.0f) {
				const bool face = io->MouseWheel > 0.0f;
				if (face != a_model.zoomFace) {
					Bridge::Zoom(face);
				}
			}
		}

		// ------------------------------------------------------------------
		// SKSE Menu Framework callbacks

		void __stdcall RenderWindows();
	}

	// both run on DevBench's listener thread; the model lock is the one every frame holds while it draws
	bool SelectCategoryNamed(const std::string& a_name)
	{
		auto&            model = Model::Get();
		std::scoped_lock lock(model.mutex);
		if (a_name.empty()) {
			SelectCategory(nullptr);
			return true;
		}
		const auto want = Text::Lower(a_name);
		for (const auto& c : model.categories) {
			if (Text::Lower(c.label) == want || Text::Lower(c.raw) == want) {
				SelectCategory(&c);
				return true;
			}
		}
		return false;
	}

	std::string CurrentCategory()
	{
		auto&            model = Model::Get();
		std::scoped_lock lock(model.mutex);
		return s.categoryRaw;
	}

	namespace
	{
		void __stdcall RenderWindows()
		{
			auto&             model = Model::Get();
			std::scoped_lock lock(model.mutex);
			if (model.mode != Mode::Editor) {
				return;
			}
			CompensateLock();
			SyncName(model);
			BuildItems(model);

			PushStyle();
			HandleHotkeys(model);
			RenderEditor(model);
			RenderSidePanel(model);
			HandleSceneMouse(model);
			PopStyle();
		}

		void DrawBanner(const char* a_icon, const std::string& a_text)
		{
			auto*       list = im::GetForegroundDrawList();
			const auto* io = im::GetIO();
			const float u = TargetFontSize();
			const auto  size = im::CalcTextSize(a_text.c_str());
			const float scale = u / std::max(1.0f, im::GetFontSize());
			const ImVec2 text{ size.x * scale, size.y * scale };
			const float  w = text.x + u * 3.0f;
			const float  h = text.y + u * 1.0f;
			const ImVec2 min{ (io->DisplaySize.x - w) * 0.5f, u * 0.8f };
			dl::AddRectFilled(list, min, min + ImVec2{ w, h }, Color::Window, h * 0.5f, 0);
			dl::AddRect(list, min, min + ImVec2{ w, h }, Color::BorderStrong, h * 0.5f, 0, 1.0f);
			PushIconFont();
			dl::AddText(list, im::GetFont(), u, min + ImVec2{ u * 0.9f, (h - u) * 0.5f }, Color::Gold, a_icon);
			PopIconFont();
			dl::AddText(list, im::GetFont(), u, min + ImVec2{ u * 2.2f, (h - text.y) * 0.5f }, Color::Text, a_text.c_str());
		}

		void __stdcall RenderHud()
		{
			Preview::Collect();
			auto&             model = Model::Get();
			std::scoped_lock lock(model.mutex);

			if (model.mode != Mode::Closed && !s.session) {
				StartSession();
			} else if (model.mode == Mode::Closed && s.session) {
				EndSession();
			}

			const bool want = model.mode == Mode::Editor && model.yieldCount == 0;
			if (s.window) {
				const bool open = s.window->IsOpen.load();
				if (open && !want) {
					ReleaseLock();
					s.window->IsOpen = false;
				} else if (!open && want) {
					s.window->IsOpen = true;
				}
			}

			if (model.yieldCount == 0) {
				const auto key = KeyName(Settings::Get().toggleKey);
				if (model.mode == Mode::Native) {
					DrawBanner(Icon::Wrench, std::format("RaceMenu's own interface  -  press {} to return to the Atelier", key));
				} else if (model.mode == Mode::Sculpt) {
					DrawBanner(Icon::Wand, std::format("Sculpting  -  press {} to return to the Atelier", key));
				}
			}
		}

		bool __stdcall OnInput(RE::InputEvent* a_event)
		{
			const auto button = a_event ? a_event->AsButtonEvent() : nullptr;
			if (!button || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard || !button->IsDown() ||
				button->GetIDCode() != Settings::Get().toggleKey || !Bridge::IsMenuOpen()) {
				return false;
			}
			Mode mode;
			int  yield;
			{
				auto&             model = Model::Get();
				std::scoped_lock lock(model.mutex);
				mode = model.mode;
				yield = model.yieldCount;
			}
			if (yield > 0) {
				return false;
			}
			switch (mode) {
			case Mode::Editor:
				Bridge::EnterNative();
				return true;
			case Mode::Native:
				Bridge::ExitNative();
				return true;
			case Mode::Sculpt:
				Bridge::ExitSculpt();
				return true;
			default:
				return false;
			}
		}
	}

	bool Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			return false;
		}
		s.window = SKSEMenuFramework::AddWindow(RenderWindows, true);
		if (!s.window) {
			return false;
		}
		s.window->IsOpen = false;
		SKSEMenuFramework::AddHudElement(RenderHud);
		SKSEMenuFramework::AddInputEvent(OnInput);
		s.registered = true;
		logger::info("registered with SKSE Menu Framework {:.2f}", SKSEMenuFramework::GetMenuFrameworkVersion());
		return true;
	}

	bool IsRegistered()
	{
		return s.registered;
	}
}
