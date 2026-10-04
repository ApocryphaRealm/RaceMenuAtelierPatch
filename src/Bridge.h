#pragma once

#include "Model.h"

// The adapter to RaceMenu. RaceMenu stays installed and loaded exactly as it is:
// its Scaleform movie keeps running underneath (hidden) and is used as the data
// source and action target. Every call here only queues work; the GFx and actor
// work itself runs on the game's UI / main task queues.

namespace RMA::Bridge
{
	enum class ColorKind
	{
		Tint,
		Overlay,
		Hair
	};

	void Install();

	[[nodiscard]] bool IsMenuOpen();

	// data
	void RequestRefresh(int a_delayMs = 0);

	// editing
	void SetValue(const EntryRef& a_ref, double a_value);
	void SetColor(const EntryRef& a_ref, ColorKind a_kind, int a_slot, std::uint32_t a_argb);
	void SetTexture(const EntryRef& a_ref, bool a_overlay, int a_slot, const std::string& a_texture);
	void ChangeRace(int a_raceID);
	void PressEntry(const EntryRef& a_ref);  // the menu's own onItemPress for one entry (choices)
	void QuerySliderInfo(double a_sliderID, double a_value);
	void QueryHeadParts(double a_sliderID);

	// scene
	void Rotate(float a_degrees);
	void Zoom(bool a_face);
	void SetLight(bool a_on);
	void SetUndressed(bool a_undressed);
	void ToggleFreeze();
	void PlayPose(const std::string& a_event);

	// presets
	void ListPresets();
	void SavePreset(const std::string& a_name);
	void LoadPreset(const std::string& a_relativePath);
	void ReadPresetInfo(const std::string& a_relativePath);
	void ExportBodySlide(bool a_himbo, const std::string& a_name, std::vector<std::pair<std::string, double>> a_sliders);

	// modes
	void EnterNative();
	void ExitNative();
	void EnterSculpt();
	void ExitSculpt();
	void OpenConsole();
	void Done(const std::string& a_name);
	void WriteDiagnostics();

	[[nodiscard]] bool IsSafePresetName(const std::string& a_name);
}
