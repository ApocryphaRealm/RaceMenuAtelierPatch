RaceMenu Atelier
Version 1.0.0 (upstream RaceMenu Atelier 1.0.0, with the Apprentice patch and the Norden Black theme)

PRIVATE: not posted anywhere. A private continuation of RustyOldSword's (formerly emberchain's) RaceMenu Atelier, kept
because the upstream mod was removed from Nexus. This package is the whole mod: it replaces RaceMenu Atelier.

Makes RaceMenu Atelier work with Apprentice - A Class Overhaul. Classes and traits get their own tiles under the CLASS
and TRAIT categories instead of sitting in the race list, and choosing one records it with Apprentice exactly as
RaceMenu's own menu does. Without the patch, choosing a class in Atelier changed your race instead (the class's number
was taken as a race) and no class was recorded.

Requirements
------------
- SKSE64, Address Library for SKSE Plugins, RaceMenu, SKSE Menu Framework 3 or Apocrypha Menu Framework
- Apprentice - A Class Overhaul is optional (without it there are simply no class or trait tiles)

Install
-------
Install with Mod Organizer 2 in place of RaceMenu Atelier (disable any other copy of it): the package holds
SKSE\Plugins\RaceMenuAtelier.dll, Atelier's own RaceMenuAtelier.ini and the Norden Black theme.ini. No plugin to sort.

What it changes
---------------
- CLASS and TRAIT: each category Apprentice adds shows its own tiles, with the description as the tooltip; the current
  choice is marked. The race list shows races only.
- A class or trait tile goes through Apprentice's own handler; Apprentice applies the choice when the menu closes, as
  it always does.
- Theme support: if SKSE\Plugins\RaceMenuAtelier\theme.ini exists, Atelier's colours come from it (this package
  ships the Norden Black theme as that file). Without it Atelier looks as it always did.

Theme
-----
SKSE\Plugins\RaceMenuAtelier\theme.ini is the Norden Black theme (black panels, silver lines, square corners). Delete
it to get Atelier's own colours back.

Credits
-------
RaceMenu Atelier by RustyOldSword, formerly emberchain (GPL-3.0) - this is a modified build of it, with its source and history at
https://github.com/ApocryphaRealm/RaceMenuAtelierPatch. Apprentice - A Class Overhaul by Simon Magus and LambdaCDM.
RaceMenu by expired6978. SKSE Menu Framework by Thiago and Quantumyilmaz.

Licence: GPL-3.0 (LICENSE). Modified by ApocryphaRealm.
