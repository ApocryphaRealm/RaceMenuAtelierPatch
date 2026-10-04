#include "UI/Widgets.h"

#include "Preview.h"

namespace RMA::UI::Widgets
{
	namespace im = ImGuiMCP;
	namespace dl = ImGuiMCP::ImDrawListManager;

	void Tooltip(const char* a_text)
	{
		if (!a_text || !*a_text) {
			return;
		}
		if (im::BeginTooltip()) {
			ApplyFont();
			im::PushTextWrapPos(U() * 22.0f);
			im::TextUnformatted(a_text);
			im::PopTextWrapPos();
			im::EndTooltip();
		}
	}

	bool IconButton(const char* a_id, const char* a_icon, const char* a_tooltip, bool a_active, float a_size)
	{
		const float s = a_size > 0.0f ? a_size : im::GetFrameHeight();
		const auto  pos = im::GetCursorScreenPos();
		const bool  pressed = im::InvisibleButton(a_id, { s, s });
		const bool  hovered = im::IsItemHovered();
		const bool  held = im::IsItemActive();
		auto*       list = im::GetWindowDrawList();

		const ImU32 bg = a_active ? Color::GoldWashStrong : held ? Color::FrameActive :
		                                                    hovered ? Color::FrameHover :
		                                                              Color::Frame;
		dl::AddRectFilled(list, pos, pos + ImVec2{ s, s }, bg, Round(U() * 0.25f), 0);
		if (a_active) {
			dl::AddRect(list, pos, pos + ImVec2{ s, s }, Color::BorderStrong, Round(U() * 0.25f), 0, 1.0f);
		}
		DrawIcon(list, pos + ImVec2{ s * 0.5f, s * 0.5f }, a_icon, a_active ? Color::GoldBright : hovered ? Color::Text : Color::TextDim);
		if (hovered && a_tooltip) {
			Tooltip(a_tooltip);
		}
		return pressed;
	}

	bool Button(const char* a_id, const char* a_icon, const char* a_label, ImVec2 a_size, bool a_primary, bool a_active)
	{
		const float u = U();
		const auto  labelSize = im::CalcTextSize(a_label ? a_label : "");
		const auto  iconSize = a_icon ? IconSize(a_icon) : ImVec2{ 0.0f, 0.0f };
		const float gap = a_icon && a_label && *a_label ? u * 0.4f : 0.0f;
		const float content = iconSize.x + gap + labelSize.x;
		ImVec2      size = a_size;
		if (size.x <= 0.0f) {
			size.x = content + u * 1.2f;
		}
		if (size.y <= 0.0f) {
			size.y = im::GetFrameHeight();
		}

		const auto pos = im::GetCursorScreenPos();
		const bool pressed = im::InvisibleButton(a_id, size);
		const bool hovered = im::IsItemHovered();
		const bool held = im::IsItemActive();
		auto*      list = im::GetWindowDrawList();

		ImU32 bg, fg;
		if (a_primary) {
			bg = held ? Color::GoldDim : hovered ? Color::GoldBright :
			                                       Color::Gold;
			fg = Color::OnAccent;
		} else {
			bg = a_active ? Color::GoldWashStrong : held ? Color::FrameActive :
			                                        hovered ? Color::FrameHover :
			                                                  Color::Frame;
			fg = a_active ? Color::GoldBright : hovered ? Color::Text :
			                                              Color::TextDim;
		}
		dl::AddRectFilled(list, pos, pos + size, bg, Round(u * 0.25f), 0);
		if (a_active && !a_primary) {
			dl::AddRect(list, pos, pos + size, Color::BorderStrong, Round(u * 0.25f), 0, 1.0f);
		}

		float x = pos.x + std::max(u * 0.4f, (size.x - content) * 0.5f);
		if (a_icon) {
			DrawIcon(list, { x + iconSize.x * 0.5f, pos.y + size.y * 0.5f }, a_icon, fg);
			x += iconSize.x + gap;
		}
		if (a_label && *a_label) {
			dl::AddText(list, { x, pos.y + (size.y - labelSize.y) * 0.5f }, fg, a_label);
		}
		return pressed;
	}

	void SectionTitle(const char* a_text)
	{
		const auto pos = im::GetCursorScreenPos();
		const auto size = SmallTextSize(a_text, 0.8f);
		const float width = im::GetContentRegionAvail().x;
		auto*       list = im::GetWindowDrawList();
		SmallText(list, pos, Color::Gold, a_text, 0.8f);
		const float lineY = std::floor(pos.y + size.y * 0.55f);
		dl::AddLine(list, { pos.x + size.x + U() * 0.5f, lineY }, { pos.x + width, lineY }, Color::Border, 1.0f);
		im::Dummy({ width, size.y + U() * 0.15f });
	}

