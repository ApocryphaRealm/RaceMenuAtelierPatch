#pragma once

// Everything the editor knows about the open RaceMenu, read out of RaceMenu's
// own Scaleform data by the Bridge and drawn by the UI. One mutex guards it;
// the UI holds it for a whole frame, the Bridge only while it merges results.

namespace RMA
{
	// RaceMenuDefines.as
	namespace Defs
	{
		inline constexpr int kTypeRace = 1;
		inline constexpr int kTypeSlider = 2;
		inline constexpr int kTypeWarPaint = 3;
		inline constexpr int kTypeFacePaint = 7;

		inline constexpr std::uint32_t kCategoryRace = 2;
		inline constexpr std::uint32_t kCategoryColor = 8192;
		inline constexpr std::uint32_t kCategoryAll = 2044;

		inline constexpr int kTintHair = 128;
		inline constexpr int kTintWarpaint = 7;
		inline constexpr int kTintDirt = 14;

		inline constexpr double kCustomSliderOffset = 1000.0;
	}

	enum class Control
	{
		Race,
		Choice,  // a race-type entry another mod put in a category of its own (Apprentice's classes and traits)
		Slider,
		Stepper,
		Sex,
		Paint
	};

	struct Category
	{
		std::string   raw;
		std::string   label;
		std::uint32_t flag{ 0 };
		std::string   textFilter;
	};

	struct PaintTexture
	{
		std::string text;
		std::string texture;
		std::string label;
		std::string search;
	};

	// Locates an entry inside RaceMenu's entryList for write-back. The index is
	// only trusted when callback and slider id still match.
	struct EntryRef
	{
		int         swfIndex{ -1 };
		std::string callback;
		double      sliderID{ -1.0 };
	};

	struct Entry
	{
		// as read from the SWF
		int                      swfIndex{ -1 };
		int                      type{ -1 };
		std::int64_t             filterFlag{ -1 };
		std::string              callback;
		double                   sliderID{ -1.0 };
		double                   min{ 0.0 };
		double                   max{ 1.0 };
		double                   interval{ 0.1 };
		double                   position{ 0.0 };
		bool                     enabled{ true };
		std::vector<std::string> textFilters;
		int                      tintType{ -1 };
		int                      tintIndex{ 0 };
		std::uint32_t            fillColor{ 0 };
		int                      raceID{ -1 };
		std::string              texture;
		int                      listType{ -1 };
		std::string              rawText;
		std::string              text;  // translated
		std::string              description;  // raceDescription, when the entry carries one

		// derived
		std::string key;
		std::string label;       // humanised
		std::string group;       // "Breasts" of "Breasts - Size"
		std::string shortLabel;  // "Size"
		std::string search;      // lower-case label + raw text
		Control     control{ Control::Slider };
		bool        hasColor{ false };
		bool        custom{ false };
		bool        canOverdrive{ false };

		// session
		double        initialPosition{ 0.0 };
		std::uint32_t initialFill{ 0 };
		std::string   initialTexture;
		double        localEditTime{ -1.0e9 };

		[[nodiscard]] EntryRef Ref() const { return { swfIndex, callback, sliderID }; }
		[[nodiscard]] bool     IsChanged() const;
	};

	struct Snapshot
	{
		std::vector<Category>                    categories;
		std::vector<Entry>                       entries;
		std::array<std::vector<PaintTexture>, 5> makeup;
		std::vector<std::string>                 picked;  // the bottom bar's choice values (Apprentice: class, trait)
	};

	struct PlayerInfo
	{
		std::string name;
		std::string race;
		int         sex{ -1 };  // 0 male, 1 female
	};

	struct HeadPart
	{
		int         index{ 0 };
		std::string name;
		std::string label;
		std::string plugin;
		std::string search;
	};

