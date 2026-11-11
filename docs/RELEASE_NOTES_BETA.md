# Resistance: Fall of Man PC port: community beta release notes

**Version:** 0.2.0-beta (GitHub pre-release). **Release date:** _to be filled in on release_.

A native PC port of *Resistance: Fall of Man* (PS3) built by static recompilation: the game's PowerPC and SPU code is translated to C/C++ and compiled for Windows. Nothing is emulated. It renders through the port's own **Direct3D 12** renderer.

> **No game files and no prebuilt game are included.** This release is the source code plus Setup. You need your own legally dumped copy of the game (see *Install*); Setup builds the game on your PC from that copy.

This is a **beta**. The campaign has been played up to Cheddar Gorge. Later levels haven't been tested. Please report what you find (see *Reporting problems*).

## What's in this beta

### Launcher
- `Launcher.exe` holds all settings and key bindings. On first start it detects your hardware and picks a preset (Steam Deck included).
- The update check only opens the Releases page. Turn it off in PC Options or with `PORT_NO_UPDATE_CHECK=1`.

### Graphics
- **Anti-aliasing / upscaling:** Off, AMD FSR 3.1, NVIDIA DLSS / DLAA (GeForce RTX) or **Intel XeSS 2** (any DirectX 12 GPU with SM 6.4). Set it in the launcher or with `RSX_FSR` = 0 / 1 / 2 / 3.
  - If DLSS or XeSS can't start, the port uses FSR 3.1.
- **Render resolution** up to 4K (`RSX_RES_SCALE`). **Upscaling** renders the 3D scene at 67, 58, 50 or 33 % (`RSX_UPSCALE`); menus and HUD stay at full resolution.
  - A *1080p → DLSS 4K* preset is available for RTX 40 / 50 cards.
- **Frame rate targets** from 30 to 165, *Just under refresh* (`RFOM_FPS=vrr`, for G-Sync / FreeSync), *Original* pacing, or unlimited (`RFOM_FPS`).
  - Game speed is correct at every frame rate.
  - Frame-timed scripted sequences step at 30 fps automatically.
- **VRR / tearing control:** *Tearing: Allowed* presents immediately with VSync off, so G-Sync / FreeSync follows the frame rate. *Never* drops late frames instead of tearing (`RFOM_PRESENT=tearing|notear`).
- **NVIDIA Reflex** Low Latency through NVAPI (`RFOM_REFLEX`, off by default).
- **Ultrawide (Hor+)**, a field-of-view slider, and a performance overlay (F3).
- **Shader precompile:** with *Compile shaders before playing* (default on, `RSX_PSO_PRECOMPILE`), pipelines are built before the first frame. Shader caches are built on your PC, never shipped. They are backed up to `%LOCALAPPDATA%\ResistancePort\shadercache`.

