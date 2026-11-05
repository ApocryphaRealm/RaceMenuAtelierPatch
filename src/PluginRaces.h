#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// 1.2.0: the race records a plugin file itself holds, as written in that file, whichever plugin wins in the game. No
// game calls in here (so it can be tried on real plugins outside the game): RaceInfo.cpp maps the ids to the running game.
namespace RMA::PluginRaces
{
	struct Race
	{
		std::uint32_t              formID{ 0 };    // as written: the top byte indexes the file's masters, else the file itself
		std::string                editorID;
		std::vector<std::uint32_t> spellList;      // SPLO, as written
		std::string                description;    // DESC when the file is not localized, in the game's code page
		std::uint32_t              stringID{ 0 };  // DESC when the file is localized (0 = none)
	};

	struct File
	{
		bool                     readable{ false };
		bool                     localized{ false };
		std::vector<std::string> masters;
		std::vector<Race>        races;
	};

	// Reads only the header and the file's top-level RACE group, compressed records included. Never throws.
	[[nodiscard]] File Read(const std::string& a_path);

	// One entry of a .DLSTRINGS file (a_data is the whole file); "" when the id is not there.
	[[nodiscard]] std::string LookupDLString(std::string_view a_data, std::uint32_t a_id);
}
