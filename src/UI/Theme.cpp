#include "UI/Theme.h"

#include "Settings.h"

namespace RMA::UI
{
	namespace
	{
		constexpr int kColors = 30;
		constexpr int kVars = 17;
	}

	float TargetFontSize()
	{
		const auto* io = ImGuiMCP::GetIO();
		const float height = io && io->DisplaySize.y > 0.0f ? io->DisplaySize.y : 1080.0f;
		return std::max(12.0f, height * (21.0f / 1080.0f) * Settings::Get().uiScale);
	}

	void ApplyFont()
	{
		ImGuiMCP::SetWindowFontScale(1.0f);
		const float base = ImGuiMCP::GetFontSize();
		ImGuiMCP::SetWindowFontScale(base > 0.0f ? TargetFontSize() / base : 1.0f);
	}

	void PushStyle()
	{
		using namespace ImGuiMCP;
		const float u = TargetFontSize();

		PushStyleColor(ImGuiCol_WindowBg, Color::Window);
		PushStyleColor(ImGuiCol_ChildBg, Color::Clear);
		PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 27, 34, 250));
		PushStyleColor(ImGuiCol_Border, Color::Border);
		PushStyleColor(ImGuiCol_BorderShadow, Color::Clear);
		PushStyleColor(ImGuiCol_FrameBg, Color::Frame);
		PushStyleColor(ImGuiCol_FrameBgHovered, Color::FrameHover);
		PushStyleColor(ImGuiCol_FrameBgActive, Color::FrameActive);
		PushStyleColor(ImGuiCol_Button, Color::Frame);
		PushStyleColor(ImGuiCol_ButtonHovered, Color::FrameHover);
		PushStyleColor(ImGuiCol_ButtonActive, Color::FrameActive);
		PushStyleColor(ImGuiCol_Header, Color::GoldWash);
		PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(214, 172, 98, 60));
		PushStyleColor(ImGuiCol_HeaderActive, Color::GoldWashStrong);
		PushStyleColor(ImGuiCol_Text, Color::Text);
		PushStyleColor(ImGuiCol_TextDisabled, Color::TextFaint);
		PushStyleColor(ImGuiCol_Separator, Color::Border);
		PushStyleColor(ImGuiCol_SliderGrab, Color::Gold);
		PushStyleColor(ImGuiCol_SliderGrabActive, Color::GoldBright);
		PushStyleColor(ImGuiCol_CheckMark, Color::Gold);
		PushStyleColor(ImGuiCol_ScrollbarBg, Color::Clear);
		PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(90, 98, 116, 150));
		PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(120, 130, 150, 200));
		PushStyleColor(ImGuiCol_ScrollbarGrabActive, Color::Gold);
		PushStyleColor(ImGuiCol_TextSelectedBg, IM_COL32(214, 172, 98, 90));
		PushStyleColor(ImGuiCol_NavHighlight, Color::GoldBright);
		PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 120));
		PushStyleColor(ImGuiCol_ResizeGrip, Color::Clear);
		PushStyleColor(ImGuiCol_ResizeGripHovered, Color::Clear);
		PushStyleColor(ImGuiCol_ResizeGripActive, Color::Clear);

		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ u * 0.8f, u * 0.75f });
		PushStyleVar(ImGuiStyleVar_WindowRounding, u * 0.35f);
		PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		PushStyleVar(ImGuiStyleVar_ChildRounding, u * 0.3f);
		PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_PopupRounding, u * 0.3f);
		PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ u * 0.5f, u * 0.3f });
		PushStyleVar(ImGuiStyleVar_FrameRounding, u * 0.25f);
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ u * 0.45f, u * 0.4f });
		PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ u * 0.35f, u * 0.3f });
		PushStyleVar(ImGuiStyleVar_ScrollbarSize, u * 0.55f);
		PushStyleVar(ImGuiStyleVar_ScrollbarRounding, u * 0.3f);
		PushStyleVar(ImGuiStyleVar_GrabMinSize, u * 0.6f);
		PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2{ 0.0f, 0.5f });
		PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
	}

	void PopStyle()
	{
		ImGuiMCP::PopStyleVar(kVars);
		ImGuiMCP::PopStyleColor(kColors);
	}

	void PushIconFont()
	{
		SKSEMenuFramework::PushFont(Icon::Font);
	}

	void PopIconFont()
	{
		FontAwesome::Pop();
	}

	ImVec2 IconSize(const char* a_icon)
	{
		PushIconFont();
		const auto size = ImGuiMCP::CalcTextSize(a_icon);
		PopIconFont();
		return size;
	}

	void DrawIcon(ImGuiMCP::ImDrawList* a_list, ImVec2 a_center, const char* a_icon, ImU32 a_color)
	{
		PushIconFont();
		const auto size = ImGuiMCP::CalcTextSize(a_icon);
		ImGuiMCP::ImDrawListManager::AddText(a_list, { std::floor(a_center.x - size.x * 0.5f), std::floor(a_center.y - size.y * 0.5f) }, a_color, a_icon);
		PopIconFont();
	}
}
