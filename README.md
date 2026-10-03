# RaceMenu Atelier

A native C++ interface for RaceMenu, drawn with Dear ImGui through
[SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352).

RaceMenu itself is left untouched. Its Scaleform menu stays loaded underneath, hidden, and is used as
the data source and action target: Atelier reads the categories and sliders RaceMenu exposes and sends
every change back through RaceMenu's own delegate calls, mod events and `CharGen` functions. Press
**F4** in the editor to switch to RaceMenu's own interface and back at any time.

## Features

- One searchable editor for every slider, with a "changed only" filter, undo/redo and per-slider reset
- Typed values, optional overdrive past the registered range of custom body morphs
- Colour editor with palette and recent colours; texture browser for tints, paints and overlays
- Head part browser with search by name, form ID or plugin
- Scene panel: drag to rotate, wheel to zoom (face / full body), light toggle, undress / redress,
  pose freeze, a small set of idle poses
- Preset browser that shows which plugins, head parts and tint textures a `.jslot` uses before loading it
- BodySlide preset export (CBBE 3BA, HIMBO) straight from the current morphs
- No ESP, no Papyrus, no replaced RaceMenu files

## Requirements

- Skyrim SE / AE with SKSE
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [RaceMenu](https://www.nexusmods.com/skyrimspecialedition/mods/19080)
- [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) 3.x and its requirements

## Building

Requires Visual Studio 2022 (C++ workload) and [xmake](https://xmake.io) 3.0+.

```bat
git clone --recursive https://github.com/emberchain/RaceMenuAtelier.git
cd RaceMenuAtelier
xmake f -m releasedbg --skyrim_vr=n
xmake build RaceMenuAtelier
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

`tools\package.ps1` stages `dist\RaceMenu Atelier\` in the Data layout and packs a `.7z` when 7-Zip
is installed. Set `RMA_DEPLOY_DIR` to a mod folder to have every build copied there.

## Settings

`Data\SKSE\Plugins\RaceMenuAtelier.ini` - most options are also editable from the Options popup in game.

## Credits

- expired6978 - RaceMenu
- Thiago and Quantumyilmaz - SKSE Menu Framework
- The SKSE team, CommonLibSSE-NG contributors, Dear ImGui

## License

GPL-3.0, see [LICENSE](LICENSE). `lib/SKSE-Menu-Framework-3-API` keeps its own LGPL-2.1 license.