### Controls
- **Mouse and keyboard:** raw mouse look (it doesn't emulate the stick), rebindable keys, and separate melee and interact keys.
- **Controllers through SDL3** (the default; `PAD_BACKEND=xinput` is still available): Xbox, PlayStation and others, swappable at any time.
- **Button prompts follow your device:** your keyboard keys, or Xbox buttons that match the real mapping. *Controller button prompts* (`RFOM_PAD_GLYPHS`) forces Xbox or PlayStation.

### PC features
- **PC menu:** F10, or hold Esc / Start. It has Resume, PC Options and **Quit to Desktop** with confirmation.
- **PC wording:** console references in the game's text use PC terms (`RFOM_PC_TEXT=0` turns this off).
- **Screenshots** (F12), a **mods folder**, local **trophies**, and saves in *Saved Games*.
- A black cover window hides the switch between programs during level loads (`RFoMSwitch.exe`).
- **Crash and hang reports** are written automatically, and the game process can restart after a freeze.

### Audio
- **Time stretching** (SoundTouch, `PORT_AUDIO_STRETCH=1`, default on): frame drops make the audio slightly slower instead of crackling.

## What you download

- **`ResistancePort-Setup-0.2.0-beta.exe`** from the GitHub release: an installer with the port's source, scripts and `Launcher.exe`. It installs the files and starts Setup.
- or the **source code** (the release's source archive or `git clone` at the tag), then `Setup.bat`.

Either way, the game itself is built on your PC; no built game program is published. Setup downloads the build tools, FFmpeg (an LGPL build) and, if you accept their licences, the NVIDIA DLSS and Intel XeSS SDKs.
## Requirements

- Windows 10 or 11, 64-bit. Steam Deck / Proton works; see the README.
- A CPU with **AVX2** (Intel 4th generation or newer, or any AMD Ryzen).
- A **DirectX 12** GPU. It has been tested on AMD RX 6700 XT and NVIDIA RTX 5070. DLSS needs a GeForce RTX card.
- The Microsoft Visual C++ 2015–2022 x64 runtime, version 14.40 or newer.
- Your own copy of **Resistance: Fall of Man, US disc (BCUS98107), version 1.00**. Other regions and patched versions are not supported yet.
- About **25 GB** of free disk space: about 16.5 GB for the game files plus the build.
- For setup: an internet connection and the free build tools. Setup installs the tools itself.

## Install

1. **Dump your own game.** Use a decrypted ISO, or a game folder containing `PS3_GAME` (the same kind of folder RPCS3 uses), from your own disc.
2. **Install the port.** Either:
   - run `ResistancePort-Setup-<version>.exe`, which installs to `C:\Games\ResistancePort` by default and asks for your game; or
   - download the source code (the release's source archive, or `git clone` at the release tag) into a folder without spaces and double-click `Setup.bat`.
3. **Setup** finds or asks for your game (`Setup.bat -Game "<ISO or folder>"` skips the question). It then:
   - checks the copy and copies it to `gamefiles\PS3_GAME`;
   - decrypts the game's two program files to `game\`, using the public application keys from RPCS3's source;
   - downloads the DLSS SDK and FFmpeg;
   - generates and builds the game-specific code. This takes 20–60 minutes the first time.
4. **Play** with `Launcher.exe`.

**How the port finds your game:**
- `run.bat`, which the launcher starts, runs `build\ResistanceFoM.exe` with `game\EBOOT.elf` and points the game's virtual disc at `gamefiles` (`PS3_VFS_ROOT`).
- Settings and controls are also kept in `%USERPROFILE%\Saved Games\Resistance Fall of Man\config`. An empty `portable.txt` turns that off.

**First launch:** the first start compiles shaders and takes longer. Later starts are fast.

## Known issues

- **Performance can dip in heavy areas.** A faster CPU helps the most.
- **Frame Generation** (DLSS on RTX 40/50, FSR 3 elsewhere) is new and off by default.
- **Audio crackle** can happen when the frame rate drops in intensive spots, for example coming out of the crater at the start of York.
- **Falling through the floor** has been seen a few times. A safeguard tries to put you back in place.
- **Shadows can flicker** occasionally.
- **FSR can ghost** on some things, such as the sky, at rare moments. DLSS doesn't show this.
- **Out-of-bounds water** (off the dock in Grimsby, under the bridge in Manchester) shows some stray triangles.
- **The Rollers level** hasn't been reached on this port yet. On RPCS3 it needed OpenGL.
- Everything else, including levels after Cheddar Gorge: **TBD after testing**.

## Reporting problems

Please read **[REPORTING.md](REPORTING.md)**. Include how to reproduce the problem, the session's log from `logs\` and any crash or hang report.

## Licences

The port is MIT-licensed (see `LICENSE`). This port uses NVIDIA DLSS. Third-party components are listed in `THIRD_PARTY_NOTICES.md`. *Resistance: Fall of Man* is a trademark of its owners. This project is not affiliated with or endorsed by them.
