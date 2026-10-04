# RaceMenu Atelier - Apprentice Patch

> **Note:** emberchain, the author of RaceMenu Atelier, has permission to incorporate this patch into RaceMenu
> Atelier. When they do, this patch will be taken down.

A patched build of [RaceMenu Atelier](https://www.nexusmods.com/skyrimspecialedition/mods/193865) (by emberchain,
GPL-3.0, [emberchain/RaceMenuAtelier](https://github.com/emberchain/RaceMenuAtelier) at `8db73c5`, its 1.0.0) that makes
it work with [Apprentice - A Class Overhaul](https://www.nexusmods.com/skyrimspecialedition/mods/169288) (by Simon Magus
and LambdaCDM). This repository is a fork: the upstream history is kept, and every change is a commit on top of it.

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
  file exists (the "RaceMenu Atelier - Norden Black" patch ships one); without it Atelier looks exactly as before.
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
