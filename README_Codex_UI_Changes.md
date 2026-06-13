# Codex UI and Asset Integration Notes

This note summarizes the UI, asset, and map work done on the `codex/ui-assets`
branch.

The latest battle-result, animation, audio, transition, pause-menu, and
performance work is documented in
[`README_UI_POLISH_TRIAL.md`](README_UI_POLISH_TRIAL.md).

## What Changed

- Added Qt resource packaging through `resources/resources.qrc`.
- Added local helper scripts:
  - `build_game.ps1`
  - `run_game.ps1`
- Added imported UI images under `assets/ui/`.
- Added imported character images under `assets/characters/`.
- Generated transparent cutout versions for the tomato monster images.
- Added `scene_lab_06` as a playable PVE map:
  - `assets/maps/lab_map_02.png`
  - `assets/maps/lab_map_02.json`
- Added PVE map selection support in the lobby:
  - `实验室通道 (01)` loads `lab_map_01`
  - `海滩果汁湾 (06)` loads `lab_map_02`
- Updated BattlePage to load the selected PVE map config instead of always using
  `lab_map_01`.
- Restored the start flow requested by the UI pass:
  - start with `scene_lab_03` full-screen splash
  - press any key or click to enter the older left-side five-button menu
  - keep wood-style menu buttons
- Reworked visible PVE/PVP lobby text that had become mojibake.
- Removed the white monster image background in battle by using cutout sprites.
- Unified several UI pages toward the imported warm illustrated asset style:
  - Start page
  - Settings page
  - Lobby page
  - Deck page
  - Deploy page
  - Battle HUD elements

## Imported Image Mapping

Original image files from `C:\Users\dfaf1234\Desktop\image` were copied into the
game project and renamed for stable code references:

| Project path | Purpose |
| --- | --- |
| `assets/ui/scene_lab_01.png` | UI/background reference image |
| `assets/ui/scene_lab_02.png` | UI style reference image |
| `assets/ui/scene_lab_03.png` | Initial splash screen |
| `assets/ui/scene_lab_04.png` | Settings/background image |
| `assets/ui/scene_lab_05.png` | Lobby/background image |
| `assets/ui/scene_lab_06.png` | Source image for the second map |
| `assets/ui/card_frame_tall.png` | Card frame UI asset |
| `assets/ui/panel_scroll.png` | Panel UI asset |
| `assets/ui/sign_lobby.png` | Lobby sign UI asset |
| `assets/ui/sign_battle.png` | Battle sign UI asset |
| `assets/ui/sign_banner.png` | Wood button/banner UI asset |
| `assets/characters/tomato_gunner.png` | Original monster image |
| `assets/characters/tomato_variant_01.png` | Original monster image |
| `assets/characters/tomato_variant_02.png` | Original monster image |
| `assets/characters/tomato_gunner_cutout.png` | Transparent battle sprite |
| `assets/characters/tomato_variant_01_cutout.png` | Transparent battle sprite |
| `assets/characters/tomato_variant_02_cutout.png` | Transparent battle sprite |
| `assets/maps/lab_map_02.png` | Playable map image copied from `scene_lab_06` |
| `assets/maps/lab_map_02.json` | Grid/path/deployment config for map 02 |
| `assets/ui/artwork/main_menu.jpg` | New full-art main menu |
| `assets/ui/artwork/pve_setup.jpg` | New PVE map and difficulty screen |
| `assets/ui/artwork/pvp_setup.png` | New PVP room and map screen |
| `assets/ui/artwork/deck_atlas.png` | New interactive deck and atlas screen |

The new full-page artwork is used as the visual base. Interactive regions are
cropped from the same source image at runtime by `ArtHotspot`, so hover, press,
selection glow, and responsive scaling stay aligned with the original art.

## Map Notes

`lab_map_01.png` and `lab_map_01.json` already existed in the repository before
this work.

`lab_map_02.json` was added as a first playable pass for the beach map. It
defines:

- 17 rows by 28 columns
- one monster route from the left cave toward the right-side juice base
- spawn and core points
- path cells
- deployable cells
- blocked cells
- a few high-ground cells

These route and placement cells are gameplay configuration, not raw UI art. They
can be adjusted later based on design requirements.

## Font and Mojibake Notes

Some older source files already contained garbled Chinese/emoji strings. This is
an encoding problem in the source text, not just a missing font problem.

The visible PVE/PVP lobby strings were cleaned up. There are still garbled
comments in some files, but comments do not show in the game UI.

For macOS, Qt should fall back to system fonts such as PingFang SC when a Windows
font like Microsoft YaHei is unavailable. The best long-term improvement is to
use a shared font helper or avoid hard-coding Windows-only font names.

## Build and Run

From the project root:

```powershell
.\build_game.ps1
.\run_game.ps1
```

The current local build has also been verified with:

```powershell
D:\Qt\Tools\CMake_64\bin\cmake.exe --build C:\Users\dfaf1234\Desktop\game\build-codex -v
```

## Remaining Art Needs

To make the UI fully match the provided reference style, the project would still
benefit from separated UI art assets:

- button normal/hover/pressed/disabled states
- panel backgrounds for settings, lobby, deck, deploy, and battle HUD
- card frames and selection states
- health/resource/wave HUD frames
- transparent animated sprites for monsters and player units
- exact map route/deploy overlays if the art team wants pixel-perfect gameplay
  placement
- a full playable `Office Panic` map image and JSON configuration; the current
  artwork only contains its selection-card preview, so it temporarily falls
  back to the Jungle Ruins gameplay map