	bool Collapsible(const char* a_id, const char* a_icon, const char* a_label, bool& a_open)
	{
		const float u = U();
		const float width = im::GetContentRegionAvail().x;
		const float height = u * 1.9f;
		const auto  pos = im::GetCursorScreenPos();
		if (im::InvisibleButton(a_id, { width, height })) {
			a_open = !a_open;
		}
		const bool hovered = im::IsItemHovered();
		auto*      list = im::GetWindowDrawList();
		dl::AddRectFilled(list, pos, pos + ImVec2{ width, height }, hovered ? Color::PanelHover : Color::Panel, Round(u * 0.3f), 0);
		dl::AddRectFilled(list, pos, pos + ImVec2{ 2.0f, height }, a_open ? Color::Gold : Color::GoldDim, 0.0f, 0);
		if (a_icon) {
			DrawIcon(list, pos + ImVec2{ u * 1.0f, height * 0.5f }, a_icon, Color::Gold);
		}
		const auto labelSize = im::CalcTextSize(a_label);
		dl::AddText(list, pos + ImVec2{ u * 2.0f, (height - labelSize.y) * 0.5f }, hovered ? Color::Text : Color::TextDim, a_label);
		DrawIcon(list, pos + ImVec2{ width - u * 0.9f, height * 0.5f }, a_open ? Icon::ChevronUp : Icon::ChevronDown, Color::TextFaint);
		return a_open;
	}

	void DrawChecker(ImGuiMCP::ImDrawList* a_list, ImVec2 a_min, ImVec2 a_max, float a_cell)
	{
		dl::AddRectFilled(a_list, a_min, a_max, IM_COL32(92, 88, 82, 255), 0.0f, 0);
		const float cell = std::max(2.0f, a_cell);
		int         row = 0;
		for (float y = a_min.y; y < a_max.y; y += cell, ++row) {
			int col = 0;
			for (float x = a_min.x; x < a_max.x; x += cell, ++col) {
				if ((row + col) % 2 == 0) {
					dl::AddRectFilled(a_list, { x, y }, { std::min(x + cell, a_max.x), std::min(y + cell, a_max.y) }, IM_COL32(140, 136, 128, 255), 0.0f, 0);
				}
			}
		}
	}

	bool Swatch(const char* a_id, std::uint32_t a_argb, ImVec2 a_size, const char* a_tooltip)
	{
		const auto pos = im::GetCursorScreenPos();
		const bool pressed = im::InvisibleButton(a_id, a_size);
		const bool hovered = im::IsItemHovered();
		auto*      list = im::GetWindowDrawList();
		DrawChecker(list, pos, pos + a_size, a_size.y * 0.5f);
		dl::AddRectFilled(list, pos, pos + a_size, FromArgb(a_argb), 0.0f, 0);
		// the colour at full opacity on the left half, so faint tints stay readable
		dl::AddRectFilled(list, pos, pos + ImVec2{ a_size.x * 0.35f, a_size.y }, FromArgb(a_argb, true), 0.0f, 0);
		dl::AddRect(list, pos, pos + a_size, hovered ? Color::GoldBright : Color::BorderStrong, 0.0f, 0, 1.0f);
		if (hovered && a_tooltip) {
			Tooltip(a_tooltip);
		}
		return pressed;
	}

	void TextureImage(const std::string& a_path, ImVec2 a_size)
	{
		const auto pos = im::GetCursorScreenPos();
		auto*      list = im::GetWindowDrawList();
		im::Dummy(a_size);
		const auto* image = Preview::Get(a_path);
		if (!image) {
			dl::AddRectFilled(list, pos, pos + a_size, Color::Track, Round(U() * 0.2f), 0);
			const char* text = a_path.empty() ? "no texture" : "no preview";
			const auto  size = SmallTextSize(text);
			SmallText(list, pos + ImVec2{ (a_size.x - size.x) * 0.5f, (a_size.y - size.y) * 0.5f }, Color::TextFaint, text);
			return;
		}
		const float scale = std::min(a_size.x / image->width, a_size.y / image->height);
		const ImVec2 fitted{ image->width * scale, image->height * scale };
		const ImVec2 min = pos + ImVec2{ (a_size.x - fitted.x) * 0.5f, (a_size.y - fitted.y) * 0.5f };
		DrawChecker(list, min, min + fitted, U() * 0.5f);
		dl::AddImage(list, image->texture, min, min + fitted, { 0.0f, 0.0f }, { 1.0f, 1.0f }, IM_COL32(255, 255, 255, 255));
		dl::AddRect(list, min, min + fitted, Color::Border, 0.0f, 0, 1.0f);
	}

