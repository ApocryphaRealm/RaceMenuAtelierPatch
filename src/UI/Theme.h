#pragma once

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

// Colours, metrics and icon glyphs of the editor. Everything is scaled from
// the current font size, so the layout follows SKSE Menu Framework's font and
// the user's scale setting at any resolution.

namespace RMA::UI
{
	using ImGuiMCP::ImU32;
	using ImGuiMCP::ImVec2;
	using ImGuiMCP::ImVec4;

	namespace Color
	{
		// Layered slate surfaces (each level a step lighter), warm gold for
		// actions and changes, desaturated hues to tell groups apart. These are the
		// defaults; an optional theme file replaces them at load (LoadTheme).
		inline ImU32 Window = IM_COL32(17, 19, 24, 242);
		inline ImU32 Panel = IM_COL32(30, 34, 42, 235);
		inline ImU32 PanelHover = IM_COL32(40, 45, 55, 245);
		inline ImU32 Row = IM_COL32(29, 33, 40, 240);
		inline ImU32 RowAlt = IM_COL32(35, 39, 48, 240);
		inline ImU32 RowHover = IM_COL32(45, 51, 62, 250);
		inline ImU32 RowBorder = IM_COL32(78, 86, 102, 110);
		inline ImU32 Frame = IM_COL32(44, 49, 60, 255);
		inline ImU32 FrameHover = IM_COL32(56, 62, 75, 255);
		inline ImU32 FrameActive = IM_COL32(66, 73, 88, 255);
		inline ImU32 Border = IM_COL32(74, 82, 98, 170);
		inline ImU32 BorderStrong = IM_COL32(214, 172, 98, 230);
		inline ImU32 Track = IM_COL32(11, 13, 17, 235);

		inline ImU32 Text = IM_COL32(234, 231, 223, 255);
		inline ImU32 TextDim = IM_COL32(164, 170, 182, 255);
		inline ImU32 TextFaint = IM_COL32(112, 119, 132, 255);
		inline ImU32 Value = IM_COL32(132, 196, 206, 255);

		inline ImU32 Gold = IM_COL32(214, 172, 98, 255);
		inline ImU32 GoldBright = IM_COL32(244, 208, 138, 255);
		inline ImU32 GoldDim = IM_COL32(140, 110, 62, 255);
		inline ImU32 GoldWash = IM_COL32(214, 172, 98, 40);
		inline ImU32 GoldWashStrong = IM_COL32(214, 172, 98, 84);
		inline ImU32 Danger = IM_COL32(222, 102, 86, 255);
		inline ImU32 Ok = IM_COL32(130, 190, 110, 255);
		inline ImU32 Shadow = IM_COL32(0, 0, 0, 170);
		inline ImU32 Clear = IM_COL32(0, 0, 0, 0);

		// group / category hues: equal lightness, about 20 points under full saturation
		inline std::array<ImU32, 10> Hues{
			IM_COL32(110, 168, 222, 255),  // steel blue
			IM_COL32(214, 150, 96, 255),   // copper
			IM_COL32(112, 192, 168, 255),  // teal
			IM_COL32(176, 146, 222, 255),  // violet
			IM_COL32(150, 196, 104, 255),  // moss
			IM_COL32(222, 132, 150, 255),  // rose
			IM_COL32(204, 188, 102, 255),  // ochre
			IM_COL32(104, 196, 214, 255),  // aqua
			IM_COL32(204, 132, 200, 255),  // mauve
			IM_COL32(168, 176, 196, 255),  // pewter
		};

		// formerly literals in the drawing code, named so a theme can set them
		inline ImU32 PopupBg = IM_COL32(24, 27, 34, 250);
		inline ImU32 HeaderHover = IM_COL32(214, 172, 98, 60);
		inline ImU32 ScrollGrab = IM_COL32(90, 98, 116, 150);
		inline ImU32 ScrollGrabHover = IM_COL32(120, 130, 150, 200);
		inline ImU32 TextSelected = IM_COL32(214, 172, 98, 90);
		inline ImU32 ModalDim = IM_COL32(0, 0, 0, 120);
		inline ImU32 TileText = IM_COL32(214, 218, 226, 255);
		inline ImU32 OnAccent = IM_COL32(28, 20, 10, 255);
		inline ImU32 KnobHover = IM_COL32(250, 246, 236, 255);
	}

	// the theme's corner factor: 1 = Atelier's rounded corners, 0 = square (Norden)
	inline float RoundingScale = 1.0f;
	inline float Round(float a_radius) { return a_radius * RoundingScale; }

	// reads Data\SKSE\Plugins\RaceMenuAtelier\theme.ini when it exists (a theme package ships it); without it the
	// defaults above stay. Logs every key it applies.
	void LoadTheme();

