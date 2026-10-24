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
}
