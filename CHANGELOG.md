# RaceMenu Atelier - Apprentice Patch - changelog

A fork of RaceMenu Atelier 1.0.0 (emberchain). Written as changes happen (rule 61); a version number is issued by the
version gate only once a build is seen working in game (rule 48).

## Unreleased

Asked for by the owner, 2026-10-04: Apprentice - A Class Overhaul's categories show in Atelier, "but their contents are
mixed with the race category. So you'd have races and classes in the same pool of options, which is wrong."

### Fixed
- Apprentice's classes and traits are no longer mixed into the race list. A race-type entry RaceMenu files outside its
  Race category is a choice of its own category: the CLASS and TRAIT categories show their own tiles, with the class or
  trait description as the tooltip, and the race grid holds only races.
- Choosing a class or trait records it with Apprentice. Atelier 1.0.0 called ChangeRace with the entry's raceID, which
  for a class is its class number - "Agent" turned the character into an Argonian and no class was recorded. A choice
  now goes through the menu's own item-press handler (Apprentice's), and the current pick is marked.

### Added
- Theme file: when `SKSE\Plugins\RaceMenuAtelier\theme.ini` exists, Atelier's colours and corner radius come from it;
  without it nothing changes. Every colour Atelier draws with is a named key (#RRGGBB or #RRGGBBAA), `fRounding` scales
  the corners. The Norden Black patch ships one (`themes\norden-black.ini`).
- DevBench tool `atelier.control`: state (categories, choice lists with the current pick, races, the bottom bar's
  picks), choose, race, category, refresh.

### Tested (2026-10-04, SE 1.5.97, Njordlinger Test, Apprentice 1.1.0, RaceMenu Atelier 1.0.0 underneath)
- RaceMenu's lists read live (TestBench `menu action=gfx`): races filterFlag 2, Apprentice's classes 2^29 and traits
  2^30, each class's raceID its class number (Agent 0 = Argonian's index).
- Patched: the race grid shows the 18 races only; CLASS shows the 18 classes, TRAIT the 32 traits. `choose Agent` and
  `choose Stormborn`: Apprentice logged "Selected Class callback: CLASS001" and "Selected Trait callback: TRAIT027" and
  updated its bottom bar; the race stayed Nord DZ; both tiles marked current. On closing the menu Apprentice sent
  ClassMenu_Callback CLASS001 and TraitMenu_Callback TRAIT027, and MAG_ClassTracker read 1.
- Norden Black theme.ini: logged "theme 'Norden Black' loaded ... 42 value(s)"; the editor drew black panels, silver
  lines and accents, square corners (frames seen at the CLASS and FACE categories).
- Not yet seen: the trait tracker global read back (the console capture of a second command came back empty), and the
  theme in the owner's eyes at full resolution.
