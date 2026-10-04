#pragma once

namespace RMA::UI
{
	// registers the editor window, the HUD banner and the hotkeys with SKSE Menu Framework
	bool Register();

	[[nodiscard]] bool IsRegistered();

	// DevBench (atelier.control): the editor's category by its label or raw text ("" = all); lock-safe
	bool          SelectCategoryNamed(const std::string& a_name);
	[[nodiscard]] std::string CurrentCategory();
}
