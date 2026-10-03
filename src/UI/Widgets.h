#pragma once

#include "UI/Theme.h"

namespace RMA::UI::Widgets
{
	void Tooltip(const char* a_text);

	// square button with a Font Awesome glyph; a_active draws it lit
	bool IconButton(const char* a_id, const char* a_icon, const char* a_tooltip, bool a_active = false, float a_size = 0.0f);

	// framed button with an optional leading glyph
	bool Button(const char* a_id, const char* a_icon, const char* a_label, ImVec2 a_size, bool a_primary = false, bool a_active = false);

	// gold small-caps title with a hairline
	void SectionTitle(const char* a_text);

	// collapsible header of the side panel
	bool Collapsible(const char* a_id, const char* a_icon, const char* a_label, bool& a_open);

	void DrawChecker(ImGuiMCP::ImDrawList* a_list, ImVec2 a_min, ImVec2 a_max, float a_cell);

	// colour swatch for a game colour (0xAARRGGBB); returns true when clicked
	bool Swatch(const char* a_id, std::uint32_t a_argb, ImVec2 a_size, const char* a_tooltip = nullptr);

	// the texture preview on a checkerboard, fitted into a_size
	void TextureImage(const std::string& a_path, ImVec2 a_size);

	struct SliderState
	{
		bool changed{ false };
		bool activated{ false };
		bool deactivated{ false };
	};

	// RaceMenu value track: fill from the origin, a notch at the session's
	// starting value, a gold thumb. Snaps to a_interval.
	SliderState ValueTrack(const char* a_id, double& a_value, double a_min, double a_max, double a_interval, double a_initial, ImVec2 a_size, ImU32 a_fill = Color::Gold);

	// text drawn at an arbitrary size through the draw list
	void SmallText(ImGuiMCP::ImDrawList* a_list, ImVec2 a_pos, ImU32 a_color, const char* a_text, float a_scale = 0.78f);
	ImVec2 SmallTextSize(const char* a_text, float a_scale = 0.78f);

	// text cut with an ellipsis to fit a_width
	std::string Ellipsize(const std::string& a_text, float a_width);
}
