<h1 align="center"><img src="assets/logo/vboycolor-logo.png" alt="VBoy Color" width="380"></h1>

<p align="center"><b>Virtual Boy games in color - in 3D on Meta Quest, on PC, and on Android phones and handhelds.</b></p>

VBoy Color is a free, open-source Virtual Boy emulator. On a Quest headset
it shows games the way the Virtual Boy was meant to be seen, in stereoscopic
3D, but in color instead of red on black. On Windows and Linux it plays them
in a window, no headset needed, and on Android phones, tablets and handhelds
with touch controls or a gamepad.

<p align="center"><a href="https://youtu.be/vsScgEVnbME"><img src="images/demo-video.jpg" alt="VBoy Color demo video: eight Virtual Boy games, the original red next to VBoy Color" width="720"></a><br>
<a href="https://youtu.be/vsScgEVnbME"><b>▶ Watch the demo</b></a> - eight games, the original red next to VBoy Color (2 min)</p>

> VBoy Color is an independent fan project. It is not affiliated with,
> endorsed by, or sponsored by Nintendo or Meta. "Virtual Boy" is a trademark
> of Nintendo. No games are included - use only ROMs you made from cartridges
> you own.

*Left: the original. Right: VBoy Color (Auto mode with the game's color
pack).*

![Galactic Pinball](images/galactic-pinball.png)
<sub>**Galactic Pinball** - the Colony table: bumpers, rails and targets each in their own color.</sub>

![Mario Clash](images/mario-clash.png)
<sub>**Mario Clash** - green pipes, grey ledges and the brick wall behind them.</sub>

![Wario Land](images/wario-land.png)
<sub>**Wario Land** - the cave in warm orange, the blocks in teal.</sub>

![Teleroboxer](images/teleroboxer.png)
<sub>**Teleroboxer** - the robot in steel, gold and red.</sub>

![Jack Bros.](images/jack-bros.png)
<sub>**Jack Bros.** - Jack Frost in blue and white, his face touched up by hand to match the Virtual Boy's drawing.</sub>

