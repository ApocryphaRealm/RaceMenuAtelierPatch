#include "Bridge.h"

#include <nlohmann/json.hpp>

namespace RMA::Bridge
{
	namespace
	{
		// AS path of RaceMenu's panel instance inside racesex_menu.swf
		constexpr const char* kPanels = "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance";
		// the folder RaceMenu's own PresetEditor.as uses
		constexpr const char* kPresetDir = "Data\\SKSE\\Plugins\\CharGen\\Presets\\";

		constexpr std::uint32_t kMaxEntries = 4000;
		constexpr std::uint32_t kMaxTextures = 2000;

		// ------------------------------------------------------------------
		// session

		// RaceMenu builds and destroys its movie asynchronously. A task queued by
		// one menu instance must never touch the next one (or a movie that is
		// being torn down), so every queued task carries the session it was
		// queued for.
		std::atomic<std::uint64_t> g_session{ 0 };

		bool IsCurrent(std::uint64_t a_session)
		{
			if (a_session == 0 || a_session != g_session.load(std::memory_order_acquire)) {
				return false;
			}
			const auto ui = RE::UI::GetSingleton();
			return ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
		}

		std::uint64_t Current()
		{
			const auto session = g_session.load(std::memory_order_acquire);
			return IsCurrent(session) ? session : 0;
		}

		template <class Fn>
		void QueueUI(Fn&& a_fn)
		{
			const auto session = Current();
			if (!session) {
				return;
			}
			SKSE::GetTaskInterface()->AddUITask([session, fn = std::forward<Fn>(a_fn)]() mutable {
				if (IsCurrent(session)) {
					fn();
				}
			});
		}

		template <class Fn>
		void QueueGame(Fn&& a_fn)
		{
			const auto session = Current();
			if (!session) {
				return;
			}
			SKSE::GetTaskInterface()->AddTask([session, fn = std::forward<Fn>(a_fn)]() mutable {
				if (IsCurrent(session)) {
					fn();
				}
			});
		}

		template <class Fn>
		void WithModel(Fn&& a_fn)
		{
			auto&             model = Model::Get();
			std::scoped_lock lock(model.mutex);
			a_fn(model);
		}

		void Status(std::string a_text)
		{
			logger::info("[status] {}", a_text);
			WithModel([&](Model& m) { m.SetStatus(std::move(a_text)); });
		}

		RE::GPtr<RE::GFxMovieView> Movie()
		{
			const auto ui = RE::UI::GetSingleton();
			return ui ? ui->GetMovieView(RE::RaceSexMenu::MENU_NAME) : nullptr;
		}

		std::string Path(const char* a_member)
		{
			return std::string(kPanels) + a_member;
		}

		// ------------------------------------------------------------------
		// GFx helpers

