#pragma once

namespace RMA::UI
{
	// registers the editor window, the HUD banner and the hotkeys with SKSE Menu Framework
	bool Register();

	[[nodiscard]] bool IsRegistered();

	// DevBench (atelier.control): the editor's category by its label or raw text ("" = all); lock-safe
	bool          SelectCategoryNamed(const std::string& a_name);
	[[nodiscard]] std::string CurrentCategory();

	// DevBench: open the "Finish character creation?" confirmation exactly as controller X does (next frame), and read
	// whether it is up and whether its Finish button holds the controller focus (1.0.1 - A then presses Finish)
	void          PadDone();
	[[nodiscard]] bool DoneConfirmOpen();
	[[nodiscard]] bool DoneFinishFocused();

	// DevBench (1.0.2): put text into a field as if typed - "name", "preset", "search" or "bodyslide" - on the next frame,
	// and read a field back; lock-safe
	bool          SetText(const std::string& a_field, const std::string& a_text);

	// DevBench (1.1.0, op=nudge): open the options, highlight an options slider ("uiscale", "editorwidth", "rotatespeed")
	// and step it a_count units (0.01 each) in a_dir, exactly as Left/Right on it does - next frame; lock-safe
	bool          NudgeSlider(const std::string& a_name, int a_dir, int a_count);
	// the three option values and the last DevBench nudge (JSON)
	[[nodiscard]] std::string Options();
	[[nodiscard]] std::string TypedText(const std::string& a_field);
}