![Mario's Tennis](images/marios-tennis.png)
<sub>**Mario's Tennis** - a clay court under a green skyline.</sub>

![Teleroboxer for colored 3D glasses](images/3d-glasses.png)
<sub>**3D with colored glasses** (Teleroboxer, PC and phones) - red / cyan on the left, amber / blue on the right; green / magenta too.</sub>

![Vertical Force side by side](images/3d-side-by-side.png)
<sub>**3D side by side** (Vertical Force, PC and phones) - the left eye's picture next to the right eye's, for a 3D TV or parallel viewing; also squeezed for a 3D TV, cross-eyed, and top and bottom.</sub>

## Features

- **Color, four ways** (Settings → Color mode):
  - **Auto** (default) - every pixel is colored by what drew it: background
    layers by their depth (far to near: dusk violet, brick, amber, sand),
    characters by their palette, each with dark/light/lightest steps. What a
    game draws without tiles (Red Alarm's wireframe world, 3-D Tetris's
    well) is colored by how far away it is - the distance between the two
    eyes' pictures - from indigo far away to gold up close. Every game shows
    many colors with no setup.
  - **Multicolor** - each of the Virtual Boy's four shades gets its own color
    (in the spirit of Red Viper's multicolour mode), from a set of palettes.
  - **Gradient** - the game's brightness mapped onto a color gradient
    (Jade, Ocean, Sunset, Ember, Frost, Toxic).
  - **Tint** - the classic look: red, or any color you mix.
- **Color packs** - hand-painted colors for a game, tile by tile, shown in
  Auto and Multicolor modes. Packs for Bound High, Dragon Hopper, Galactic Pinball,
  Innsmouth no Yakata, Jack Bros., Mario Clash, Mario's Tennis, Panic Bomber, Space
  Invaders Virtual Collection, Teleroboxer, Vertical Force, V-Tetris and Wario
  Land are built in, plus the menus and HUD of Red Alarm and 3-D Tetris
  (their 3-D worlds are drawn without tiles, so Auto colors them by depth),
  Virtual Bowling's HUD, gauges and menus over its Sunset lanes, and Golf's
  menus and hole maps (its rounds are drawn without tiles too). Packs hold
  only colors, no game graphics. A `.vbcp` file next to a ROM overrides the
  built-in one, and you can paint your own on PC: see
  [docs/COLOR_PACKS.md](docs/COLOR_PACKS.md).
- **Colors per game** - every Virtual Boy game starts in the scheme that
  suits it best (picked game by game: Red Alarm's wireframes by depth,
  Golf's fairways green, Virtual Bowling's lanes in Sunset wood...),
  and remembers yours once you change it.
- **A library of your games** - each one a card with its box art,
  downloaded from libretro's thumbnail collection (keep playing while it
  comes). A game without box art, or with no internet, gets its title
  screen in its own colors instead, made on your device (nothing of any
  game ships with the app); Settings → Download box art off shows title
  screens for all. As a list (Y) or last played first (X), and all games
  or only those with a color pack (the chip at the top, or Settings → Only
  games with color packs). Pick with the
  sticks, a gamepad, the mouse, or by pointing a Quest controller and
  pulling its trigger.
- **Real 3D on Quest** - each eye gets its own picture, as on the hardware,
  and both eyes always show the same colors. The display runs at the
  headset's highest refresh rate, so scrolling stays smooth.
- **Save states** - several slots per game, with thumbnails in color.
- **RetroAchievements** - log in with your
  [RetroAchievements](https://retroachievements.org) account (Achievements in
  the menu: an on-screen keyboard types your name and password - or a real
  keyboard on PC) and earn each game's achievements as you play. An unlock
  pops up over the game; the Achievements page lists the game's
  achievements with their badges, unlocked and still to get. Softcore: save
  states and everything else stay on (a save state keeps the achievements'
  progress too). Only the login token the site hands back is kept, in the
  settings folder - never your password. A game needs the ROM version
  RetroAchievements lists (the usual No-Intro dump).
- **Gamepads everywhere** - in the PC window, in PC VR and on Android, with
  the usual emulator layout (the Virtual Boy's A and B where they are on its
  controller: right and bottom); every pad connected works, and pads plugged
  in later are picked up.
- **Button mapping** - Quest controllers, gamepads and keyboard, all
  remappable - for all games, or per game (hold a game's card in the
  library).
- **Screen looks** - Settings → Screen → Look: *Sharp* pixels, *Smooth*
  (OmniScale's rounded edges and diagonals) or *LED* (the Virtual Boy's
  own rows of light, with a soft glow). Both eyes always get the same
  look, so the 3D stays clean.
- **Screen placement** - size and distance of the screen in VR; on PC,
  Settings → Screen → Size: as big as the window allows (*Fit*) or *Whole
  pixels*.
- **3D on a normal screen** (PC and phones) - Settings → Screen → 3D, then
  its format: *side by side* (3D TV - squeezed, as a TV's Side-by-Side mode
  expects -, full, or cross-eyed), *top and bottom* (3D TV or full), or
  *colored glasses* - *red / cyan*, *green / magenta* or *amber / blue*
  (mixed with Dubois' method, which keeps the games' colors as true as the
  glasses allow). Pictures of both at the top of this page.
- **Your room as the background** (Quest) - Settings → Adjust screen →
  Show your room around it: the game floats in your room (passthrough)
  instead of in the dark.
- **Updates** - VBoy Color looks for a new version when it starts (About →
  Check when VBoy Color starts) and shows *Update available* in the
  sidebar; About → Update installs it. On PC the app replaces its own files
  (your games and saves stay) and restarts; on Android the system's
  installer takes the new APK (allow VBoy Color to install apps once; on the
  Quest it installs when the app next starts). A beta looks for betas too,
  a full release only for full releases.
- **Your own folders** - Settings → Folders: choose where your games,
  saved games, save states and settings go (a synced folder, say). On PC
  you pick them right in the menu, with the mouse, a gamepad or in VR; on
  Android with the system's folder picker (on the Quest, when the app next
  starts). The files already there are copied over - newer ones win, the
  one replaced is kept as `.bak`, and the originals stay where they were.
- **Video recording** (PC) - F12 records the original red and the colored
  version side by side, frame for frame, for comparison videos (F12 again
  to stop; it stops on its own after ten minutes).
