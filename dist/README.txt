RaceMenu Atelier - Apprentice Patch
Version 1.0.0

NOTE FROM THE AUTHOR: emberchain, the author of RaceMenu Atelier, has my permission to incorporate this patch into
RaceMenu Atelier. When they do, this patch will be taken down.

Makes RaceMenu Atelier work with Apprentice - A Class Overhaul. Classes and traits get their own tiles under the CLASS
and TRAIT categories instead of sitting in the race list, and choosing one records it with Apprentice exactly as
RaceMenu's own menu does. Without the patch, choosing a class in Atelier changed your race instead (the class's number
was taken as a race) and no class was recorded.

Requirements
------------
- RaceMenu Atelier 1.0.0 (and its requirements: SKSE64, Address Library for SKSE Plugins, RaceMenu, SKSE Menu Framework 3
  or Apocrypha Menu Framework)
- Apprentice - A Class Overhaul (the patch does nothing harmful without it: there are simply no class or trait tiles)

Install
-------
Install with Mod Organizer 2 and place it BELOW RaceMenu Atelier (it must win the conflict): it replaces
SKSE\Plugins\RaceMenuAtelier.dll with the patched build. Your RaceMenuAtelier.ini is not touched. No plugin to sort.

Compatible with "RaceMenu Atelier - Norden Black": both ship the same patched DLL, so either can sit above the other.

Uninstall: remove the patch; Atelier's own DLL is used again.

What it changes
---------------
- CLASS and TRAIT: each category Apprentice adds shows its own tiles, with the description as the tooltip; the current
  choice is marked. The race list shows races only.
- A class or trait tile goes through Apprentice's own handler; Apprentice applies the choice when the menu closes, as
  it always does.
- Theme support: if SKSE\Plugins\RaceMenuAtelier\theme.ini exists, Atelier's colours come from it (the Norden Black
  patch ships one). Without it Atelier looks as it always did.

Credits
-------
RaceMenu Atelier by emberchain (GPL-3.0) - this is a modified build of it, with its source and history at
https://github.com/ApocryphaRealm/RaceMenuAtelierPatch. Apprentice - A Class Overhaul by Simon Magus and LambdaCDM.
RaceMenu by expired6978. SKSE Menu Framework by Thiago and Quantumyilmaz.

Licence: GPL-3.0 (LICENSE). Modified by ApocryphaRealm.
