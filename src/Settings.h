#pragma once

// Viewer preferences, kept in Data\SKSE\Plugins\RaceMenuAtelier.ini.

namespace RMA
{
	struct Settings
	{
		float uiScale{ 1.0f };
		float editorWidth{ 0.30f };  // share of the screen width
		bool  sidePanel{ true };
		bool  sexFilter{ true };
		bool  overdrive{ false };
		bool  confirmDone{ true };
		bool  groupHeaders{ true };
		bool  showTechnicalNames{ false };
		float rotateSpeed{ 0.35f };  // degrees per pixel of mouse drag

		std::uint32_t              toggleKey{ 0x3E };  // F4, DirectInput scan code
		std::vector<std::uint32_t> recentColors;

		static Settings& Get();

		void Load();
		void Save() const;
		void AddRecentColor(std::uint32_t a_argb);
	};
}
