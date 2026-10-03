#pragma once

namespace RMA::UI
{
	// registers the editor window, the HUD banner and the hotkeys with SKSE Menu Framework
	bool Register();

	[[nodiscard]] bool IsRegistered();
}
