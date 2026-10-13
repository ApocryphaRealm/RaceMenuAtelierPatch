# RaceMenu Atelier - changelog

A fork of RaceMenu Atelier 1.0.0 (emberchain). Written as changes happen (rule 61); a version number is issued by the
version gate only once a build is seen working in game (rule 48).

## 1.0.1 - 2026-10-13 - untested

Asked for by the owner, 2026-10-13, playing with a controller: "I want the trait and class categories to be directly
after the race category"; "when you press X with controller, it will automatically press the done button, just like
the original race menu"; and, after trying the first build (L1/R1 window switching with a gold outline), "I want the
left stick to change focus from the left to the right pane, and I want the right stick to be character rotation, just
like the original race menu", with every item reachable by the D-pad and no white frame to enter and leave with A and B.

### Changed
- Category order: the Class and Trait categories (any category holding another mod's choices) now come directly after
  Race in the category rail, and the controller triggers step through them in that order. The rest keep RaceMenu's
  order, and each category keeps its colour.
- Controller navigation, as in RaceMenu: the D-pad moves freely over every item of a window - categories, sliders,
  race and class cards, presets - with no area to enter with A and leave with B, and no white frame round an area; only
  the highlighted item shows. A activates, B backs out of a popup.
- D-pad left/right on a slider steps it (by its interval, repeating while held), and on an option row (hair, eyes and
  other parts) moves to the previous or next option; up and down move between items. A on a slider no longer jumps its
  value to the mouse pointer. The step arrows and the typed value stay for the mouse and take no D-pad stop.

### Added
- Controller X (PlayStation Square) presses Done, as R does on the keyboard and as in RaceMenu: with "Ask before
  finishing the character" on (the default) it opens the same confirmation, otherwise it finishes at once. Switchable
  in Options ("Controller X finishes the character", `bGamepadDone` in RaceMenuAtelier.ini, on by default).
- Left stick left/right switches between the two windows - left the editor (categories and sliders), right the scene
  and presets panel (or its tab when the panel is hidden) - once per push, not repeating while held. Up and down on the
  left stick do nothing. In RaceMenu's own interface (F4) the left stick is the game's as before.

### Fixed
- Controller: the right stick turns the character while the Atelier is open, as in RaceMenu (full tilt at the Q/E
  speed; not while a slider is held with A, where the menu framework gives the right stick to that slider). The menu
  framework keeps every stick event from the game while the editor holds the input, so RaceMenu and rotation mods such
  as Racemenu Rotation with Gamepad Support never saw it (in 1.0.0 as well); the editor now reads the right stick
  itself and never claims it. Reported by the owner, 2026-10-13: the character's rotation was not responding to the
  sticks.

### Build
- Built through the shared build environment with `/d1trimfile`: no build-machine path in the DLL, no user folder in
  the PDB.

## Private continuation - 2026-10-13

The owner, 2026-10-13: "because the mod is removed from nexus, keep a copy of rma as one of our mods and give it a
distro folder, we won't post it anywhere but we might develop it or patch it as needed as a private repo". So this
repository is now RaceMenu Atelier itself, kept as our own mod and not posted: the proven 1.0.0 build below, shipped
with the Norden Black theme (`themes/norden-black.ini`, back in this repository). No code changed; the RaceMenu Atelier
1.0.1 merge waits on branch `merge-upstream-1.0.1`.

## 1.0.0 - 2026-10-04 - working

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
  the corners. The Norden Black patch ships one (repo RaceMenuAtelierNordenBlack, `themes/norden-black.ini`).
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
- The package itself (2026-10-04, installed from 7. current test builds): no theme file -> "no theme file ...; Atelier's
  own colours" and Atelier's own look; the race grid 18 races; `choose Battlemage` -> Apprentice "Selected Class
  callback: CLASS005", the race stayed Nord DZ.
- Not yet seen: the trait tracker global read back (the console capture of a second command came back empty), and the
  theme in the owner's eyes at full resolution.
