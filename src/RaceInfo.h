#pragma once

#include "Model.h"

// 1.2.0 (the owner, in game, 2026-11-05: the editor never says what a race is). What the race panel shows, read from the
// game's live TESRace after data load, never from a table of ours: a mod that changes a race, its spell list or its
// text - an override plugin, SkyPatcher at load, a race mod of any kind - shows up by itself.
namespace RMA::RaceData
{
	// Game thread (a UI task), after data load. a_name is the name RaceMenu's list carries for the race; a_listDescription
	// the description the list carries, used only when no race of that name is found or the record has none.
	[[nodiscard]] RaceInfo Read(std::string_view a_name, std::string_view a_listDescription);

	// The text as the game would show it, as UTF-8: <mag>, <dur> and <area> from a_effect when there is one, <Global=X>
	// from the global's value, <25> as the plain number, every other tag (<font ...>) dropped, <br> a line break.
	[[nodiscard]] std::string Clean(std::string_view a_text, const RE::Effect* a_effect = nullptr);
}