- **Tool-assisted runs** (PC) - drop a Virtual Boy run from
  [TASVideos](https://tasvideos.org/Movies-VBoy) (a `.bk2` file) on the
  window, or open it with `VBoyColor.exe`: its game (from your `roms`
  folder) plays the whole run by itself, in color - press F12 to record it.
  Your own save stays untouched; when the run ends, you take over.

## Getting started

### Quest (Quest 2, 3, 3S, Pro)

1. Download `VBoyColor-<version>-quest-android-arm64.apk` from
   [Releases](https://github.com/yuk27/VBoyColor/releases).
2. Install it with [SideQuest](https://sidequestvr.com) or
   `adb install VBoyColor-<version>-quest-android-arm64.apk` (the headset must be in developer
   mode).
3. Copy your `.vb` ROMs to a folder on the headset (for example
   `Download/VBoyColor`). The color packs are built in.
4. Open VBoy Color from **Unknown Sources** in the app library and pick that
   folder when it asks.

### Android phones and tablets

The same `VBoyColor-<version>-quest-android-arm64.apk` works on phones, tablets
and Android handhelds (Android 10 or newer, 64-bit ARM, with Vulkan - nearly
every device from the last several years).
Install it, open VBoy Color and pick the folder with your `.vb` ROMs when it
asks. The game fills the screen's height, with the Virtual Boy controller
beside it: the left D-pad with Select and Start (and the menu button under
them), B and A with the right D-pad above them (only in the games that use
it - Red Alarm, Teleroboxer, 3-D Tetris and Golf), L and R on top. Settings →
Touch controls → Edit lets you drag every control where you want it and
make it bigger or smaller (the buttons are Kenney's Input Prompts) - and a
game can have its own layout (hold its card in the library). Connect a
gamepad and the on-screen buttons step aside until you touch the screen
again. Back opens the menu, and Back (or *Back to game*) returns to the
game. With no game loaded, the menu sits inside a Virtual Boy.

### Windows

Download `VBoyColor-<version>-windows-x64.zip` from
[Releases](https://github.com/yuk27/VBoyColor/releases), unzip it, put your
ROMs in the `roms` folder and run `VBoyColor.exe`. You can also drop a `.vb`
file on the window, or open one with `VBoyColor.exe`. `VBoyColorVR.exe` is the
VR version for a PC headset (Quest Link, Virtual Desktop or SteamVR).

### Linux

Download `VBoyColor-<version>-linux-x64.tar.gz`, unpack it, put your ROMs in
`roms` and run `./VBoyColor` from that folder. Needs a Vulkan driver.

## Controls

**Menu:** the left controller's Menu button, a gamepad's Select + Start
held together (or Guide), or **Tab** or **Esc** on PC. Some PC VR runtimes
keep the Menu button for themselves - there, hold the left stick click for a
second. In the menu: sticks / D-pad / arrow keys move, A (Enter) picks, B
(Esc) goes back - out to the sidebar, or back to the game. The mouse and the
Quest controllers' lasers work too: a quick trigger press clicks, holding the
trigger and moving drags (scrolls lists and the library).

| Virtual Boy | Quest controllers | Keyboard | Gamepad |
|---|---|---|---|
| Left D-pad | left stick | arrow keys | D-pad or left stick |
| Right D-pad | right stick | W A S D | right stick |
| A / B | A / B | X / Z | B / A - where the Virtual Boy has them (right and bottom face buttons) |
| L / R | left / right trigger | Q / E | LB / RB (or LT / RT) |
| Start / Select | Y / X | Enter / Backspace | Start / Back (View) |

The gamepad column is the PC window's and the phone's. With a gamepad in VR
(VBoyColorVR.exe, or a pad paired with the Quest), A is on A and B on B.
Everything can be remapped in Settings → Button mapping - and per game:
hold a game's card in the library (a finger or a Quest laser held still on
it; right-click with a mouse) for its own button mapping and, on a phone,
its own touch controls. On PC, **Alt+Enter** toggles fullscreen.

## Building

See [docs/BUILDING.md](docs/BUILDING.md) - Windows, Linux and Quest, all from
one CMake project. Every push is built by GitHub Actions.

## Support the project

[![Support VBoy Color](images/support-badge.svg)](https://github.com/sponsors/yuk27)

https://github.com/sponsors/yuk27

## License

VBoy Color is free software under the **GNU General Public License v3.0**
(see [LICENSE](LICENSE)). The emulator core, Beetle VB, is GPL-2.0-or-later.
Other components and fonts are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Thanks

- **[VirtualBoyGo](https://github.com/CidVonHighwind/VirtualBoyGo)** by
  CidVonHighwind - VBoy Color started as a fork of its OpenXR + Vulkan
  rework. The VR app, renderer, menu, save states and button mapping come
  from there. Thank you for building it and sharing it.
- **[Beetle VB](https://github.com/libretro/beetle-vb-libretro)** - libretro's
  fork of Mednafen's Virtual Boy emulation, the emulator core. It is compiled
  untouched, apart from a generated hook in its video code that reports which
  tile drew each pixel (`cmake/PatchBeetleVip.cmake`).
- **[Red Viper](https://github.com/skyfloogle/red-viper)** - the Virtual Boy
  emulator for the 3DS, for the idea of coloring the four shades (its
  multicolour mode). No Red Viper code is used.
- **[RetroAchievements](https://retroachievements.org)** and its
  [rcheevos](https://github.com/RetroAchievements/rcheevos) library (MIT) -
  the achievements, made by its community for every Virtual Boy game it covers.
- **[Super Bit Shader](https://github.com/AshleyPikachu/Super-Bit-Shader)** by
  Ashley the Pikachu - RetroArch shaders that color the Virtual Boy, the Game
  Boy and the other monochrome handhelds, with side-by-side 3D for the
  Virtual Boy in VR. Thank you for keeping the colored Virtual Boy scene
  going with your great work. Super Bit Shader implementation is WIP.
- The libraries and fonts listed in
  [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