	struct PresetInfo
	{
		bool                     loaded{ false };
		bool                     readable{ false };
		std::vector<std::string> mods;
		std::vector<std::string> missing;
		std::vector<std::string> headParts;  // "Hair - Plugin.esp"
		std::vector<std::string> tints;      // texture paths
	};

	struct HistoryStep
	{
		enum class Kind
		{
			Value,
			Color,
			Texture
		};

		std::string   key;
		Kind          kind{ Kind::Value };
		double        from{ 0.0 };
		double        to{ 0.0 };
		std::uint32_t fromColor{ 0 };
		std::uint32_t toColor{ 0 };
		std::string   fromTexture;
		std::string   toTexture;
	};

	using HistoryItem = std::vector<HistoryStep>;

	enum class Mode
	{
		Closed,
		Editor,
		Native,
		Sculpt
	};

	class Model
	{
	public:
		static Model& Get();

		// recursive: the UI holds it for a frame and calls Bridge functions that
		// update the model synchronously before queueing their game work
		std::recursive_mutex mutex;

		// RaceMenu data
		std::vector<Category>                    categories;
		std::vector<Entry>                       entries;
		std::array<std::vector<PaintTexture>, 5> makeup;
		std::unordered_map<std::string, int>     byKey;
		PlayerInfo                               player;
		std::vector<std::string>                 picked;  // names the menu shows as chosen (bottom bar)
		std::unordered_map<std::int64_t, std::string> chosen;  // category flag -> key of this session's pick
		bool                                     hasData{ false };
		std::uint64_t                            dataEpoch{ 0 };   // bumps on every snapshot
		std::uint64_t                            valueEpoch{ 0 };  // bumps on every local edit

		// session / mode
		Mode mode{ Mode::Closed };
		int  yieldCount{ 0 };  // other menus on top that need the input
		bool frozen{ false };
		bool undressed{ false };
		bool zoomFace{ true };
		bool lightOn{ true };

		// replies from the Bridge
		std::vector<std::string>                           presetFiles;
		bool                                               presetsLoaded{ false };
		std::unordered_map<std::string, PresetInfo>        presetInfo;
		std::unordered_map<double, std::string>            partNames;  // sliderID -> head part name
		double                                             headPartSlider{ -1.0 };
		std::vector<HeadPart>                              headParts;
		bool                                               headPartsLoaded{ false };
		std::string                                        status;
		double                                             statusTime{ 0.0 };

		// history
		std::deque<HistoryItem> past;
		std::deque<HistoryItem> future;

		// --- called by the Bridge (lock held by the caller) ---
		void ApplySnapshot(Snapshot&& a_snapshot);
		void ApplyPlayer(PlayerInfo&& a_player);
		void ResetSession();
		void SetStatus(std::string a_text);

		// --- editing (lock held; dispatches to the game) ---
		Entry*       Find(const std::string& a_key);
		const Entry* FindSlider(double a_sliderID) const;

		void SetValue(Entry& a_entry, double a_value, bool a_send = true);
		void SetColor(Entry& a_entry, std::uint32_t a_argb, bool a_send = true);
		void SetTexture(Entry& a_entry, const std::string& a_texture);
		void ChangeRace(const Entry& a_entry);
		void PressChoice(const Entry& a_entry);

		void Record(HistoryItem a_item);
		void Undo();
		void Redo();
		void ResetEntry(Entry& a_entry);
		void ResetAll();

		[[nodiscard]] int  TintIndexFor(const Entry& a_entry) const;
		[[nodiscard]] bool IsCurrentRace(const Entry& a_entry) const;
		[[nodiscard]] bool IsCurrentChoice(const Entry& a_entry) const;

	private:
		void Apply(const HistoryStep& a_step, bool a_forward);
	};

	// text helpers shared with the UI
	namespace Text
	{
		std::string Lower(std::string_view a_text);
		std::string Humanise(std::string_view a_text, bool a_dropVariant = false);
		std::string Translate(const std::string& a_text);
		std::string FileStem(std::string_view a_path);
		double      Now();
	}
}
