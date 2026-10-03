#include "Settings.h"

namespace RMA
{
	namespace
	{
		constexpr auto        kPath = "Data\\SKSE\\Plugins\\RaceMenuAtelier.ini";
		constexpr std::size_t kRecentColors = 10;

		std::string Trimmed(std::string_view a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r");
			if (first == std::string_view::npos) {
				return {};
			}
			const auto last = a_text.find_last_not_of(" \t\r");
			return std::string(a_text.substr(first, last - first + 1));
		}

		bool ParseBool(const std::string& a_value, bool a_fallback)
		{
			if (a_value == "1" || a_value == "true" || a_value == "True") {
				return true;
			}
			if (a_value == "0" || a_value == "false" || a_value == "False") {
				return false;
			}
			return a_fallback;
		}

		float ParseFloat(const std::string& a_value, float a_fallback, float a_min, float a_max)
		{
			char*       end = nullptr;
			const float v = std::strtof(a_value.c_str(), &end);
			return end != a_value.c_str() && std::isfinite(v) ? std::clamp(v, a_min, a_max) : a_fallback;
		}
	}

	Settings& Settings::Get()
	{
		static Settings settings;
		return settings;
	}

	void Settings::Load()
	{
		std::ifstream in(kPath);
		if (!in) {
			return;
		}
		std::string line;
		while (std::getline(in, line)) {
			const auto eq = line.find('=');
			if (line.empty() || line[0] == ';' || line[0] == '[' || eq == std::string::npos) {
				continue;
			}
			const auto key = Trimmed(std::string_view(line).substr(0, eq));
			const auto value = Trimmed(std::string_view(line).substr(eq + 1));
			if (key == "fUIScale") {
				uiScale = ParseFloat(value, uiScale, 0.6f, 2.0f);
			} else if (key == "fEditorWidth") {
				editorWidth = ParseFloat(value, editorWidth, 0.2f, 0.5f);
			} else if (key == "bSidePanel") {
				sidePanel = ParseBool(value, sidePanel);
			} else if (key == "bSexFilter") {
				sexFilter = ParseBool(value, sexFilter);
			} else if (key == "bOverdrive") {
				overdrive = ParseBool(value, overdrive);
			} else if (key == "bConfirmDone") {
				confirmDone = ParseBool(value, confirmDone);
			} else if (key == "bGroupHeaders") {
				groupHeaders = ParseBool(value, groupHeaders);
			} else if (key == "bShowTechnicalNames") {
				showTechnicalNames = ParseBool(value, showTechnicalNames);
			} else if (key == "fRotateSpeed") {
				rotateSpeed = ParseFloat(value, rotateSpeed, 0.05f, 2.0f);
			} else if (key == "iToggleKey") {
				toggleKey = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 0));
			} else if (key == "sRecentColors") {
				recentColors.clear();
				std::size_t start = 0;
				while (start < value.size() && recentColors.size() < kRecentColors) {
					const auto end = value.find(',', start);
					const auto token = Trimmed(std::string_view(value).substr(start, end == std::string::npos ? std::string::npos : end - start));
					if (!token.empty()) {
						recentColors.push_back(static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 16)));
					}
					if (end == std::string::npos) {
						break;
					}
					start = end + 1;
				}
			}
		}
	}

	void Settings::Save() const
	{
		std::ofstream out(kPath, std::ios::trunc);
		if (!out) {
			logger::warn("could not write {}", kPath);
			return;
		}
		std::string colors;
		for (const auto color : recentColors) {
			colors += std::format("{}{:08X}", colors.empty() ? "" : ",", color);
		}
		out << "[General]\n";
		out << "; size of the editor relative to the SKSE Menu Framework font\n";
		out << "fUIScale = " << uiScale << "\n";
		out << "; share of the screen width used by the editor panel (0.2 - 0.5)\n";
		out << "fEditorWidth = " << editorWidth << "\n";
		out << "bSidePanel = " << (sidePanel ? 1 : 0) << "\n";
		out << "; hide body sliders that clearly belong to the other body type\n";
		out << "bSexFilter = " << (sexFilter ? 1 : 0) << "\n";
		out << "; let typed values go past the registered range of custom body morphs\n";
		out << "bOverdrive = " << (overdrive ? 1 : 0) << "\n";
		out << "bConfirmDone = " << (confirmDone ? 1 : 0) << "\n";
		out << "bGroupHeaders = " << (groupHeaders ? 1 : 0) << "\n";
		out << "bShowTechnicalNames = " << (showTechnicalNames ? 1 : 0) << "\n";
		out << "; degrees per pixel when dragging the character\n";
		out << "fRotateSpeed = " << rotateSpeed << "\n";
		out << "; DirectInput scan code of the key that switches to RaceMenu's own interface (0x3E = F4)\n";
		out << "iToggleKey = " << std::format("0x{:X}", toggleKey) << "\n";
		out << "sRecentColors = " << colors << "\n";
	}

	void Settings::AddRecentColor(std::uint32_t a_argb)
	{
		std::erase_if(recentColors, [&](std::uint32_t c) { return (c & 0xFFFFFF) == (a_argb & 0xFFFFFF); });
		recentColors.insert(recentColors.begin(), a_argb);
		if (recentColors.size() > kRecentColors) {
			recentColors.resize(kRecentColors);
		}
	}
}
