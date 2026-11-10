# VBoy Color on the Steam Frame

The Steam Frame runs SteamOS on ARM64. This fork builds `VBoyColorVR` as a native
Linux ARM64 app using OpenXR and Vulkan, without a streaming PC, x86 translation
or Lepton. Beetle VB still emulates the Virtual Boy games. The app shares its
renderer, menus and emulator with the Quest and PC versions.

## What This Branch Adds

- A projection-layer screen on SteamVR, keeping left and right game images separate
  while preserving screen size, distance, IPD offset and Follow Head.
- The Valve Frame controller profile, including right Menu, left View, D-pad and
  bumpers. Pointer selection and remapping work through the existing menu.
- Linux ARM64 packages containing both `VBoyColorVR` and the flat `VBoyColor` app.
- `install-to-steam.py`, which adds the VR app to Steam with its name, icon and art.

The detailed source paths are in [How It Works](#how-it-works) below.

## Installing

1. Build this branch using [the steps below](#building-it-yourself). Copy the whole
   `build-frame/VBoyColorVR-linux-arm64/` folder to the Frame, for example as
   `~/Games/VBoyColor`. If you're using an upstream release instead, its archive is
   `VBoyColor-<version>-linux-arm64.tar.gz` from
   [yuk27/VBoyColor](https://github.com/yuk27/VBoyColor/releases).
2. Put your `.vb` ROMs in its `roms` folder. Settings, saves and save states
   go next to them, same as the desktop apps (Settings > Folders moves them).
3. Run `./install-to-steam.py` in that folder. It adds VBoy Color to Steam
   as a non-Steam game, as a VR app, with the Quest's icon and library
   artwork made from the logo (`steam/`, made by `tools/make_icon.py`), so
   it looks like any other game in the library. Steam only reads its list of
   non-Steam games when it starts, so the script closes it for a moment
   (on SteamOS by stopping and starting Steam's service). Running it again
   updates the entry; `--uninstall` takes it out again.
4. Start it from the library, not from a terminal: SteamVR only hands the
   controllers to apps it launched.

## Controls

The Frame's controllers are a gamepad split in two: the D-pad and View on
the left, A, B, X, Y and Menu on the right. The buttons do what the same
buttons do on a Quest, and the extra ones work like a gamepad's.

| Virtual Boy | Steam Frame |
|---|---|
| Left D-pad | left stick or the D-pad |
| Right D-pad | right stick |
| A / B | A / B |
| L / R | left / right trigger, or the bumpers |
| Start | Y |
| Select | X, or View |
| VBoy Color's menu | Menu (or hold the left stick click for a second) |

Everything can be remapped in Settings > Button mapping, as anywhere else.
The D-pad, the bumpers and View show up there as the gamepad's buttons.

## The refresh rate

SteamVR picks the refresh rate, not the app: set it for VBoy Color in
SteamVR's per-app settings in the headset. The Virtual Boy draws 50.27
frames a second, so a rate that's a multiple of it (100 Hz) scrolls most
evenly, and 120 Hz is the next best.

## How it works

Two changes make the Frame work. Both probably help PC VR on SteamVR too, but
I've only tried them on the Frame.

The first is the 3D. The screen used to be two quad layers, one per eye
(`eyeVisibility` LEFT and RIGHT), but SteamVR shows such a layer to both
eyes, so on the Frame both eyes got the same picture and the 3D was gone.
On SteamVR the screen now goes out as a projection layer instead, which is
per eye by definition: each eye's view looks from that eye straight at its
quad, with the field of view the quad's corners make from there
(`OpenXrApp::RenderScreenProjection`). The compositor draws each picture
exactly where the quad was, so placement, size, Distance, the IPD offset and
Follow head all work as before. Other runtimes keep the quads.

The second is the controllers. With `XR_VALVE_frame_controller_interaction`
the Frame's controllers get their own bindings (`XrInput::Initialize`).
Touch's bindings are still suggested for every other runtime and controller.

The Linux builds now include `VBoyColorVR` (it already built there, it just
wasn't packaged), and CI builds Linux on ARM64 too.

## Building It Yourself

### Docker (Frame Package)

Install Git and Docker, start the Docker engine, then use a POSIX shell:

```sh
git clone --recurse-submodules -b steam-frame https://github.com/astromaddie/VBoyColor.git
cd VBoyColor
DOCKER_DEFAULT_PLATFORM=linux/arm64 ./tools/steam_frame/build-linux-arm64.sh
```

Apple Silicon macOS and ARM64 Linux can run this container natively. On Intel/AMD
Linux, Intel macOS or Windows/WSL2, Docker also needs working ARM64 emulation. The
script doesn't configure that. Use WSL's shell on Windows. The explicit
`DOCKER_DEFAULT_PLATFORM` matters on x86 hosts: the Dockerfile downloads an ARM64
CMake binary and the helper doesn't pass `--platform` itself. This builds a Linux
app, not a native macOS or Windows app.

The first run needs network access to Ubuntu's package archives and GitHub.
`linux-arm64.Dockerfile` uses Ubuntu 22.04, CMake 3.31.6, GCC, Ninja and the X11/Vulkan
runtime dependencies. CMake fetches the Vulkan/OpenXR headers and libraries,
glslang, FreeType, GLFW and other dependencies. You don't need a separate Vulkan
SDK or ROMs to compile.

The helper builds `VBoyColorVR` and `VBoyColor` with three jobs, then calls
`tools/steam_frame/package.sh`. Build files stay in `build-frame/`; downloaded
CMake dependencies are cached in the Docker volume `vboycolor-frame-deps`.
The image is named `vboycolor-frame-build`. It doesn't install or launch anything
on the headset.

The ready-to-copy folder is:

```text
build-frame/VBoyColorVR-linux-arm64/
```

It includes both executables, runtime fonts/icons/logo/color packs, an empty ROM
folder, Steam artwork, the installer and licences. The helper produces a folder,
not a tarball. To make an archive:

```sh
tar -czf build-frame/VBoyColorVR-linux-arm64.tar.gz \
  -C build-frame VBoyColorVR-linux-arm64
```

Keep your ROMs and saves outside this generated folder on the build host:
`package.sh` replaces it on every run. On the Frame, keep the existing `roms/`
and user data when updating the app. Set alternate locations in Settings > Folders
if you want data separate from the installation.

### Native ARM64 Linux

The Linux dependency and CMake instructions in [BUILDING.md](BUILDING.md#linux)
apply. Build both targets:

```sh
cmake -S . -B build-pc -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_WAYLAND=OFF
cmake --build build-pc --target VBoyColor VBoyColorVR -j2
```

This builds for the host architecture. On x86-64 it produces x86-64 executables,
not Frame binaries. Use the Docker route above for the packaged ARM64 output.
Ubuntu 22.04 is the package's compatibility baseline; a build on a newer distro
can require newer glibc/libstdc++ symbols than the headset provides.

### Checks and Common Problems

- Confirm `git submodule status` has no uninitialized (`-`) entries. Use
  `git submodule update --init --recursive` to populate the emulator core.
- `file build-frame/VBoyColorVR` should identify an AArch64 ELF executable.
  A Mach-O or x86-64 binary is the wrong target for the Frame.
- An `exec format error` inside Docker usually means the container isn't ARM64
  or the host's ARM64 emulation isn't available. Check the Docker platform first.
- A missing newer GLIBC/GLIBCXX symbol on the headset means the binary was built
  against a newer runtime. Rebuild with the Jammy container rather than replacing
  system libraries on the Frame.
- No controller input from a terminal launch: launch through the Steam shortcut.
- The app doesn't choose the Frame refresh rate. Use SteamVR's per-app setting;
  100 Hz is close to twice the Virtual Boy's 50.27 Hz rate.

The Docker helper compiles and packages; it doesn't run a headset test or a full
regression suite. CI also compiles both Linux targets on x86-64 and ARM64, but
that alone doesn't verify controller input or stereo presentation. The emulator
and colour-pack harness is described in [BUILDING.md](BUILDING.md#test-harness).