	inline ImVec4 ToVec4(ImU32 a_color)
	{
		return {
			static_cast<float>((a_color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
			static_cast<float>((a_color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
			static_cast<float>((a_color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
			static_cast<float>((a_color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f
		};
	}

	inline ImU32 WithAlpha(ImU32 a_color, float a_alpha)
	{
		const auto alpha = static_cast<ImU32>(std::clamp(a_alpha, 0.0f, 1.0f) * ((a_color >> IM_COL32_A_SHIFT) & 0xFF));
		return (a_color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
	}

	// game colours are 0xAARRGGBB, ImGui's are 0xAABBGGRR
	inline ImU32 FromArgb(std::uint32_t a_argb, bool a_opaque = false)
	{
		const ImU32 a = a_opaque ? 255 : (a_argb >> 24) & 0xFF;
		return IM_COL32((a_argb >> 16) & 0xFF, (a_argb >> 8) & 0xFF, a_argb & 0xFF, a);
	}

	inline ImVec2 operator+(ImVec2 a, ImVec2 b) { return { a.x + b.x, a.y + b.y }; }
	inline ImVec2 operator-(ImVec2 a, ImVec2 b) { return { a.x - b.x, a.y - b.y }; }
	inline ImVec2 operator*(ImVec2 a, float s) { return { a.x * s, a.y * s }; }

	// Font Awesome 6 solid glyphs, provided to SKSE Menu Framework by "imGui Icons"
	struct Glyph
	{
		char text[4];

		constexpr explicit Glyph(char32_t a_code) :
			text{ static_cast<char>(0xE0 | (a_code >> 12)), static_cast<char>(0x80 | ((a_code >> 6) & 0x3F)), static_cast<char>(0x80 | (a_code & 0x3F)), 0 }
		{}

		operator const char*() const { return text; }
	};

	namespace Icon
	{
		inline constexpr const char* Font = "fa-solid-900";

		inline constexpr Glyph ChevronLeft{ 0xF053 };
		inline constexpr Glyph ChevronRight{ 0xF054 };
		inline constexpr Glyph ChevronDown{ 0xF078 };
		inline constexpr Glyph ChevronUp{ 0xF077 };
		inline constexpr Glyph Undo{ 0xF0E2 };
		inline constexpr Glyph Redo{ 0xF01E };
		inline constexpr Glyph Search{ 0xF002 };
		inline constexpr Glyph Close{ 0xF00D };
		inline constexpr Glyph Check{ 0xF00C };
		inline constexpr Glyph Gear{ 0xF013 };
		inline constexpr Glyph Filter{ 0xF0B0 };
		inline constexpr Glyph Sliders{ 0xF1DE };
		inline constexpr Glyph Grid{ 0xF00A };
		inline constexpr Glyph User{ 0xF007 };
		inline constexpr Glyph Users{ 0xF0C0 };
		inline constexpr Glyph Person{ 0xF183 };
		inline constexpr Glyph Face{ 0xF118 };
		inline constexpr Glyph Eye{ 0xF06E };
		inline constexpr Glyph Brow{ 0xF7A4 };
		inline constexpr Glyph Mouth{ 0xF596 };
		inline constexpr Glyph Scissors{ 0xF0C4 };
		inline constexpr Glyph Palette{ 0xF53F };
		inline constexpr Glyph Brush{ 0xF1FC };
		inline constexpr Glyph SprayCan{ 0xF5BD };
		inline constexpr Glyph Hand{ 0xF256 };
		inline constexpr Glyph Feet{ 0xF54B };
		inline constexpr Glyph Mask{ 0xF630 };
		inline constexpr Glyph Layers{ 0xF5FD };
		inline constexpr Glyph Puzzle{ 0xF12E };
		inline constexpr Glyph Sun{ 0xF185 };
		inline constexpr Glyph Moon{ 0xF186 };
		inline constexpr Glyph Shirt{ 0xF553 };
		inline constexpr Glyph Snowflake{ 0xF2DC };
		inline constexpr Glyph Play{ 0xF04B };
		inline constexpr Glyph Stop{ 0xF04D };
		inline constexpr Glyph ZoomIn{ 0xF00E };
		inline constexpr Glyph ZoomOut{ 0xF010 };
		inline constexpr Glyph RotateLeft{ 0xF2EA };
		inline constexpr Glyph RotateRight{ 0xF2F9 };
		inline constexpr Glyph Refresh{ 0xF2F1 };
		inline constexpr Glyph Folder{ 0xF07C };
		inline constexpr Glyph Save{ 0xF0C7 };
		inline constexpr Glyph Export{ 0xF56E };
		inline constexpr Glyph Info{ 0xF05A };
		inline constexpr Glyph Warning{ 0xF071 };
		inline constexpr Glyph Wand{ 0xF0D0 };
		inline constexpr Glyph Wrench{ 0xF0AD };
		inline constexpr Glyph Venus{ 0xF221 };
		inline constexpr Glyph Mars{ 0xF222 };
		inline constexpr Glyph Reset{ 0xF0E2 };
		inline constexpr Glyph Dot{ 0xF111 };
		inline constexpr Glyph Columns{ 0xF0DB };
		inline constexpr Glyph Bug{ 0xF188 };
		inline constexpr Glyph Feather{ 0xF56B };

		// race crests
		inline constexpr Glyph Dragon{ 0xF6D5 };
		inline constexpr Glyph Wizard{ 0xF6E8 };
		inline constexpr Glyph Fire{ 0xF06D };
		inline constexpr Glyph Star{ 0xF005 };
		inline constexpr Glyph Crown{ 0xF521 };
		inline constexpr Glyph Paw{ 0xF1B0 };
		inline constexpr Glyph Mountain{ 0xF6FC };
		inline constexpr Glyph Hammer{ 0xF6E3 };
		inline constexpr Glyph Shield{ 0xF3ED };
		inline constexpr Glyph Leaf{ 0xF06C };
		inline constexpr Glyph Droplet{ 0xF043 };
	}

	// --- metrics ------------------------------------------------------------

	// target font size in pixels for the current display and user scale
	float TargetFontSize();

	// scales the current window's font to the target size; call right after
	// every Begin / BeginChild / BeginPopup
	void ApplyFont();

	// one "unit" = the current font size
	inline float U() { return ImGuiMCP::GetFontSize(); }

	// --- style scopes ---------------------------------------------------------

	void PushStyle();
	void PopStyle();

	// --- icon helpers ----------------------------------------------------------

	void PushIconFont();
	void PopIconFont();
	ImVec2 IconSize(const char* a_icon);
	void   DrawIcon(ImGuiMCP::ImDrawList* a_list, ImVec2 a_center, const char* a_icon, ImU32 a_color);
}
