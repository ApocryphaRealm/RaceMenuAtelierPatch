# RaceMenu Atelier

A private continuation of RustyOldSword's (formerly emberchain's) RaceMenu Atelier, with the Apprentice patch and the
Norden Black theme. Kept as one of our own mods because the upstream mod was removed from Nexus (the owner,
2026-10-13); it is not posted anywhere.

- Upstream: RaceMenu Atelier by RustyOldSword (formerly emberchain), GPL-3.0 -
  [rustyoldsword/RaceMenuAtelier](https://github.com/rustyoldsword/RaceMenuAtelier) (current, 1.0.3) and
  [emberchain/RaceMenuAtelier](https://github.com/emberchain/RaceMenuAtelier) (the old repository, to 1.0.1). It was on
  Nexus as mod 193865 until Nexus staff removed it; it is also on dwemermods.com (mod 4840).
- `main` is the proven build: Atelier 1.0.0 (`8db73c5`) plus the Apprentice patch below, version 1.0.0, DLL SHA-256
  `2409DA145E3B601AE5D46BE50A624CB36AEA491C75C98E00645B8226AB1F3A6E`, shipped with the Norden Black theme
  (`themes/norden-black.ini`, installed as `SKSE\Plugins\RaceMenuAtelier\theme.ini`).
- Branch `merge-upstream-1.0.1` holds the merge of upstream 1.0.1 and a build-path flag; not yet tested in game.

The patch makes Atelier work with [Apprentice - A Class Overhaul](https://www.nexusmods.com/skyrimspecialedition/mods/169288)
(by Simon Magus and LambdaCDM). The upstream history is kept, and every change is a commit on top of it.

## The problem it fixes

Apprentice adds its classes and traits to RaceMenu's race list: two categories (`$APPCLASS`, `$APPTRAIT`) and one
race-type row per class and trait, each filed under its own category flag and carrying an `isClass` / `isTrait` mark
and its class number in `raceID`. It replaces RaceMenu's item-press handler, so a click on such a row records the pick
(sent as `ClassMenu_Callback` / `TraitMenu_Callback` when the menu closes).

Atelier 1.0.0 drew every race-type row in one race grid, whatever its category - classes and traits sat in the pool
with the races - and a click called `ChangeRace(raceID)` directly. A class's `raceID` is its class number, so choosing
"Agent" (0) turned the character into the race at index 0 (Argonian) and recorded no class.

## What the patch changes

- A race-type entry outside RaceMenu's Race category is a **choice** of the category it is filed under. Each such
  category gets its own tiles (with the entry's description as the tooltip), the race grid holds only races, and a
  click goes through the menu's own `onItemPress({index})` - Apprentice's handler - never `ChangeRace`. The current
  pick is marked, read from the class/trait values Apprentice shows in RaceMenu's bottom bar. Nothing in this is named
  after Apprentice except that read, so any mod that adds a list the same way is handled the same way.
- **Themes**: Atelier's colours and corner radius are read from `SKSE\Plugins\RaceMenuAtelier\theme.ini` when that
  file exists (this mod ships the Norden Black theme as that file); without it Atelier looks exactly as before.
- **DevBench tool `atelier.control`** (for testing): the categories, the choice lists with the current pick, `choose`,
  `race`, `category`, `refresh`.

Everything else is Atelier 1.0.0 unchanged.

## Building

Visual Studio 2022+ (C++ workload) and xmake 3.0+:

```bat
git submodule update --init --recursive
xmake f -m releasedbg --skyrim_vr=n
xmake build RaceMenuAtelier
```

The DLL is `build\windows\x64\releasedbg\RaceMenuAtelier.dll` - the same file name as Atelier's, so the patch replaces it.

## Licence

GPL-3.0, as RaceMenu Atelier (see `LICENSE` and `NOTICE.md`). `lib/SKSE-Menu-Framework-3-API` keeps its LGPL-2.1 licence;
`src/DevBench/DevBenchAPI.*` is MIT (`src/DevBench/DevBenchAPI.LICENSE.txt`).