		double GetNum(const RE::GFxValue& a_obj, const char* a_name, double a_fallback = 0.0)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_name, &v) && v.IsNumber() ? v.GetNumber() : a_fallback;
		}

		bool GetBool(const RE::GFxValue& a_obj, const char* a_name, bool a_fallback = false)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_name, &v) && v.IsBool() ? v.GetBool() : a_fallback;
		}

		std::string GetStr(const RE::GFxValue& a_obj, const char* a_name)
		{
			RE::GFxValue v;
			return a_obj.GetMember(a_name, &v) && v.IsString() ? std::string(v.GetString()) : std::string{};
		}

		std::vector<std::string> GetStrArray(const RE::GFxValue& a_obj, const char* a_name)
		{
			std::vector<std::string> out;
			RE::GFxValue             v;
			if (a_obj.GetMember(a_name, &v) && v.IsArray()) {
				for (std::uint32_t i = 0; i < v.GetArraySize(); ++i) {
					RE::GFxValue element;
					if (v.GetElement(i, &element) && element.IsString()) {
						out.emplace_back(element.GetString());
					}
				}
			}
			return out;
		}

		bool InvokeGameDelegate(const char* a_callback, std::initializer_list<RE::GFxValue> a_args)
		{
			auto movie = Movie();
			if (!movie) {
				return false;
			}
			RE::GFxValue args[2];
			args[0] = a_callback;
			movie->CreateArray(&args[1]);
			for (const auto& arg : a_args) {
				args[1].PushBack(arg);
			}
			bool ok = movie->Invoke("_global.gfx.io.GameDelegate.call", nullptr, args, 2);
			if (!ok) {
				ok = movie->Invoke("gfx.io.GameDelegate.call", nullptr, args, 2);
			}
			if (!ok) {
				logger::warn("GameDelegate.call({}) failed", a_callback);
			}
			return ok;
		}

		void SendModEvent(const std::string& a_event, const std::string& a_str, float a_num)
		{
			SKSE::ModCallbackEvent event{ RE::BSFixedString(a_event), RE::BSFixedString(a_str), a_num, nullptr };
			SKSE::GetModCallbackEventSource()->SendEvent(&event);
		}

		// Keeps RaceMenu's own entry objects in step with what was applied, so its
		// native list (F4) and any later re-read show the real values.
		void WriteBack(const EntryRef& a_ref, const char* a_member, const RE::GFxValue& a_value)
		{
			auto movie = Movie();
			if (!movie || a_ref.swfIndex < 0) {
				return;
			}
			RE::GFxValue list;
			if (!movie->GetVariable(&list, Path(".racePanel.itemList.entryList").c_str()) || !list.IsArray() ||
				static_cast<std::uint32_t>(a_ref.swfIndex) >= list.GetArraySize()) {
				return;
			}
			RE::GFxValue entry;
			if (!list.GetElement(static_cast<std::uint32_t>(a_ref.swfIndex), &entry) || !entry.IsObject()) {
				return;
			}
			if (GetStr(entry, "callbackName") != a_ref.callback || GetNum(entry, "sliderID", -1.0) != a_ref.sliderID) {
				return;
			}
			entry.SetMember(a_member, a_value);
		}

		// ------------------------------------------------------------------
		// reading RaceMenu

		void ReadSnapshot(Snapshot& a_out)
		{
			auto movie = Movie();
			if (!movie) {
				return;
			}

			RE::GFxValue categories;
			if (movie->GetVariable(&categories, Path(".racePanel.slidingCategoryList.categoryList.entryList").c_str()) && categories.IsArray()) {
				for (std::uint32_t i = 0; i < categories.GetArraySize(); ++i) {
					RE::GFxValue e;
					if (!categories.GetElement(i, &e) || !e.IsObject()) {
						continue;
					}
					Category category;
					category.raw = GetStr(e, "text");
					category.flag = static_cast<std::uint32_t>(GetNum(e, "flag"));
					category.textFilter = GetStr(e, "textFilter");
					a_out.categories.push_back(std::move(category));
				}
			}

			RE::GFxValue list;
			if (movie->GetVariable(&list, Path(".racePanel.itemList.entryList").c_str()) && list.IsArray()) {
				const auto count = std::min(list.GetArraySize(), kMaxEntries);
				a_out.entries.reserve(count);
				for (std::uint32_t i = 0; i < count; ++i) {
					RE::GFxValue e;
					if (!list.GetElement(i, &e) || !e.IsObject()) {
						continue;
					}
					Entry entry;
					entry.swfIndex = static_cast<int>(i);
					entry.rawText = GetStr(e, "text");
					entry.type = static_cast<int>(GetNum(e, "type", -1));
					// -1: no filterFlag at all, which CategoryFilter treats as
					// "matches every category" (most skee-injected sliders)
					entry.filterFlag = static_cast<std::int64_t>(GetNum(e, "filterFlag", -1));
					entry.callback = GetStr(e, "callbackName");
					entry.sliderID = GetNum(e, "sliderID", -1);
					entry.min = GetNum(e, "sliderMin");
					entry.max = GetNum(e, "sliderMax", 1.0);
					entry.interval = GetNum(e, "interval", 0.1);
					entry.position = GetNum(e, "position");
					entry.enabled = GetBool(e, "enabled", true);
					entry.textFilters = GetStrArray(e, "textFilters");
					entry.tintType = static_cast<int>(GetNum(e, "tintType", -1));
					entry.tintIndex = static_cast<int>(GetNum(e, "tintIndex", 0));
					entry.fillColor = static_cast<std::uint32_t>(static_cast<std::int64_t>(GetNum(e, "fillColor", 0)));
					entry.raceID = static_cast<int>(GetNum(e, "raceID", -1));
					entry.texture = GetStr(e, "texture");
					entry.listType = static_cast<int>(GetNum(e, "listType", -1));
					entry.description = GetStr(e, "raceDescription");
					if (!entry.enabled || entry.rawText.empty() || entry.type < 1 || entry.type > 7) {
						continue;
					}
					a_out.entries.push_back(std::move(entry));
				}
			}

			// what the bottom bar shows as chosen for lists other mods add (Apprentice: ClassValue, TraitValue)
			RE::GFxValue playerInfo;
			if (movie->GetVariable(&playerInfo, Path(".bottomBar.playerInfo").c_str()) && playerInfo.IsObject()) {
				for (const auto field : { "ClassValue", "TraitValue" }) {
					RE::GFxValue text;
					if (playerInfo.GetMember(field, &text) && text.IsObject()) {
						if (auto value = GetStr(text, "text"); !value.empty()) {
							a_out.picked.push_back(std::move(value));
						}
					}
				}
			}

			// texture catalogs for the five paint kinds (war, body, hand, feet, face)
			RE::GFxValue makeup;
			if (movie->GetVariable(&makeup, Path(".makeupList").c_str()) && makeup.IsArray()) {
				for (std::uint32_t i = 0; i < makeup.GetArraySize() && i < a_out.makeup.size(); ++i) {
					RE::GFxValue sub;
					if (!makeup.GetElement(i, &sub) || !sub.IsArray()) {
						continue;
					}
					for (std::uint32_t j = 0; j < sub.GetArraySize() && j < kMaxTextures; ++j) {
						RE::GFxValue t;
						if (!sub.GetElement(j, &t) || !t.IsObject()) {
							continue;
						}
						a_out.makeup[i].push_back({ GetStr(t, "text"), GetStr(t, "texture"), {}, {} });
					}
				}
			}
		}

		PlayerInfo ReadPlayer()
		{
			PlayerInfo info;
			if (const auto player = RE::PlayerCharacter::GetSingleton()) {
				if (const auto name = player->GetName(); name && *name) {
					info.name = name;
				}
				if (const auto race = player->GetRace()) {
					if (const auto raceName = race->GetFullName(); raceName && *raceName) {
						info.race = raceName;
					}
				}
				if (const auto base = player->GetActorBase()) {
					info.sex = base->GetSex() == RE::SEX::kFemale ? 1 : (base->GetSex() == RE::SEX::kMale ? 0 : -1);
				}
			}
			return info;
		}

		void PushSnapshot()
		{
			Snapshot snapshot;
			ReadSnapshot(snapshot);
			auto player = ReadPlayer();
			const auto count = snapshot.entries.size();
			WithModel([&](Model& m) {
				m.ApplySnapshot(std::move(snapshot));
				m.ApplyPlayer(std::move(player));
			});
			logger::info("read {} RaceMenu entries", count);
		}

		// ------------------------------------------------------------------
		// presentation: what of RaceMenu's own UI is visible

		void SetSwfVisible(bool a_visible)
		{
			if (auto movie = Movie()) {
				movie->SetVariable("_root.RaceSexMenuBaseInstance._visible", RE::GFxValue(a_visible));
			}
		}

		void SetCursorVisible(bool a_visible)
		{
			const auto ui = RE::UI::GetSingleton();
			if (auto cursor = ui ? ui->GetMovieView(RE::CursorMenu::MENU_NAME) : nullptr) {
				cursor->SetVisible(a_visible);
			}
		}

		// Called on the UI thread whenever the mode or the yield count changes.
		void ApplyPresentation()
		{
			Mode mode;
			int  yield;
			{
				auto&             model = Model::Get();
				std::scoped_lock lock(model.mutex);
				mode = model.mode;
				yield = model.yieldCount;
			}
			const bool editor = mode == Mode::Editor;
			SetSwfVisible(!editor);
			// ImGui draws its own cursor while the editor owns the input
			SetCursorVisible(!editor || yield > 0);
		}

		void SetMode(Mode a_mode)
		{
			WithModel([&](Model& m) { m.mode = a_mode; });
			QueueUI([] { ApplyPresentation(); });
		}

		// ------------------------------------------------------------------
		// freeze: stop the animation graph and blinking, restored on close

		struct FreezeState
		{
			bool  animGraphUpdate{ true };
			bool  aiEnabled{ true };
			bool  notPushable{ false };
			bool  recordHits{ true };
			bool  hitFlags{ true };
			float blinkDelay{ 0.0f };
		};

		std::atomic<bool> g_frozen{ false };
		FreezeState       g_freeze;

		void MaintainFrozenFace()
		{
			if (!g_frozen.load(std::memory_order_acquire)) {
				return;
			}
			if (const auto player = RE::PlayerCharacter::GetSingleton()) {
				if (const auto face = player->GetFaceGenAnimationData()) {
					face->eyesBlinkingTimer = std::numeric_limits<float>::max();
				}
			}
		}

		void SetFrozen(bool a_frozen)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (!player || a_frozen == g_frozen.load(std::memory_order_acquire)) {
				return;
			}
			auto& data = player->GetActorRuntimeData();
			if (a_frozen) {
				g_freeze.animGraphUpdate = data.boolFlags.all(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);
				g_freeze.aiEnabled = player->IsAIEnabled();
				data.boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);
				if (const auto controller = player->GetCharController()) {
					g_freeze.notPushable = controller->flags.all(RE::CHARACTER_FLAGS::kNotPushable);
					g_freeze.recordHits = controller->flags.all(RE::CHARACTER_FLAGS::kRecordHits);
					g_freeze.hitFlags = controller->flags.all(RE::CHARACTER_FLAGS::kHitFlags);
					controller->flags.set(RE::CHARACTER_FLAGS::kNotPushable);
					controller->flags.reset(RE::CHARACTER_FLAGS::kRecordHits, RE::CHARACTER_FLAGS::kHitFlags);
				}
				player->EnableAI(false);
				player->StopMoving(1.0f);
				if (const auto face = player->GetFaceGenAnimationData()) {
					g_freeze.blinkDelay = face->eyesBlinkingTimer;
				}
				g_frozen.store(true, std::memory_order_release);
				MaintainFrozenFace();
			} else {
				if (g_freeze.animGraphUpdate) {
					data.boolFlags.set(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);
				} else {
					data.boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);
				}
				if (const auto controller = player->GetCharController()) {
					const auto restore = [&](RE::CHARACTER_FLAGS a_flag, bool a_on) {
						a_on ? controller->flags.set(a_flag) : controller->flags.reset(a_flag);
					};
					restore(RE::CHARACTER_FLAGS::kNotPushable, g_freeze.notPushable);
					restore(RE::CHARACTER_FLAGS::kRecordHits, g_freeze.recordHits);
					restore(RE::CHARACTER_FLAGS::kHitFlags, g_freeze.hitFlags);
				}
				player->EnableAI(g_freeze.aiEnabled);
				if (const auto face = player->GetFaceGenAnimationData()) {
					face->eyesBlinkingTimer = g_freeze.blinkDelay;
				}
				g_frozen.store(false, std::memory_order_release);
			}
			WithModel([&](Model& m) { m.frozen = a_frozen; });
			logger::info("player animation {}", a_frozen ? "frozen" : "restored");
		}

		// ------------------------------------------------------------------
		// undress

		std::vector<RE::TESBoundObject*> g_stash;

		void Undress(bool a_undress)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto equip = RE::ActorEquipManager::GetSingleton();
			if (!player || !equip) {
				return;
			}
			if (a_undress) {
				g_stash.clear();
				for (auto& [object, data] : player->GetInventory()) {
					if (object && object->IsArmor() && data.second && data.second->IsWorn()) {
						g_stash.push_back(object);
					}
				}
				for (auto* object : g_stash) {
					equip->UnequipObject(player, object, nullptr, 1, nullptr, false, false, false, true);
				}
			} else {
				for (auto* object : g_stash) {
					equip->EquipObject(player, object, nullptr, 1, nullptr, false, false, false, true);
				}
				g_stash.clear();
			}
			// the menu pauses the world; rebuild the biped or the change only shows after closing
			player->Update3DModel();
		}

		// ------------------------------------------------------------------
		// refresh scheduling and the watcher

		std::mutex          g_refreshLock;
		std::vector<double> g_refreshAt;

		struct WatchState
		{
			std::uint32_t lastSeen{ 0 };
			std::uint32_t lastPushed{ 0 };
			std::uint32_t lastMakeupSeen{ 0 };
			std::uint32_t lastMakeupPushed{ 0 };
			std::string   lastRace;
			int           lastSex{ -2 };
		};
		WatchState g_watch;

		bool RefreshDue()
		{
			std::scoped_lock lock(g_refreshLock);
			const double     now = Text::Now();
			const auto       before = g_refreshAt.size();
			std::erase_if(g_refreshAt, [now](double t) { return t <= now; });
			return g_refreshAt.size() != before;
		}

		// Vanilla sliders exist when the menu opens, but skee / CBBE / 3BA / HIMBO
		// customs are injected by Papyrus seconds later, and a race change rebuilds
		// the whole list. Push when a count has settled on a new value, when a
		// refresh was asked for, or when the character's race or sex changed.
		void Poll()
		{
			MaintainFrozenFace();
			auto movie = Movie();
			if (!movie) {
				return;
			}
			std::uint32_t count = 0;
			std::uint32_t makeupTotal = 0;
			RE::GFxValue  list;
			if (movie->GetVariable(&list, Path(".racePanel.itemList.entryList").c_str()) && list.IsArray()) {
				count = list.GetArraySize();
			}
			RE::GFxValue makeup;
			if (movie->GetVariable(&makeup, Path(".makeupList").c_str()) && makeup.IsArray()) {
				for (std::uint32_t i = 0; i < makeup.GetArraySize(); ++i) {
					RE::GFxValue sub;
					if (makeup.GetElement(i, &sub) && sub.IsArray()) {
						makeupTotal += sub.GetArraySize();
					}
				}
			}
			const auto player = ReadPlayer();

			auto&      w = g_watch;
			const bool slidersSettled = count > 0 && count == w.lastSeen && count != w.lastPushed;
			const bool makeupSettled = makeupTotal > 0 && makeupTotal == w.lastMakeupSeen && makeupTotal != w.lastMakeupPushed;
			const bool characterChanged = w.lastSex != -2 && (player.race != w.lastRace || player.sex != w.lastSex);
			const bool due = RefreshDue();

			if (slidersSettled || makeupSettled || due || characterChanged) {
				PushSnapshot();
				w.lastPushed = count;
				w.lastMakeupPushed = makeupTotal;
				if (characterChanged) {
					// the rebuilt list often settles a moment later
					RequestRefresh(700);
				}
			}
			w.lastSeen = count;
			w.lastMakeupSeen = makeupTotal;
			w.lastRace = player.race;
			w.lastSex = player.sex;
		}

		void WatcherLoop()
		{
			for (;;) {
				std::this_thread::sleep_for(250ms);
				const auto session = Current();
				if (!session) {
					continue;
				}
				SKSE::GetTaskInterface()->AddUITask([session] {
					if (IsCurrent(session)) {
						Poll();
					}
				});
			}
		}

		// ------------------------------------------------------------------
		// menu events

		bool TakesInput(const RE::BSFixedString& a_menu)
		{
			using Flag = RE::UI_MENU_FLAGS;
			const auto ui = RE::UI::GetSingleton();
			auto       menu = ui ? ui->GetMenu(a_menu.c_str()) : nullptr;
			if (a_menu == RE::Console::MENU_NAME || a_menu == RE::MessageBoxMenu::MENU_NAME) {
				return true;
			}
			return menu && menu->menuFlags.any(Flag::kPausesGame, Flag::kUsesCursor, Flag::kModal);
		}

		std::set<std::string> g_yielding;

		class MenuWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static MenuWatcher* GetSingleton()
			{
				static MenuWatcher singleton;
				return &singleton;
			}

			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto& name = a_event->menuName;
				if (name == RE::RaceSexMenu::MENU_NAME) {
					a_event->opening ? OnOpen() : OnClose();
				} else if (name != RE::CursorMenu::MENU_NAME && name != RE::HUDMenu::MENU_NAME) {
					OnOtherMenu(name, a_event->opening);
				}
				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			static void OnOpen()
			{
				logger::info("RaceSex Menu opened");
				const auto session = g_session.fetch_add(1, std::memory_order_acq_rel) + 1;
				g_watch = {};
				g_yielding.clear();
				{
					std::scoped_lock lock(g_refreshLock);
					g_refreshAt.clear();
				}
				WithModel([](Model& m) {
					m.ResetSession();
					m.mode = Mode::Editor;
				});
				// hide RaceMenu's own panels at once, the data follows over a few frames
				SKSE::GetTaskInterface()->AddUITask([session] {
					if (IsCurrent(session)) {
						ApplyPresentation();
						PushSnapshot();
					}
				});
			}

			static void OnClose()
			{
				logger::info("RaceSex Menu closed");
				// invalidate every queued task before anything else
				g_session.fetch_add(1, std::memory_order_acq_rel);
				g_yielding.clear();
				if (g_frozen.load(std::memory_order_acquire)) {
					SKSE::GetTaskInterface()->AddTask([] { SetFrozen(false); });
				}
				g_stash.clear();
				WithModel([](Model& m) {
					m.mode = Mode::Closed;
					m.ResetSession();
				});
				SetCursorVisible(true);
			}

			static void OnOtherMenu(const RE::BSFixedString& a_name, bool a_opening)
			{
				if (!Current()) {
					return;
				}
				const std::string name(a_name.c_str());
				if (a_opening) {
					if (!TakesInput(a_name) || !g_yielding.insert(name).second) {
						return;
					}
					logger::info("{} opened over RaceMenu, yielding input", name);
				} else if (g_yielding.erase(name) == 0) {
					return;
				}
				const int yield = static_cast<int>(g_yielding.size());
				WithModel([yield](Model& m) { m.yieldCount = yield; });
				ApplyPresentation();
			}
		};

		// ------------------------------------------------------------------
		// presets

		bool IsSafePresetPath(const std::string& a_name)
		{
			if (a_name.empty() || a_name.size() > 240 || a_name.find('\0') != std::string::npos) {
				return false;
			}
			const std::filesystem::path path(a_name);
			if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
				return false;
			}
			for (const auto& part : path) {
				if (part == "." || part == "..") {
					return false;
				}
			}
			const auto ext = Text::Lower(path.extension().string());
			return ext == ".jslot" || ext == ".slot";
		}

		bool IsSafeSliderName(const std::string& a_name)
		{
			return !a_name.empty() && a_name.size() <= 128 && std::ranges::all_of(a_name, [](unsigned char c) {
				return std::isalnum(c) || c == '_' || c == '-' || c == ' ';
			});
		}

		std::string XmlEscape(const std::string& a_text)
		{
			std::string out;
			for (const char c : a_text) {
				switch (c) {
				case '&':
					out += "&amp;";
					break;
				case '<':
					out += "&lt;";
					break;
				case '>':
					out += "&gt;";
					break;
				case '"':
					out += "&quot;";
					break;
				case '\'':
					out += "&apos;";
					break;
				default:
					out += c;
				}
			}
			return out;
		}

		bool PresetCallFailed(const RE::GFxValue& a_result)
		{
			// like the SWF: a falsy return value means success
			return a_result.IsBool() ? a_result.GetBool() : (a_result.IsNumber() && a_result.GetNumber() != 0);
		}

		std::vector<std::string> ListPresetFiles()
		{
			std::vector<std::string>    files;
			const std::filesystem::path root(kPresetDir);
			std::error_code             ec;
			std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
			for (const std::filesystem::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
				if (ec || !it->is_regular_file(ec)) {
					continue;
				}
				auto name = it->path().lexically_relative(root).generic_string();
				if (IsSafePresetPath(name)) {
					files.emplace_back(std::move(name));
				}
			}
			std::ranges::sort(files, [](const std::string& a, const std::string& b) { return Text::Lower(a) < Text::Lower(b); });
			return files;
		}
	}

	// ======================================================================

	void Install()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuWatcher::GetSingleton());
		}
		std::thread(WatcherLoop).detach();
	}

	bool IsMenuOpen()
	{
		return Current() != 0;
	}

	void RequestRefresh(int a_delayMs)
	{
		std::scoped_lock lock(g_refreshLock);
		g_refreshAt.push_back(Text::Now() + a_delayMs / 1000.0);
	}

	// Replays a change exactly like RaceMenu's SliderListEntry does:
	// GameDelegate.call is the real apply path for every slider, vanilla and
	// custom alike (shared callbacks such as ChangeDoubleMorph are told apart by
	// sliderID on the C++ side). RSM_SliderChange is RaceMenu's Papyrus-side
	// bookkeeping notification, sent afterwards as the SWF does.
	void SetValue(const EntryRef& a_ref, double a_value)
	{
		QueueUI([ref = a_ref, a_value] {
			InvokeGameDelegate(ref.callback.c_str(), { RE::GFxValue(a_value), RE::GFxValue(ref.sliderID) });
			SendModEvent("RSM_SliderChange", ref.callback, static_cast<float>(a_value));
			WriteBack(ref, "position", RE::GFxValue(a_value));
			if (ref.callback == "ChangeSex") {
				RequestRefresh(400);
				RequestRefresh(1500);
			}
		});
	}

	void SetColor(const EntryRef& a_ref, ColorKind a_kind, int a_slot, std::uint32_t a_argb)
	{
		// The SWF composes the colour with AS2 bitwise ops, which are signed
		// int32, and Papyrus parses the string as a signed int as well.
		const auto signedColor = std::to_string(static_cast<std::int32_t>(a_argb));
		QueueUI([ref = a_ref, a_kind, a_slot, a_argb, signedColor] {
			switch (a_kind) {
			case ColorKind::Hair:
				SendModEvent("RSM_HairColorChange", signedColor, 0.0f);
				break;
			case ColorKind::Overlay:
				SendModEvent("RSM_OverlayColorChange", signedColor, static_cast<float>(a_slot));
				break;
			case ColorKind::Tint:
				SendModEvent("RSM_TintColorChange", signedColor, static_cast<float>(a_slot));
				break;
			}
			WriteBack(ref, "fillColor", RE::GFxValue(static_cast<double>(a_argb)));
		});
	}

	void SetTexture(const EntryRef& a_ref, bool a_overlay, int a_slot, const std::string& a_texture)
	{
		QueueUI([ref = a_ref, a_overlay, a_slot, a_texture] {
			SendModEvent(a_overlay ? "RSM_OverlayTextureChange" : "RSM_TintTextureChange", a_texture, static_cast<float>(a_slot));
			if (a_overlay) {
			}
			WriteBack(ref, "texture", RE::GFxValue(a_texture.c_str()));
		});
	}

	void PressEntry(const EntryRef& a_ref)
	{
		// what a click on the row does in RaceMenu's own list: onItemPress({index}) on the panels instance. A mod
		// that adds rows (Apprentice) replaces that handler, so its pick is recorded exactly as from its own UI.
		QueueUI([a_ref] {
			auto movie = Movie();
			if (!movie || a_ref.swfIndex < 0) {
				return;
			}
			RE::GFxValue list;
			RE::GFxValue entry;
			if (!movie->GetVariable(&list, Path(".racePanel.itemList.entryList").c_str()) || !list.IsArray() ||
				static_cast<std::uint32_t>(a_ref.swfIndex) >= list.GetArraySize() ||
				!list.GetElement(static_cast<std::uint32_t>(a_ref.swfIndex), &entry) || !entry.IsObject() ||
				GetStr(entry, "callbackName") != a_ref.callback) {
				logger::warn("PressEntry: entry {} ({}) moved, not pressed", a_ref.swfIndex, a_ref.callback);
				return;
			}
			RE::GFxValue panels;
			if (!movie->GetVariable(&panels, kPanels) || !panels.IsObject()) {
				return;
			}
			RE::GFxValue event;
			movie->CreateObject(&event);
			event.SetMember("index", RE::GFxValue(static_cast<double>(a_ref.swfIndex)));
			event.SetMember("entry", entry);
			const bool ok = panels.Invoke("onItemPress", nullptr, &event, 1);
			logger::info("PressEntry: onItemPress({}) for {} {}", a_ref.swfIndex, a_ref.callback, ok ? "sent" : "FAILED");
		});
		RequestRefresh(250);
	}

	void ChangeRace(int a_raceID)
	{
		// the call the SWF makes in onItemPress: ChangeRace(raceID, -1)
		QueueUI([a_raceID] {
			InvokeGameDelegate("ChangeRace", { RE::GFxValue(static_cast<double>(a_raceID)), RE::GFxValue(-1.0) });
			SendModEvent("RSM_SliderChange", "ChangeRace", static_cast<float>(a_raceID));
			RequestRefresh(500);
			RequestRefresh(1500);
			RequestRefresh(3000);
		});
	}

	void QuerySliderInfo(double a_sliderID, double a_value)
	{
		QueueUI([a_sliderID, a_value] {
			auto movie = Movie();
			if (!movie) {
				return;
			}
			RE::GFxValue result;
			RE::GFxValue args[2]{ a_sliderID, a_value };
			if (!movie->Invoke("_global.skse.plugins.CharGen.GetSliderData", &result, args, 2) || !result.IsObject()) {
				return;
			}
			auto name = GetStr(result, "partName");
			WithModel([&](Model& m) { m.partNames[a_sliderID] = std::move(name); });
		});
	}

	// The data behind RaceMenu's own parts panel; `source` is the plugin filter
	// its hair browser shows.
	void QueryHeadParts(double a_sliderID)
	{
		WithModel([&](Model& m) {
			m.headPartSlider = a_sliderID;
			m.headParts.clear();
			m.headPartsLoaded = false;
		});
		QueueUI([a_sliderID] {
			auto movie = Movie();
			if (!movie) {
				return;
			}
			std::vector<HeadPart> parts;
			RE::GFxValue          result;
			RE::GFxValue          args[1]{ a_sliderID };
			if (movie->Invoke("_global.skse.plugins.CharGen.GetSliderPartData", &result, args, 1) && result.IsObject()) {
				RE::GFxValue list;
				if (result.GetMember("parts", &list) && list.IsArray()) {
					for (std::uint32_t i = 0; i < list.GetArraySize(); ++i) {
						RE::GFxValue part;
						if (!list.GetElement(i, &part) || !part.IsObject()) {
							continue;
						}
						HeadPart hp;
						hp.name = GetStr(part, "name");
						if (hp.name.empty()) {
							continue;
						}
						hp.index = static_cast<int>(GetNum(part, "index", i));
						hp.plugin = GetStr(part, "source");
						if (hp.plugin.empty()) {
							hp.plugin = "Unknown source";
						}
						hp.label = Text::Humanise(hp.name, true);
						hp.search = Text::Lower(std::format("{} {} {} {}", hp.index, hp.name, hp.label, hp.plugin));
						parts.push_back(std::move(hp));
					}
				}
			}
			WithModel([&](Model& m) {
				if (m.headPartSlider == a_sliderID) {
					m.headParts = std::move(parts);
					m.headPartsLoaded = true;
				}
			});
		});
	}

	// ----------------------------------------------------------------------
	// scene

	// Rotates the loaded 3D root, as skee's SetPlayerRotation does. Changing the
	// game heading only turns the head, because head tracking compensates.
	void Rotate(float a_degrees)
	{
		if (a_degrees == 0.0f) {
			return;
		}
		QueueGame([a_degrees] {
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto root = player ? player->Get3D(false) : nullptr;
			if (!root) {
				return;
			}
			const float   rad = a_degrees * 0.017453292f;
			const float   c = std::cos(rad);
			const float   s = std::sin(rad);
			RE::NiMatrix3 rz;
			rz.entry[0][0] = c;
			rz.entry[0][1] = -s;
			rz.entry[0][2] = 0.0f;
			rz.entry[1][0] = s;
			rz.entry[1][1] = c;
			rz.entry[1][2] = 0.0f;
			rz.entry[2][0] = 0.0f;
			rz.entry[2][1] = 0.0f;
			rz.entry[2][2] = 1.0f;
			root->local.rotate = root->local.rotate * rz;
			RE::NiUpdateData ctx;
			root->UpdateWorldData(&ctx);
		});
	}

	void Zoom(bool a_face)
	{
		WithModel([&](Model& m) { m.zoomFace = a_face; });
		QueueUI([a_face] { InvokeGameDelegate("ZoomPC", { RE::GFxValue(a_face) }); });
	}

	void SetLight(bool a_on)
	{
		WithModel([&](Model& m) { m.lightOn = a_on; });
		QueueUI([a_on] { SendModEvent("RSM_ToggleLight", "", a_on ? 1.0f : 0.0f); });
	}

	void SetUndressed(bool a_undressed)
	{
		WithModel([&](Model& m) { m.undressed = a_undressed; });
		QueueGame([a_undressed] { Undress(a_undressed); });
	}

	void ToggleFreeze()
	{
		QueueGame([] { SetFrozen(!g_frozen.load(std::memory_order_acquire)); });
	}

	void PlayPose(const std::string& a_event)
	{
		QueueGame([a_event] {
			SetFrozen(false);
			if (const auto player = RE::PlayerCharacter::GetSingleton()) {
				const bool ok = player->NotifyAnimationGraph(a_event);
				if (!ok) {
					Status("That pose is not available for this character");
				}
			}
		});
	}

	// ----------------------------------------------------------------------
	// presets

	bool IsSafePresetName(const std::string& a_name)
	{
		if (a_name.empty() || a_name.size() > 80 || a_name.find("..") != std::string::npos) {
			return false;
		}
		return std::ranges::all_of(a_name, [](unsigned char c) {
			return std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '(' || c == ')' || c == '.' || c >= 0x80;
		});
	}

	// RaceMenu's GetExternalFiles only lists the root folder, while preset packs
	// usually sort characters into subfolders.
	void ListPresets()
	{
		QueueGame([] {
			auto files = ListPresetFiles();
			WithModel([&](Model& m) {
				m.presetFiles = std::move(files);
				m.presetsLoaded = true;
			});
		});
	}

	void SavePreset(const std::string& a_name)
	{
		if (!IsSafePresetName(a_name)) {
			Status("Invalid preset name");
			return;
		}
		QueueUI([a_name] {
			auto movie = Movie();
			if (!movie) {
				return;
			}
			const auto   path = std::string(kPresetDir) + a_name + ".jslot";
			RE::GFxValue result;
			RE::GFxValue args[2]{ path.c_str(), true };
			movie->Invoke("_global.skse.plugins.CharGen.SavePreset", &result, args, 2);
			const bool failed = PresetCallFailed(result);
			logger::info("SavePreset({}) failed={}", path, failed);
			Status(failed ? "Could not save the preset" : "Saved preset " + a_name);
			if (!failed) {
				ListPresets();
			}
		});
	}

	// After a successful load the SWF's follow-up is replayed (onLoadPreset +
	// ReloadSliders): skee applied morphs and head parts, but tints and hair
	// colour are applied by the UI side, and the slider list has to be rebuilt.
	void LoadPreset(const std::string& a_relativePath)
	{
		if (!IsSafePresetPath(a_relativePath)) {
			Status("Invalid preset path");
			return;
		}
		const auto session = Current();
		QueueUI([a_relativePath, session] {
			auto movie = Movie();
			if (!movie) {
				return;
			}
			const auto   path = std::string(kPresetDir) + a_relativePath;
			const bool   jslot = Text::Lower(path).ends_with(".jslot");
			RE::GFxValue dataOut;
			movie->CreateObject(&dataOut);
			RE::GFxValue result;
			RE::GFxValue args[3]{ path.c_str(), dataOut, jslot };
			movie->Invoke("_global.skse.plugins.CharGen.LoadPreset", &result, args, 3);
			const bool failed = PresetCallFailed(result);
			logger::info("LoadPreset({}) failed={}", path, failed);
			if (failed) {
				Status("Could not load " + a_relativePath);
				return;
			}
			const auto hairColor = static_cast<std::int32_t>(static_cast<std::int64_t>(GetNum(dataOut, "hairColor")));
			SendModEvent("RSM_RequestTintSave", "", 0.0f);
			Status("Loaded " + a_relativePath);
			std::thread([hairColor, session] {
				std::this_thread::sleep_for(500ms);
				SKSE::GetTaskInterface()->AddUITask([hairColor, session] {
					if (!IsCurrent(session)) {
						return;
					}
					if (auto m2 = Movie()) {
						m2->Invoke("_global.skse.plugins.CharGen.ReloadSliders", nullptr, nullptr, 0);
					}
					SendModEvent("RSM_RequestTintLoad", "", 0.0f);
					SendModEvent("RSM_HairColorChange", std::to_string(hairColor), 0.0f);
					// positions change without the entry count changing
					RequestRefresh(1200);
					RequestRefresh(2500);
				});
			}).detach();
		});
	}

	// .jslot is JSON: show the mods, head parts and tint textures it uses, and
	// which of those plugins are missing from the load order.
	void ReadPresetInfo(const std::string& a_relativePath)
	{
		if (!IsSafePresetPath(a_relativePath)) {
			return;
		}
		WithModel([&](Model& m) { m.presetInfo[a_relativePath] = {}; });
		QueueGame([a_relativePath] {
			PresetInfo info;
			info.loaded = true;
			std::string content;
			if (const auto wide = SKSE::stl::utf8_to_utf16(a_relativePath)) {
				std::ifstream in(std::filesystem::path(L"Data\\SKSE\\Plugins\\CharGen\\Presets") / *wide, std::ios::binary);
				if (in) {
					content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
				}
			}
			const auto json = nlohmann::json::parse(content, nullptr, false);
			if (!json.is_discarded() && json.is_object()) {
				info.readable = true;
				const auto data = RE::TESDataHandler::GetSingleton();
				if (const auto it = json.find("mods"); it != json.end() && it->is_array()) {
					for (const auto& mod : *it) {
						if (mod.is_object() && mod.contains("name") && mod["name"].is_string()) {
							auto name = mod["name"].get<std::string>();
							if (data && !data->LookupModByName(name)) {
								info.missing.push_back(name);
							}
							info.mods.push_back(std::move(name));
						}
					}
				}
				static constexpr std::array kPartTypes{ "Misc", "Face", "Eyes", "Hair", "Facial hair", "Scar", "Eyebrows" };
				if (const auto it = json.find("headParts"); it != json.end() && it->is_array()) {
					for (const auto& part : *it) {
						if (!part.is_object()) {
							continue;
						}
						const int   type = part.value("type", -1);
						std::string id = part.value("formIdentifier", std::string{});
						const auto  plugin = id.substr(0, id.find('|'));
						const auto  typeName = type >= 0 && type < static_cast<int>(kPartTypes.size()) ? std::string(kPartTypes[type]) : std::format("Type {}", type);
						info.headParts.push_back(std::format("{} - {}", typeName, plugin.empty() ? "?" : plugin));
					}
				}
				for (const auto* key : { "tintInfo", "tints" }) {
					if (const auto it = json.find(key); it != json.end() && it->is_array()) {
						for (const auto& tint : *it) {
							if (tint.is_object()) {
								info.tints.push_back(tint.value("texture", std::string{}));
							}
						}
						break;
					}
				}
			}
			WithModel([&](Model& m) { m.presetInfo[a_relativePath] = std::move(info); });
		});
	}

	// Writes under Data, which MO2 redirects into Overwrite while BodySlide is
	// launched through MO2 too. The value is RaceMenu's morph on BodySlide's
	// zero-based scale; it is deliberately not clamped so overdrive survives.
	void ExportBodySlide(bool a_himbo, const std::string& a_name, std::vector<std::pair<std::string, double>> a_sliders)
	{
		if (!IsSafePresetName(a_name)) {
			Status("Invalid BodySlide preset name");
			return;
		}
		std::erase_if(a_sliders, [](const auto& s) { return !IsSafeSliderName(s.first) || !std::isfinite(s.second); });
		if (a_sliders.empty()) {
			Status(a_himbo ? "No HIMBO morphs on this character" : "No CBBE 3BA morphs on this character");
			return;
		}
		SKSE::GetTaskInterface()->AddTask([a_himbo, a_name, sliders = std::move(a_sliders)] {
			const std::string flavor = a_himbo ? "HIMBO" : "CBBE 3BA";
			const std::string set = a_himbo ? "HIMBO" : "CBBE 3BBB Body Amazing";
			const auto        outPath = std::filesystem::path("Data\\CalienteTools\\BodySlide\\SliderPresets") /
			                     std::filesystem::path(SKSE::stl::utf8_to_utf16("RaceMenu Atelier - " + a_name + " [" + flavor + "].xml").value_or(L"RaceMenu Atelier.xml"));
			try {
				std::filesystem::create_directories(outPath.parent_path());
				std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
				if (!out) {
					Status("Could not write the BodySlide preset");
					return;
				}
				out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SliderPresets>\n";
				out << "  <Preset name=\"" << XmlEscape(a_name) << "\" set=\"" << set << "\">\n";
				if (a_himbo) {
					out << "    <Group name=\"HIMBO\"/>\n";
				} else {
					out << "    <Group name=\"CBBE\"/>\n    <Group name=\"3BBB\"/>\n    <Group name=\"3BA\"/>\n";
				}
				for (const auto& [slider, morph] : sliders) {
					const double value = morph * 100.0;
					out << "    <SetSlider name=\"" << XmlEscape(slider) << "\" size=\"small\" value=\"" << value << "\"/>\n";
					out << "    <SetSlider name=\"" << XmlEscape(slider) << "\" size=\"big\" value=\"" << value << "\"/>\n";
				}
				out << "  </Preset>\n</SliderPresets>\n";
				logger::info("exported {} BodySlide sliders to {}", sliders.size(), outPath.string());
				Status(std::format("Saved {} BodySlide preset ({} sliders)", flavor, sliders.size()));
			} catch (const std::exception& e) {
				logger::warn("BodySlide export failed: {}", e.what());
				Status("BodySlide export failed");
			}
		});
	}

	// ----------------------------------------------------------------------
	// modes

	// RaceMenu's own UI stays available as a complete fallback: the editor steps
	// aside and the original SWF takes the input.
	void EnterNative()
	{
		SetMode(Mode::Native);
	}

	void ExitNative()
	{
		SetMode(Mode::Editor);
		RequestRefresh(0);
	}

	// Sculpt's live mesh canvas belongs to RaceMenu's native CharGen code and is
	// drawn by its SWF, so the real workspace is opened rather than imitated.
	void EnterSculpt()
	{
		QueueUI([] {
			auto movie = Movie();
			if (!movie) {
				return;
			}
			RE::GFxValue mode(3.0);  // Sliders, Presets, Camera, Sculpt
			if (!movie->Invoke(Path(".modeSelect.setMode").c_str(), nullptr, &mode, 1)) {
				Status("RaceMenu's Sculpt mode is not available");
				return;
			}
			SetMode(Mode::Sculpt);
		});
	}

	void ExitSculpt()
	{
		QueueUI([] {
			if (auto movie = Movie()) {
				RE::GFxValue mode(0.0);
				movie->Invoke(Path(".modeSelect.setMode").c_str(), nullptr, &mode, 1);
			}
			SetMode(Mode::Editor);
			RequestRefresh(0);
		});
	}

	void OpenConsole()
	{
		QueueUI([] {
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(RE::Console::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
			}
		});
	}

	// RaceMenu's "ChangeName" callback opens the vanilla name prompt, which
	// would stay invisible under a hidden SWF and block the accept flow. Set
	// the name directly and close the menu through the normal message queue, so
	// the game and RaceMenu still run their shutdown bookkeeping.
	void Done(const std::string& a_name)
	{
		QueueUI([a_name] {
			const auto ui = RE::UI::GetSingleton();
			auto       menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : nullptr;
			if (!menu) {
				logger::warn("Done ignored: RaceSexMenu is unavailable");
				return;
			}
			if (!a_name.empty()) {
				menu->ChangeName(a_name.c_str());
			}
			WithModel([](Model& m) { m.mode = Mode::Closed; });
			SetSwfVisible(true);
			SetCursorVisible(true);
			if (const auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(RE::RaceSexMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
				logger::info("RaceSexMenu hide queued by Done");
			}
		});
	}

	// A short report for bug reports; no preset data or file paths.
	void WriteDiagnostics()
	{
		const auto dir = SKSE::log::log_directory();
		if (!dir) {
			Status("Could not find the SKSE log folder");
			return;
		}
		std::ofstream out(*dir / "RaceMenuAtelier-diagnostic.txt", std::ios::trunc);
		if (!out) {
			Status("Could not write the diagnostic report");
			return;
		}
		const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
		auto&       model = Model::Get();
		out << "RaceMenu Atelier " << plugin->GetVersion().string() << "\n";
		out << "Game runtime: " << REL::Module::get().version().string(".") << "\n";
		out << "RaceMenu open: " << (IsMenuOpen() ? "yes" : "no") << "\n";
		out << "Session: " << g_session.load() << "\n";
		{
			std::scoped_lock lock(model.mutex);
			out << "Entries: " << model.entries.size() << ", categories: " << model.categories.size() << "\n";
			out << "Mode: " << static_cast<int>(model.mode) << ", yielding: " << model.yieldCount << "\n";

			// category matching inputs as read from the SWF
			out << "\nCategories (raw | flag | textFilter):\n";
			for (const auto& c : model.categories) {
				out << "  " << c.raw << " | " << c.flag << " | " << c.textFilter << "\n";
			}
			out << "\nEntries (swfIndex | type | text | filterFlag | textFilters | callback | sliderID):\n";
			for (const auto& e : model.entries) {
				out << "  " << e.swfIndex << " | " << e.type << " | " << e.rawText << " | " << e.filterFlag << " | ";
				for (std::size_t i = 0; i < e.textFilters.size(); ++i) {
					out << (i ? "," : "") << e.textFilters[i];
				}
				out << " | " << e.callback << " | " << e.sliderID << "\n";
			}
		}
		out << "Attach this file together with the full Crash Logger report.\n";
		Status("Diagnostic report saved next to the SKSE log");
	}
}
