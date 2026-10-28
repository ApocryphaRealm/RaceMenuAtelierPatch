# RaceMenu Atelier - changelog

A fork of RaceMenu Atelier 1.0.0 (emberchain). Written as changes happen (rule 61); a version number is issued by the
version gate only once a build is seen working in game (rule 48).

## 1.1.0 - 2026-10-27 - untested

(1.0.2, the Cyrillic fix below, was never posted; it is folded into 1.1.0 because translating the editor is new functionality.)

### Added
- The editor's own text is in the game's language (the owner, 2026-10-27: does the rest of Atelier's menu change language?). Until now only what the game and RaceMenu hand over - slider, category and race names - followed the language; every button, tab, header, tooltip, confirmation, the preset panel, the options, the status line and the banners were English. All 160 of Atelier's own strings are now in English, Japanese, Korean, Chinese, Russian, German, French, Spanish, Italian, Polish and Czech, read from Data\Interface\Translations\RaceMenuAtelier_<language>.txt for the game's sLanguage (a missing key falls back to English). The missing-letter notice's text moved from the DLL into the same files. The names of mods, tools and files (RaceMenu, BodySlide, CBBE 3BA, HIMBO, showracemenu, folder paths) stay as they are. The log and DevBench keep English text; atelier.control op=state adds status_shown (the line as drawn) and text_language.
- DevBench atelier.control: op=glyphs (framework, font, characters seen and missing, their glyph sets, the notice, one probe per script), op=set_text field=name|preset|search|bodyslide text=<UTF-8> and op=typed (put text in a field as if typed, read it back), op=presets (the list as shown, with each file's loadability), op=save_preset name=<UTF-8>.
- A notice above the categories when the menu font cannot draw letters the editor shows. The editor draws in the menu framework's font: SKSE Menu Framework 3 ships with its Cyrillic, Polish, Chinese, Japanese and Korean letters switched off (Character Glyphs / EnableCyrillic etc. in SKSEMenuFramework.ini), and Dear ImGui draws a question mark for every letter its font lacks. Twice a second the editor checks what it shows against the font; a letter still missing after two seconds shows which glyph set it belongs to and where to switch it on (Options > Open Settings > Character Glyphs, or the INI key and a restart). Apocrypha Menu Framework adds missing letters by itself, so there the notice only appears if a letter stays missing. The notice is in all eleven languages and falls back to English when the font cannot draw the player's own language. Each missing glyph set is written to RaceMenuAtelier.log once.

### Fixed
- Text in another alphabet no longer turns into question marks where the editor itself caused it (letoposlednee on Nexus, 2026-10-27: Cyrillic letters showed as ?????). Preset file names were read through the Windows ANSI code page, so a letter it lacks became a question mark and the rest reached the menu as bytes it cannot read; each preset is now listed by its real (UTF-8) name, a name RaceMenu saved from typed text shows as it was typed, and a file whose name RaceMenu cannot open on this Windows is greyed out with its Load button off instead of failing. The preset inspector now reads such files too. Names the game hands over in its own code page (the player and race name, anything read from RaceMenu) are turned into UTF-8 before they are drawn, and a typed name is never cut in the middle of a letter.

## 1.0.1 - 2026-10-13 - working (2026-10-24 change below untested)

2026-10-24 (the owner, 2026-10-23: "after pressing X to press Done, it should autofocus to the Finish button so that I
can press A and it will press Finish"): **the "Finish character creation?" confirmation opened by controller X puts the
controller focus on Finish**, so A finishes with no D-pad move. B is still Cancel; keyboard R, Enter and the mouse are
unchanged (the focus is set only when the confirmation came from the controller). DevBench atelier.control: op=pad_done
opens the confirmation the way X does; op=state reports done_confirm_open and done_finish_focused.

2026-10-27 (the owner, after trying it: A did not press Finish): **controller A confirms the Finish, Reset and Load
preset confirmations directly**, not only through the menu framework's navigation focus; A on Cancel (reached with the
D-pad) still cancels. A counts only as a fresh press after the confirmation has appeared, so the A that pressed Load
(or Reset) does not also confirm it in the same moment (the owner's round, 06:22: the Load confirmation never showed). Each confirmation now logs how it opened, whether the framework was in controller navigation,
every A it sees with where the focus was, and how it closed - so a press that never reaches the window shows as absent
from the log.

Asked for by the owner, 2026-10-13, playing with a controller: "I want the trait and class categories to be directly
after the race category"; "when you press X with controller, it will automatically press the done button, just like
the original race menu"; and, after trying the first build (L1/R1 window switching with a gold outline), "I want the
left stick to change focus from the left to the right pane, and I want the right stick to be character rotation, just
like the original race menu", with every item reachable by the D-pad and no white frame to enter and leave with A and B.
Then, trying that build: "Race menu is still generating a frame around certain areas ... I would rather just be able to
go and use the D-pad. Similarly to how AMF works, where it remembers where you last had your cursor: if I'm on race,
then I should D-pad right to enter the races without having to press any other button. And then if I press D-pad left
to get out of there, then it goes back to race, and then I can D-pad down to go to trait"; on a slider "I don't want to
leave the menu once I get to the end of it", with a guard so a held D-pad left goes only to the far left before leaving;
and "I want right stick click to toggle zoom in race menu."

### Changed
- README and README.txt describe the public release on Nexus Mods (approved by Nexus Mods staff on 2026-10-27) and link the source, in place of the old private, not posted note.
- Category order: the Class and Trait categories (any category holding another mod's choices) now come directly after
  Race in the category rail, and the controller triggers step through them in that order. The rest keep RaceMenu's
  order, and each category keeps its colour.
- Controller navigation in two columns, D-pad only, as in AMF's own menu: the category list, and beside it the chosen
  category's content (the race list, the sliders, the options). Up and down in the category list choose the category
  and its content shows at once. Right enters the content, on the item last highlighted there (each category remembers
  its own, and its scroll), or on a first visit on the current race or choice. In the content up and down move between
  items; left moves left one item at a time and, pressed again once nothing is further left, returns to that same
  category - a held left repeats only as far as the far left and does not leave, and the press that reaches the far left
  does not leave either. B returns to the category list from anywhere in the content. Nothing is entered with A or left
  with B any more, no area is ever framed, and the highlighted item is marked with a frame of its own (Atelier's items
  drew no highlight before, so only the areas showed one).
- D-pad left/right on a slider steps it (by its interval, repeating while held), and on an option row (hair, eyes and
  other parts) moves to the previous or next option; on these rows left and right never leave for the category list,
  even at the end of the range - move up or down to another row first. A on a slider no longer jumps its value to the
  mouse pointer. The step arrows and the typed value stay for the mouse and take no D-pad stop.
- The lists inside the scene and presets panel and inside the texture and head-part pickers are reached and left with
  the D-pad directly, with no frame round them; B closes a picker, and B on a confirmation (Finish, Reset, Load preset)
  is Cancel.

### Added
- Controller X (PlayStation Square) presses Done, as R does on the keyboard and as in RaceMenu: with "Ask before
  finishing the character" on (the default) it opens the same confirmation, otherwise it finishes at once. Switchable
  in Options ("Controller X finishes the character", `bGamepadDone` in RaceMenuAtelier.ini, on by default).
- Left stick left/right switches between the two windows - left the editor (back into the column it was left in),
  right the scene and presets panel (or its tab when the panel is hidden) - once per push, not repeating while held. Up
  and down on the left stick do nothing. In RaceMenu's own interface (F4) the left stick is the game's as before.
- Right stick click (R3) toggles the camera between the whole body and the close-up on the face, as Z does (RaceMenu's
  own ZoomPC); the right stick still turns the character.

### Fixed
- Controller: the white frame round an area, and A to enter it and B to leave it, were still there in the first rebuild.
  The menu framework runs Dear ImGui 1.90.8 (the version the SKSE Menu Framework 3 header is generated from), but that
  build took it for 1.92: it switched its child flattening off below 1.91 (and the 1.92 flag it would have used does not
  exist in 1.90.8), so the category list and the content stayed ImGui child windows that are one framed item of the
  editor each. Now each column keeps its navigation to itself, is never an item of the editor, and the editor moves the
  focus between the two itself; the D-pad step on sliders and the no-stop step arrows, also switched off by that test,
  now work.
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
