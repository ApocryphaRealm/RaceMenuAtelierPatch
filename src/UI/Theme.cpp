#include "UI/Theme.h"

#include "Settings.h"

namespace RMA::UI
{
	namespace
	{
		constexpr int kColors = 30;
		constexpr int kVars = 17;
	}

	namespace
	{
		std::string_view Trim(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) {
				a_text.remove_prefix(1);
			}
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) {
				a_text.remove_suffix(1);
			}
			return a_text;
		}

		// #RRGGBB or #RRGGBBAA
		std::optional<ImU32> ParseColor(std::string_view a_text)
		{
			a_text = Trim(a_text);
			if (!a_text.empty() && a_text.front() == '#') {
				a_text.remove_prefix(1);
			}
			if (a_text.size() != 6 && a_text.size() != 8) {
				return std::nullopt;
			}
			std::uint32_t v = 0;
			const auto [end, ec] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), v, 16);
			if (ec != std::errc{} || end != a_text.data() + a_text.size()) {
				return std::nullopt;
			}
			if (a_text.size() == 6) {
				return IM_COL32((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, 255);
			}
			return IM_COL32((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
		}

		ImU32* ColorSlot(std::string_view a_key)
		{
			static const std::pair<std::string_view, ImU32*> slots[] = {
				{ "Window", &Color::Window }, { "Panel", &Color::Panel }, { "PanelHover", &Color::PanelHover },
				{ "Row", &Color::Row }, { "RowAlt", &Color::RowAlt }, { "RowHover", &Color::RowHover },
				{ "RowBorder", &Color::RowBorder }, { "Frame", &Color::Frame }, { "FrameHover", &Color::FrameHover },
				{ "FrameActive", &Color::FrameActive }, { "Border", &Color::Border }, { "BorderStrong", &Color::BorderStrong },
				{ "Track", &Color::Track }, { "Text", &Color::Text }, { "TextDim", &Color::TextDim },
				{ "TextFaint", &Color::TextFaint }, { "Value", &Color::Value }, { "Accent", &Color::Gold },
				{ "AccentBright", &Color::GoldBright }, { "AccentDim", &Color::GoldDim }, { "AccentWash", &Color::GoldWash },
				{ "AccentWashStrong", &Color::GoldWashStrong }, { "Danger", &Color::Danger }, { "Ok", &Color::Ok },
				{ "Shadow", &Color::Shadow }, { "PopupBg", &Color::PopupBg }, { "HeaderHover", &Color::HeaderHover },
				{ "ScrollGrab", &Color::ScrollGrab }, { "ScrollGrabHover", &Color::ScrollGrabHover },
				{ "TextSelected", &Color::TextSelected }, { "ModalDim", &Color::ModalDim }, { "TileText", &Color::TileText },
				{ "OnAccent", &Color::OnAccent }, { "KnobHover", &Color::KnobHover },
			};
			for (const auto& [name, slot] : slots) {
				if (name == a_key) {
					return slot;
				}
			}
			if (a_key.size() == 4 && a_key.starts_with("Hue") && a_key[3] >= '0' && a_key[3] <= '9') {
				return &Color::Hues[static_cast<std::size_t>(a_key[3] - '0')];
			}
			return nullptr;
		}
	}

	void LoadTheme()
	{
		constexpr const char* kThemePath = "Data\\SKSE\\Plugins\\RaceMenuAtelier\\theme.ini";
		std::ifstream in(kThemePath);
		if (!in) {
			logger::info("no theme file ({}); Atelier's own colours", kThemePath);
			return;
		}
		std::string line;
		std::string name = "(unnamed)";
		int         applied = 0;
		while (std::getline(in, line)) {
			const auto eq = line.find('=');
			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[' || eq == std::string::npos) {
				continue;
			}
			const auto key = Trim(std::string_view(line).substr(0, eq));
			const auto value = Trim(std::string_view(line).substr(eq + 1));
			if (key == "sName") {
				name = std::string(value);
			} else if (key == "fRounding") {
				float f = 1.0f;
				if (std::from_chars(value.data(), value.data() + value.size(), f).ec == std::errc{}) {
					RoundingScale = std::clamp(f, 0.0f, 2.0f);
					++applied;
				}
			} else if (auto* slot = ColorSlot(key)) {
				if (const auto color = ParseColor(value)) {
					*slot = *color;
					++applied;
				} else {
					logger::warn("theme: {} = '{}' is not #RRGGBB or #RRGGBBAA", key, value);
				}
			} else {
				logger::warn("theme: unknown key '{}'", key);
			}
		}
		logger::info("theme '{}' loaded from {}: {} value(s)", name, kThemePath, applied);
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
		PushStyleColor(ImGuiCol_PopupBg, Color::PopupBg);
		PushStyleColor(ImGuiCol_Border, Color::Border);
		PushStyleColor(ImGuiCol_BorderShadow, Color::Clear);
		PushStyleColor(ImGuiCol_FrameBg, Color::Frame);
		PushStyleColor(ImGuiCol_FrameBgHovered, Color::FrameHover);
		PushStyleColor(ImGuiCol_FrameBgActive, Color::FrameActive);
		PushStyleColor(ImGuiCol_Button, Color::Frame);
		PushStyleColor(ImGuiCol_ButtonHovered, Color::FrameHover);
		PushStyleColor(ImGuiCol_ButtonActive, Color::FrameActive);
		PushStyleColor(ImGuiCol_Header, Color::GoldWash);
		PushStyleColor(ImGuiCol_HeaderHovered, Color::HeaderHover);
		PushStyleColor(ImGuiCol_HeaderActive, Color::GoldWashStrong);
		PushStyleColor(ImGuiCol_Text, Color::Text);
		PushStyleColor(ImGuiCol_TextDisabled, Color::TextFaint);
		PushStyleColor(ImGuiCol_Separator, Color::Border);
		PushStyleColor(ImGuiCol_SliderGrab, Color::Gold);
		PushStyleColor(ImGuiCol_SliderGrabActive, Color::GoldBright);
		PushStyleColor(ImGuiCol_CheckMark, Color::Gold);
		PushStyleColor(ImGuiCol_ScrollbarBg, Color::Clear);
		PushStyleColor(ImGuiCol_ScrollbarGrab, Color::ScrollGrab);
		PushStyleColor(ImGuiCol_ScrollbarGrabHovered, Color::ScrollGrabHover);
		PushStyleColor(ImGuiCol_ScrollbarGrabActive, Color::Gold);
		PushStyleColor(ImGuiCol_TextSelectedBg, Color::TextSelected);
		PushStyleColor(ImGuiCol_NavHighlight, Color::GoldBright);
		PushStyleColor(ImGuiCol_ModalWindowDimBg, Color::ModalDim);
		PushStyleColor(ImGuiCol_ResizeGrip, Color::Clear);
		PushStyleColor(ImGuiCol_ResizeGripHovered, Color::Clear);
		PushStyleColor(ImGuiCol_ResizeGripActive, Color::Clear);

		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ u * 0.8f, u * 0.75f });
		PushStyleVar(ImGuiStyleVar_WindowRounding, Round(u * 0.35f));
		PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		PushStyleVar(ImGuiStyleVar_ChildRounding, Round(u * 0.3f));
		PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_PopupRounding, Round(u * 0.3f));
		PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ u * 0.5f, u * 0.3f });
		PushStyleVar(ImGuiStyleVar_FrameRounding, Round(u * 0.25f));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ u * 0.45f, u * 0.4f });
		PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ u * 0.35f, u * 0.3f });
		PushStyleVar(ImGuiStyleVar_ScrollbarSize, u * 0.55f);
		PushStyleVar(ImGuiStyleVar_ScrollbarRounding, Round(u * 0.3f));
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
