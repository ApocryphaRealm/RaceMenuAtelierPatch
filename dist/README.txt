RaceMenu Atelier
Version 1.0.1
Based on upstream RaceMenu Atelier 1.0.0, with the Apprentice patch and the Norden Black theme.

A continuation of RustyOldSword's (formerly emberchain's) RaceMenu Atelier, GPL-3.0, released on Nexus Mods with the
approval of Nexus Mods staff. This package is the whole mod: it replaces RaceMenu Atelier.
Source: https://github.com/ApocryphaRealm/RaceMenuAtelierPatch

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

Controller
----------
As in RaceMenu:
- Left stick left / right: move between the editor on the left (categories and sliders) and the scene and presets
  panel on the right, once per push.
- Right stick: turns the character. Right stick click (R3): close-up on the face and back.
- D-pad, two columns: in the category list up / down choose the category (its content shows beside it) and right
  enters that content, on the item last highlighted there (or the current race or choice). In the content up / down
  move between items; on a slider or option row left / right change the value and never leave the row; on any other
  row left moves left one item at a time, and a further press at the far left returns to the category. B also
  returns to the category list. No area is ever framed or entered with A; only the highlighted item is marked.
- A activates (picks a race, presses a button); B closes a popup or the confirmation.
- L2 / R2 step through the categories.
- X (PlayStation Square): Done, as R on the keyboard (switchable in Options).
- The Class and Trait categories come directly after Race.

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
