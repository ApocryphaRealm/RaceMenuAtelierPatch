# Notice

This is a modified version of **RaceMenu Atelier** by **RustyOldSword** (formerly **emberchain**)
(https://github.com/rustyoldsword/RaceMenuAtelier, formerly https://github.com/emberchain/RaceMenuAtelier; once
https://www.nexusmods.com/skyrimspecialedition/mods/193865, removed by Nexus staff), licensed GPL-3.0. Copyright in
RaceMenu Atelier stays with its author. The modifications (choice categories for lists other mods add to RaceMenu, the theme file, the DevBench tool)
are by ApocryphaRealm, 2026, under the same licence. The upstream commit history is kept in this repository.

Made to work with **Apprentice - A Class Overhaul** by Simon Magus and LambdaCDM
(https://www.nexusmods.com/skyrimspecialedition/mods/169288). No Apprentice file is included or required at build
time; the patch reads only what Apprentice puts into RaceMenu's menu at run time.

The Norden Black colour values (`themes/norden-black.ini`, shipped as `SKSE\Plugins\RaceMenuAtelier\theme.ini`) follow **Norden UI** by Nithog
(https://www.nexusmods.com/skyrimspecialedition/mods/166086) as recoloured by Norden UI - Black. They are numbers only;
no Norden art is included.

Also included:
- SKSE Menu Framework 3 API header (`lib/SKSE-Menu-Framework-3-API`), LGPL-2.1, Thiago and Quantumyilmaz.
- CommonLibSSE-NG (`lib/CommonLibSSE-NG`), its own licence.
- devbench cross-plugin API glue (`src/DevBench/DevBenchAPI.*`), MIT, Alan Tse and devbench contributors.
- RaceMenu by expired6978 is required at run time and not included.