	SliderState ValueTrack(const char* a_id, double& a_value, double a_min, double a_max, double a_interval, double a_initial, ImVec2 a_size, ImU32 a_fill)
	{
		SliderState state;
		const float u = U();
		const auto  pos = im::GetCursorScreenPos();
		im::InvisibleButton(a_id, a_size, ImGuiMCP::ImGuiButtonFlags_PressedOnClick);
		const bool hovered = im::IsItemHovered();
		const bool active = im::IsItemActive();
		state.activated = im::IsItemActivated();
		state.deactivated = im::IsItemDeactivated();

		const double range = a_max - a_min;
		const float  radius = std::floor(u * 0.42f);
		const float  x0 = pos.x + radius;
		const float  x1 = pos.x + a_size.x - radius;
		const float  cy = std::floor(pos.y + a_size.y * 0.5f);
		const auto   toX = [&](double v) {
            const double t = range > 0.0 ? std::clamp((v - a_min) / range, 0.0, 1.0) : 0.0;
            return x0 + static_cast<float>(t) * (x1 - x0);
		};

		if (active && range > 0.0) {
			const float mouse = im::GetMousePos().x;
			double      t = std::clamp(static_cast<double>((mouse - x0) / std::max(1.0f, x1 - x0)), 0.0, 1.0);
			double      v = a_min + t * range;
			if (a_interval > 0.0) {
				v = a_min + std::round((v - a_min) / a_interval) * a_interval;
			}
			v = std::clamp(v, a_min, a_max);
			if (std::abs(v - a_value) > 1e-9) {
				a_value = v;
				state.changed = true;
			}
		}

		auto*       list = im::GetWindowDrawList();
		const float half = std::max(2.0f, std::floor(u * 0.14f));
		dl::AddRectFilled(list, { x0 - radius * 0.5f, cy - half }, { x1 + radius * 0.5f, cy + half }, Color::Track, half, 0);
		dl::AddRect(list, { x0 - radius * 0.5f, cy - half }, { x1 + radius * 0.5f, cy + half }, WithAlpha(Color::RowBorder, 0.8f), half, 0, 1.0f);

		// fill from zero when the range crosses it, otherwise from the minimum
		const double origin = (a_min < 0.0 && a_max > 0.0) ? 0.0 : a_min;
		const float  ox = toX(origin);
		const float  vx = toX(a_value);
		dl::AddRectFilled(list, { std::min(ox, vx), cy - half }, { std::max(ox, vx), cy + half },
			active || hovered ? a_fill : WithAlpha(a_fill, 0.8f), half, 0);
		if (origin != a_min) {
			dl::AddLine(list, { ox, cy - half * 2.2f }, { ox, cy + half * 2.2f }, Color::TextFaint, 1.0f);
		}

		// where the session started
		if (std::abs(a_initial - a_value) > 1e-6) {
			const float ix = toX(a_initial);
			dl::AddRectFilled(list, { ix - 1.0f, cy - radius * 0.9f }, { ix + 1.0f, cy + radius * 0.9f }, WithAlpha(Color::GoldBright, 0.55f), 0.0f, 0);
		}

		const bool outside = a_value < a_min - 1e-9 || a_value > a_max + 1e-9;
		dl::AddCircleFilled(list, { vx, cy }, radius + 1.5f, Color::Shadow, 20);
		dl::AddCircleFilled(list, { vx, cy }, radius, outside ? Color::Danger : active ? Color::GoldBright : hovered ? Color::KnobHover : Color::Text, 20);
		dl::AddCircleFilled(list, { vx, cy }, radius * 0.4f, a_fill, 12);
		return state;
	}

	void SmallText(ImGuiMCP::ImDrawList* a_list, ImVec2 a_pos, ImU32 a_color, const char* a_text, float a_scale)
	{
		dl::AddText(a_list, im::GetFont(), im::GetFontSize() * a_scale, a_pos, a_color, a_text);
	}

	ImVec2 SmallTextSize(const char* a_text, float a_scale)
	{
		const auto size = im::CalcTextSize(a_text);
		return { size.x * a_scale, size.y * a_scale };
	}

	std::string Ellipsize(const std::string& a_text, float a_width)
	{
		if (im::CalcTextSize(a_text.c_str()).x <= a_width) {
			return a_text;
		}
		std::string out = a_text;
		while (!out.empty()) {
			// drop one UTF-8 code point
			std::size_t cut = out.size() - 1;
			while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) {
				--cut;
			}
			out.erase(cut);
			if (im::CalcTextSize((out + "...").c_str()).x <= a_width) {
				return out + "...";
			}
		}
		return "...";
	}
}
