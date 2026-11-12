# Resistance: Fall of Man: native PC port

<a href="https://recomplauncher.com/game/resistance-fall-of-man-recomp"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

A static recompilation of the PS3 game *Resistance: Fall of Man* into a native Windows program. Nothing is emulated: the game's PowerPC and SPU code is translated to C/C++ and compiled for your PC. It renders through its own Direct3D 12 renderer, with NVIDIA DLSS / DLAA and AMD FSR 3.1.

> **No game files and no translated game code are distributed, in this repository or in the installer.** You need your own copy of the game. Setup decrypts its program on your PC, translates it into C/C++ and builds the game there; the generated code and the built game never leave your PC, and no prebuilt game program is released. The repository holds the port's own code plus the small references it needs to find its place in the game's program: function addresses, patch values and short context lines from the generated code (`tools/hooks/`).

## Highlights

- **60 fps and higher** (30 to 165, *Just under refresh* for G-Sync / FreeSync, or unlimited). Game speed is correct at every frame rate.
- **NVIDIA DLSS / DLAA, AMD FSR 3.1 and Intel XeSS**, NVIDIA Reflex, render resolutions up to 4K, and a *1080p → DLSS 4K* preset for RTX 40 / 50 cards.
- **Native PC controls:** raw mouse look (not stick emulation), rebindable keys, and separate melee and interact keys. Xbox and PlayStation controllers work and can be swapped at any time.
- **Button prompts follow your device:** your keyboard keys, or Xbox buttons when an Xbox controller is in use.
- **In-game PC menu** (F10, or hold Esc / Start): Resume, PC Options, and **Quit to Desktop** with a confirmation. No Alt+F4 needed.
- **Ultrawide (Hor+)**, field of view slider, performance overlay (F3), screenshots (F12), mods folder, and saves in *Saved Games*.
- **Launcher** that detects your hardware and picks recommended settings.
- **Performance work:** the renderer was profiled down to about 5 µs of CPU per draw (see *Performance*).

## What you need

- A PC running Windows 10 or 11 (64-bit).
- A CPU with AVX2 (Intel 4th generation or newer, or any AMD Ryzen).
- A DirectX 12 GPU. It has been tested on AMD (RX 6700 XT) and NVIDIA (RTX 5070) cards. DLSS needs a GeForce RTX card; every other GPU uses FSR 3.1 or Intel XeSS (XeSS runs on any DirectX 12 GPU with Shader Model 6.4).
- Your own dump of **Resistance: Fall of Man, US disc (BCUS98107), version 1.00**: the folder that contains `PS3_GAME`, the same kind of folder RPCS3 uses. Other regions and patched versions are not supported yet.
- About 25 GB of free disk space: 16 GB for the game files, the rest for the build tools and the build.
- An internet connection for setup.

## Install

**Quick install (recommended):**

1. Download the release zip from the Releases page and unzip all of it to a folder with no spaces in the path, for example `C:\Games\ResistancePort`.
2. Run **`Setup.exe`** in that folder. (It is not code-signed: if Windows says *"Windows protected your PC"*, click **More info → Run anyway**.)
3. Pick your game: **ISO...** or **Folder...**. Setup checks the copy at once and says what to do if it can't be used (see *If your game copy is not accepted* below).
4. Click **Install** and wait. Setup installs the missing build tools (one administrator prompt), copies your game files, builds the game with as many parallel jobs as your RAM allows, adds Start-menu and desktop shortcuts and starts the launcher. It shows the progress and the time left. If it stops (error, sleep, power cut), run `Setup.exe` again and click **Resume**.

The release contains no game code: the game is always built from your own copy. Requirements, disk space, build times, uninstalling and troubleshooting (SmartScreen, antivirus, low RAM, no winget): [docs/INSTALL.md](docs/INSTALL.md).

### Manual install (advanced)

**With the Inno installer** (if a `-Setup-<version>.exe` is on the Releases page): download `ResistancePort-Setup-<version>.exe` from the Releases page and run it. It installs the port (default `C:\Games\ResistancePort`), adds Start-menu and desktop shortcuts, and asks for your game. It lists the copies it finds on the PC (common folders and RPCS3's game list); you can also type or paste a path, or use the **ISO...** / **Folder...** buttons. It checks the copy before going on and says what is wrong and what to do if it can't be used (see *If your game copy is not accepted* below). At the end it runs `Setup.bat` for you (step 2 below). The installer contains only the port; the game files always come from your own copy. Use a decrypted ISO or a game folder (the same kind RPCS3 uses). Tick **Portable mode** to keep settings and saves only in the install folder.

**By hand:**

1. Download this repository (**Code → Download ZIP**) and unzip it to a path without spaces, for example `C:\Games\ResistancePort`.
2. Double-click **`Setup.bat`**. It:
   - installs the free build tools if they're missing (Visual Studio 2022 Build Tools, LLVM, CMake, Ninja, Python) using `winget`;
   - looks for your game (an ISO or a game folder), checks it, then copies it into the port folder (or moves a folder, if it's on the same drive);
   - decrypts the game's two program files using the application keys from RPCS3's public source code;
   - downloads the DLSS SDK and FFmpeg;
   - generates the game code from your copy and builds the game.

   The first run takes a while: installing the tools, plus a 20 to 60 minute build.
3. Play with **`Launcher.exe`**. On the first start it detects your hardware and picks recommended settings; everything is adjustable there.

Run Setup again only after downloading a newer version of the port.

**Choosing the game in Setup.bat:** it lists the copies it found, numbered. Type a number, or type or paste the path of the ISO or game folder (you can also drag it into the window). **B** opens a file picker (Windows only). `Setup.bat -Game "<path>"` skips the question. The path can be an ISO, the game folder, or a file inside it. An ISO is opened by Windows, copied from and closed again; if Windows can't open it, Setup reads it directly.

### If your game copy is not accepted

The installer and Setup check the copy before copying anything. The supported version is **Resistance: Fall of Man (US disc, BCUS98107)**, about 16.5 GB.

| Message | What to do |
|---|---|
| compressed archive (.zip, .7z, .rar) | Extract it first, then pick the ISO or folder inside. |
| PlayStation Store package (.pkg) / game update folder | Pick the disc copy. Game updates are not needed. |
| not &lt;game&gt; / from another region | Use the supported disc version listed above. |
| PlayStation Store version | Only the disc version is supported. |
| encrypted disc image / PARAM.SFO is not valid | Decrypt the ISO first (for example PS3Dec with the disc key from the `.dkey` file), or use a game folder from a PS3 or RPCS3. |
| program file was decrypted or replaced (plain ELF) | Use the original, unmodified files. Setup decrypts them itself. |
| files are missing / smaller than expected | The copy is incomplete. Copy or dump it again. |
| different from the known file (warning) | The copy may be modified. You can continue; Setup stops later if the decrypted program is not the supported version. |
| not enough free space | Free up space, or install on another drive. |

The checks run on your PC only. Nothing is uploaded.

**Updates:** the launcher checks this repository's Releases page once at start and says when a newer version is out (it only opens the download page; nothing is downloaded or replaced). Turn it off with Launcher → Advanced → *Check for updates* → Off, or the environment variable `PORT_NO_UPDATE_CHECK=1`. The installed version is in the `VERSION` file.

**Settings and controls** (`settings.ini`, `settings.bat`, `controls.txt`, `mouselook.cfg`) are also kept in `%USERPROFILE%\Saved Games\Resistance Fall of Man\config`. Missing files are restored from there when the launcher or game starts, so they survive reinstalling or updating the port. To keep everything in the port folder only, create an empty file named `portable.txt` next to `Launcher.exe` (the installer's *Portable mode* does this).

**No console window:** the launcher starts the game without a console. Everything the console used to show is written to `logs\run.log`.

**First launch:** the game shows its one-time "initial setup" screens. Shaders are compiled before the game starts, with a **Compiling shaders N / M** progress panel (see *Shader precompile*), so levels don't sit black while they compile. The shader cache is kept, including a backup that survives updates and reinstalls.

## Controls

Keyboard and mouse, Xbox controllers, PlayStation pads (DualShock 4, DualSense, DualShock 3 with its driver) and Switch Pro / Joy-Con all work, and you can switch between them at any time. Controllers are read through SDL3 (built in, no extra DLL), so **DS4Windows and Steam Input are no longer needed** (they still work; a virtual Xbox pad they add next to the real controller is ignored as a duplicate). If SDL3 can't start, the port falls back to XInput (`logs\run.log`: `[cellPad] controllers: SDL3` or `SDL3 could not start`).

- **Button prompts follow the last device you used.** On keyboard and mouse the game shows your key bindings. On an Xbox / XInput controller it shows Xbox buttons that match the real mapping: Cross = **A**, Circle = **B**, Square = **X**, Triangle = **Y**, L1/R1 = **LB/RB**, L2/R2 = **LT/RT**, L3/R3 = **LS/RS**, Select = **View**, Start = **Menu**. The Xbox glyphs are drawn by the port (coloured circles with letters); no Microsoft artwork is used. A PlayStation controller (by its SDL3 type, or with XInput by its USB / Bluetooth ID) keeps the PlayStation symbols. Switch controllers are read by button position (bottom = Cross) and show PlayStation symbols; with *Nintendo button layout = Nintendo style* A (right) confirms and B cancels, and the prompts show the Xbox-style letters. `RFOM_PAD_GLYPHS=auto|xbox|ps` overrides the choice (default `auto`; use it under Steam Input if the real controller type isn't reported).
- **Controller hot-plug:** controllers are polled every few milliseconds, so plugging one in or pulling it out works at any time without a restart. The log shows `[cellPad] controller N connected` / `disconnected`.
- **Mouse look is native.** Raw mouse movement (Windows raw input) turns the player's camera directly, 1:1 on foot; it is not converted into an analog stick. Scoped weapons scale it by their zoom.
- Melee and interact are split onto separate keys (on the PS3 they shared a button).
- Bindings are edited in the launcher (Controls page) or by hand in `controls.txt` (`KEY = ACTION`, one per line; the header of the file lists every key and action name).

Mouse and controller options in `controls.txt`:

| Option | Default | What it does |
|---|---|---|
| `mouse_look` | `direct` | `direct`: raw mouse deltas turn the camera. `stick`: the old analog-stick emulation. |
| `mouse_sensitivity` | `2.00` | Overall mouse speed (also multiplied by the game's own Look Sensitivity). |
| `mouse_degrees_per_count` | `0.050` | Degrees turned per mouse count in direct mode, before `mouse_sensitivity`. |
| `mouse_invert_y` | `0` | `1` inverts vertical look. |
| `mouse_deadzone`, `mouse_smoothing` | `0.28`, `0.03` | Only for `mouse_look = stick`. |
| `mouse_release_key` | `F8` | Frees / recaptures the mouse cursor. |
| `pad_interact` | `NONE` | A controller button that only uses things (doors, switches, vehicles); the Action button then only melees. `NONE` = as on PS3. |

More input options (launcher → Controls page, written to `settings.bat`; log line at start: `[controls] PC options: vertical mouse x.., stick deadzone .., curve .., zoom hold|toggle (game)`):

| Launcher setting | Choices (default) | Environment variable | What it does |
|---|---|---|---|
| Vertical sensitivity | 50%, 75%, **Same as horizontal**, 125%, 150%, 200% | `PAD_MOUSE_SENS_Y=0.1..4` | Up/down mouse speed relative to left/right (direct and stick mouse look). |
| Zoomed sensitivity | 50%, 75%, **Same**, 125%, 150% | `RFOM_ADS_SENS=0.1..3` | Extra mouse multiplier while zoomed or scoped, on top of the automatic zoom matching (`RFOM_ZOOM_SENS`). |
| Zoom | **Toggle** (PS3), Hold | `PAD_AIM_HOLD=1` | Hold: zoomed only while the zoom key / R3 is held. The game's zoom is a toggle, so the port presses zoom again when you let go (keyboard, mouse and the first controller). |
| Stick deadzone | **Default** (24%, per axis, as on PS3), 5-30% | `PAD_DEADZONE=0..0.9` | A radial deadzone for both controller sticks, rescaled so the full range stays usable. Raise it if the view drifts. |
| Stick response curve | **Linear**, Fast, Precise, Very precise | `PAD_CURVE=0.5..3` (exponent; 1 = linear) | Above 1: small movements are slower for fine aiming; below 1: quicker. A curve other than 1 also switches the sticks to a radial 24% deadzone when none is chosen. |

"Restore default controls" also resets these.

**Controllers (SDL3), gyro and light bar** (launcher → Controls page, and in game *More PC Options*; written to `settings.bat`). Rumble works on every pad that has it, DualSense included. **Gyro aiming** (DualShock 4, DualSense, Switch Pro, Joy-Con; off by default) turns the camera directly like the mouse (smooth, not limited by the stick; with `mouse_look = stick` it drives the right stick instead). Leave the controller still for about a second after it connects so its resting drift is calibrated.

| Launcher setting | Choices (default) | Environment variable | What it does |
|---|---|---|---|
| Controller support | **SDL3**, XInput | `PAD_BACKEND=sdl3\|xinput` | XInput = the old path (Xbox pads and wrappers only, up to four, no gyro / light bar). `sdl2` is accepted and uses SDL3. Restart to change. |
| Gyro aiming | **Off**, On, While zoomed | `PAD_GYRO=0\|1\|aim` | `aim`: only while zoomed in / scoped. |
| Gyro sensitivity | **2x**, 0.5x-6x | `PAD_GYRO_SENS=0.05..20` | View degrees per controller degree. |
| Gyro vertical | **Normal**, Inverted | `PAD_GYRO_INVERT_Y=1` | On top of the game's own invert setting. |
| Gyro turning axis | **Controller**, Player | `PAD_GYRO_SPACE=local\|player` | Player: yaw follows gravity, so it works with the controller flat or upright. |
| Gyro calibration | **On connect**, Off | `PAD_GYRO_CALIBRATE=0` | Off: no 1 s still-calibration. |
| Light bar | **Player colour** (blue), Off | `PAD_LIGHTBAR=0` | DualShock 4 / DualSense light bar. |
| Nintendo button layout | **By position**, Nintendo style | `PAD_NINTENDO_SWAP=1` | Switch controllers: A confirms, B cancels. |
| (none) | **Automatic** (the first connected) | `PAD_P1_CONTROLLER=1..8` | Which controller (SDL3: in connection order) is the player's while it is connected; otherwise the first connected one is used. |
| (none) | **On** | `PAD_DEDUP=0` | Keep virtual duplicates (Steam Input / DS4Windows / ViGEm) as separate controllers. |
| (none) | **360** | `PAD_GYRO_STICK_DPS=deg/s` | Gyro speed equal to a full right stick (only when gyro drives the stick). |

### Keys that always work

| Key | What it does |
|---|---|
| **Alt + Enter** | Switch between borderless fullscreen and a window. |
| **F8** | Free / capture the mouse cursor (changeable with `mouse_release_key`). |
| **F10**, or hold **Esc** for 1 s, or hold **Start / Menu** for 1.5 s | **PC menu**: Resume / PC Options / Quit to Desktop (see *Window, focus and quitting*). It always opens on Resume (the mouse takes over the highlight only once moved). The game is paused and muted while it is up, and its SPU threads idle instead of spinning. |
| **F12** or **PrtScn** | Screenshot: a PNG in the `screenshots` folder (`RFOM_YYYYMMDD_HHMMSS_mmm.png`); a "Screenshot saved" toast shows briefly. |
| **F3** | Performance overlay (see *Performance overlay*). |
| **Alt + F4** / the window's close button | Also quits cleanly (waits for a save in progress). |

## Launcher settings

`Launcher.exe` writes `settings.ini` (its own choices) and `settings.bat` (the environment variables the game reads); **Play** runs `run.bat`, which loads `settings.bat`. On the first start (no `settings.ini` yet) the launcher detects your hardware and fills in the recommended settings. Each list tags the value recommended for your PC with a small "recommended" label. The Display, Graphics and Upscaling & Frame Gen pages open with a **Recommended for your PC** panel: what was detected (GPU, VRAM, CPU cores/threads, RAM, screen), the recommended configuration, whether your settings differ from it, and **Auto-detect (recommended)**, which applies it. Hover a setting to see what it does in the help strip at the bottom; most options state their pros and cons.

The options are sorted into pages (the same pages as the Resistance 2 / 3 launchers); each page groups them under headings. `Launcher.exe --page N` opens on page N:

| N | Page | What is on it |
|---|---|---|
| 0 | Display | Screen (mode, window size, monitor, resizable window, Alt+Enter, aspect ratio, seamless level loads), Frame rate (limit, menu frame rate, VSync, tearing), HDR |
| 1 | Graphics | Resolution (render resolution, scaling), Quality (texture filtering, LOD bias, shadow maps), View (field of view), Accessibility (colour-blind filter) |
| 2 | Upscaling & Frame Gen | Anti-aliasing and upscaling (DLSS / FSR / XeSS, DLSS model, sharpening, object motion vectors), Frame generation and latency (Frame Generation, Reflex) |
| 3 | Performance | Shader cache (compile before playing, compile threads, **Rebuild shader cache**) |
| 4 | Controls | Key bindings, Mouse, Controller, Gyro, Controller buttons |
| 5 | Audio | Sound (speaker layout, audio buffer), In the background |
| 6 | Interface & Notifications | Notifications (achievement pop-ups), Movies (hold to skip, skip hint), Overlays (F3 overlay and its corner), Photo mode and Discord |
| 7 | Advanced | Start-up and quitting, Gameplay (gore, adaptive difficulty), Mods and textures, Saves (location, backups, **Open save folder**), Updates |
| 8 | Achievements | Unofficial fan-made achievements read from your save (see *Achievements* under *Trophies*) |
| 9 | Saves & tools | Save backups, the benchmark |
| 10 | About | Version, credits and licences, the key list, folder buttons, the project page, **Report a problem**, **Reset all to defaults** |

Options that are empty by default write nothing to `settings.bat` until changed (the game's default is used).

**Display**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Display mode | **Borderless fullscreen**, Windowed | `RSX_DISPLAY=borderless\|windowed` | Alt+Enter switches in game. |
| Window size | 1280x720 … 3840x2160 (**1920x1080**) | `RSX_WINDOW=WxH` | Windowed mode only. |
| Monitor | **Main display**, Monitor 2-4 | `RSX_MONITOR=0\|2\|3\|4` | Numbered in Windows' order, main display first. The log shows `[display] monitor n of m`. |
| Resizable window / Alt+Enter | **On**, Off | `RSX_RESIZABLE=0`, `RSX_ALT_ENTER=0` | See *PC extras*. |
| Aspect ratio | **Auto** (window shape), 16:9 (PS3), 16:10, 21:9, 32:9 | `RFOM_ASPECT=auto\|off\|16:10\|21:9\|32:9\|<a:b>\|<decimal>` | Ultrawide / Hor+; see *Ultrawide* below. |
| Seamless level loads | **On**, Off | `RFOM_SEAMLESS=0` | |
| Frame rate limit | 30, 55, **60**, 72, 82, 90, 120, 144, 165, Just under refresh (G-Sync / FreeSync), Unlimited | `CHILD_RFOM_FPS` (→ `RFOM_FPS`, `vrr` = just under refresh) | Game speed is the same at every limit (see *Frame rate* below). |
| Menu frame rate | 30, 60, **90**, Same as the game | `RFOM_MENU_FPS` (0 = same as the game) | Cap while the pause / options menu is up over the game. Above ~90 fps the pause menu's dimming layer can flicker. |
| VSync | **Off**, Every refresh, Every 2nd, Every 3rd | `RSX_VSYNC=0..3` | Off is smoothest with FreeSync / G-Sync. |
| Tearing | **Allowed** (lowest latency, VRR), Never | `RFOM_PRESENT=tearing\|notear` | With VSync off: Allowed presents immediately (with G-Sync / FreeSync the screen follows the frame rate); Never drops late frames instead of tearing (`mailbox` / `fifo` are accepted as synonyms). Log: `[D3D12] present: ...`. |
| HDR output, paper white, peak brightness, saturation | **Off**, Auto, HDR10, scRGB | `RFOM_HDR`, `RFOM_HDR_PAPER`, `RFOM_HDR_PEAK`, `RFOM_HDR_SAT` | See *PC extras*. |

Button: **Reset to defaults** (asks first; also on the Graphics and Upscaling & Frame Gen pages) puts the display, graphics and upscaling settings, the field of view and the launcher's original PC options back to the recommended defaults for this PC. Controls are not changed.

**Graphics**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Render resolution | 1280x720 (PS3), 1920x1080, **2560x1440**, 3200x1800, 3840x2160 | `RSX_RES_SCALE=1\|1.5\|2\|2.5\|3` (x 720p) | Above your screen's resolution it acts as supersampling. |
| Scaling | **Smooth**, Sharp pixels | `RSX_FILTER=linear\|point` | How the final image is fitted to the screen. |
| Texture filtering | Trilinear (PS3), 2x, 4x, 8x, **16x** anisotropic | `RSX_ANISO=0\|2\|4\|8\|16` | |
| Texture LOD bias | **Auto** (matches the upscale ratio), 0, -0.5, -1, -1.5, +0.5 | `RSX_LOD_BIAS` | |
| Shadow map resolution | **Auto** (up to 2x), 1x (PS3), 1.5x, 2x, 3x, Same as resolution | `RFOM_SHADOW_SCALE` | |
| Field of view | slider 60-150 (default 82, the game's own) | `RFOM_FOV=<degrees>` | Gameplay camera FOV. |
| Colour-blind filter | **Off**, Protanopia, Deuteranopia, Tritanopia | `RFOM_COLORBLIND=1\|2\|3` | Also live in F9. |

**Upscaling & Frame Gen**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Anti-aliasing | **Off**, AMD FSR 3.1, NVIDIA DLSS / DLAA, Intel XeSS | `RSX_FSR=0\|1\|2\|3` | DLSS needs a GeForce RTX card; XeSS runs on any GPU (XMX on Intel Arc, DP4a elsewhere). If DLSS or XeSS cannot start the game uses FSR 3.1, and if that fails too, plain bilinear scaling. |
| Upscaling | **Native**, Quality 67%, Balanced 58%, Performance 50%, Ultra Performance 33% | `RSX_UPSCALE=1\|0.667\|0.58\|0.5\|0.333` | With FSR / DLSS / XeSS on, the 3D scene is drawn at that share of the render resolution and rebuilt by the upscaler. The menus and HUD stay at full resolution. |
| DLSS model | **Default**, K (sharper), J (smoother) | `RSX_DLSS_PRESET=11\|10` | |
| Sharpening | Off, **Low**, Medium, High | `RSX_SHARPEN=0\|0.25\|0.5\|0.8` | |
| FSR 3.1 sharpening | Off, **0.25**, 0.5, 0.75, 1.0 | `RSX_FSR_SHARP` | |
| Object motion vectors | **On**, Off | `RSX_DLSS_OBJMV=1\|0` | |
| Frame Generation | **Off**, DLSS 2x, DLSS 3x, DLSS 4x (needs Anti-aliasing = NVIDIA DLSS on an RTX 40 / 50), FSR 3 2x (any GPU, any temporal anti-aliasing) | `RFOM_DLSS_FG=0\|2\|3\|4\|fsr3`, `RFOM_FG_BACKEND=auto\|dlss\|fsr3` | See *Frame Generation*. |
| NVIDIA Reflex / low latency | **Off**, On, On + Boost | `RFOM_REFLEX=1\|2` | See *PC extras*. |

**Performance**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Compile shaders before playing | **On**, Off | `RSX_PSO_PRECOMPILE` | See *Shader precompile*. |
| Shader compile threads | **Auto** (all cores but one), 2-16 | `RSX_PSO_PRECOMPILE_THREADS` | |

Button: **Rebuild shader cache** (asks first).

**Controls:** key bindings, the mouse (sensitivity, invert, mouse aiming, the free-cursor key, vertical and zoomed sensitivity, zoom mode, shaking off grabs with the mouse (`RFOM_GRAB_MOUSE=0` = off), the extra crosshair), the controller (use / interact button, deadzone, response curve, controller support, button prompts `RFOM_PAD_GLYPHS=\|xbox\|ps`, Nintendo layout, light bar, pause on disconnect, southpaw, player 1 controller `PAD_P1_CONTROLLER=1..4`), gyro, and the controller button remap. See *Controls* above.

**Audio**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Speaker layout | **5.1 surround**, 7.1 surround, Stereo | `RFOM_SPEAKERS=6\|8\|2` | |
| Audio buffer | **Auto** (100 ms; 150 ms under 12 CPU threads), 80, 100, 150, 200 ms | `PORT_AUDIO_BUFFER_MS` | See *Audio*. |
| In the background | **Pause and mute**, Mute only, Keep running | `RSX_FOCUS_PAUSE=0\|1`, `RSX_FOCUS_MUTE=0\|1` | What happens on Alt+Tab or a click on another window. Pausing frees the CPU/GPU, releases the mouse and resumes exactly where you were. |

**Interface & Notifications**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Achievement notifications | **On**, Off | `RFOM_TROPHY_TOAST=0` | The pop-up with the grade badge when an achievement is earned. Off still records it. |
| Hold to skip movies | **On**, Off | `RFOM_CUTSCENE_SKIP=0` | |
| Show cutscene skip hint | **On**, Off | `PORT_SKIP_HINT=0` (`RFOM_SKIP_HINT=0` also works) | The "Hold Space / A to skip" notification when a movie starts. Off hides it; holding still skips. |
| Performance overlay | **Off** (F3 shows it), On at start, Disabled | `RFOM_OVERLAY=\|1\|0` | See *Performance overlay*. |
| Overlay corner | **Top left**, Top right, Bottom left, Bottom right | `RFOM_OVERLAY_POS=tl\|tr\|bl\|br` | Notifications appear in the opposite top corner. |
| Photo mode (F7), Discord Rich Presence | **On** / **Off** | `RFOM_PHOTO=0`, `RFOM_DISCORD=1` | See *PC extras*. |

**Advanced**

| Setting | Choices (default) | Environment variable | Notes |
|---|---|---|---|
| Skip intro movies | **On**, Off | `RFOM_SKIP_INTRO=0` | |
| Quit keys | **On**, Off (Alt+F4 only) | `RFOM_QUIT_PROMPT=1\|0` | F10 / hold Esc / hold Start → the PC menu (Resume / PC Options / Quit to Desktop). |
| Blood and gore | **Full**, Console region setting | `RFOM_GORE=0` | |
| Adaptive difficulty | **On (PS3 original)**, Off | `RFOM_DYNDIFF=0` | |
| Mods folder | **Off**, On | `RFOM_MODS=0\|1\|<folder>` | See *Mods*. |
| Texture packs | **Off**, Load mods\textures, Dump to texdump | `RSX_TEXPACK=0\|1\|<folder>`, `RSX_TEXDUMP=0\|1\|<folder>` | See *Texture packs*. |
| Save location | **Saved Games folder**, Game folder (portable) | `RFOM_SAVE_DIR=` (empty) \| `portable` \| a folder path | See *Saves* below. |
| Back up saves before playing | **On (keep 10)**, keep 5, keep 30, Off | `RFOM_SAVE_BACKUP` | |
| Check for updates | **On**, Off | — (launcher only) | |

Button: **Open save folder**.

**Achievements, Saves & tools and About** are as before: unofficial achievements, save backups and the benchmark, and the version / credits / folders / **Report a problem** / **Reset all to defaults** (asks first; resets every setting, the key bindings included).

The sidebar picture is drawn by `tools\art\make_r1_art.py` (original artwork, generated in code) and embedded in `Launcher.exe` from `project\launcher_side.png`.

### Hardware presets (auto-detect)

The launcher's recommendation (also available without the launcher with `RFOM_PRESET`) picks the discrete GPU if there is one and maps it to a tier:

| Tier | Typical GPU | Render (output) | Anti-aliasing / upscaling | Filtering |
|---|---|---|---|---|
| Low | integrated graphics, < 1.5 GB cards | 720p | off | 4x |
| Low | GTX 1050 / RX 560 / Radeon 780M, 1.5-3.5 GB | 1080p | DLSS (RTX) or FSR 3.1, Quality (67%) | 8x |
| Medium | GTX 1060-1660 / RX 580 / RTX 3050, 3.5-7 GB | your screen, up to 1440p | DLSS or FSR 3.1 (Quality at 1440p, native AA below) | 16x |
| High | RTX 2070-4060 / RX 6600-7700, 7 GB+ | your screen | DLSS or FSR 3.1 (Performance on 4K screens, Quality at 1440p, native AA at 1080p) | 16x |
| **Ultra** | **RTX 4070 / 5070 or better, or RTX 3080+, with 10 GB+**; RX 6800 / 7800 or better with 12 GB+ | **3840x2160** | **DLSS Performance on RTX** (scene at 1080p → 4K), FSR 3.1 Performance otherwise | 16x |

The discrete GPU is preferred on laptops. Sharpening is Low (Medium for 720p on a larger screen). Frame rate: 60 fps (30 on CPUs with 4 threads or fewer); VSync off.

`RFOM_PRESET=auto|low|medium|high|ultra` fills in only the graphics variables you have **not** set yourself (running through the launcher sets them all, so the preset is for running `build\ResistanceFoM.exe` directly). The log shows `[preset] ...`.

### 1080p rendered → DLSS 4K output (recommended for RTX 40 / 50 cards)

The scene is drawn at 1920x1080, DLSS (Performance) rebuilds it at 3840x2160, and the 4K image is scaled down to your screen, which is much sharper than native 1080p on a 1080p monitor. The HUD and menus are drawn at 4K.

- **Launcher:** Graphics → Auto-detect (recommended) on an RTX 4070 / 5070-class card, or by hand: Render resolution **3840 x 2160**, Anti-aliasing **NVIDIA DLSS / DLAA**, Upscaling **Performance (50%)**.
- **Environment variables:** `RSX_RES_SCALE=3`, `RSX_FSR=2`, `RSX_UPSCALE=0.5` (or `RFOM_PRESET=ultra`).
- DLSS history is reset only on real scene changes (camera cuts, level loads); if DLSS cannot start, FSR 3.1 is used, then plain scaling.

## Frame rate

The game advances its simulation by the real time each frame took, so the frame rate limit (30 / 60 / 120 / 144 / Unlimited …) does **not** change game speed. Details:

- `RFOM_FPS=<n>` (the launcher writes `CHILD_RFOM_FPS`): target frame rate. `0` = uncapped (240), `orig` = the game's own pacing (as on PS3). Values below 30 are raised to 30.
- Scripted sequences that are timed in frames (a chapter's opening walk, for example) run at the original 30 fps steps automatically, then the full rate returns with control (`RFOM_SCRIPT30=0` disables this).
- `RFOM_DT_MAX=<seconds>` raises the largest time step (e.g. `0.05` keeps real speed down to 20 fps; otherwise the game slows down below 30 fps, as on the PS3).
- `RFOM_FPS=vrr` (launcher: *Just under refresh*): caps just below the refresh rate of the monitor the game window is on (refresh − refresh²/3600: 138 fps on 144 Hz, 157 on 165 Hz, 58 on 60 Hz), so a G-Sync / FreeSync screen stays in its variable-refresh range without VSync's latency. Use it with VSync Off. Log: `[fps] RFOM_FPS=vrr: 144 Hz monitor -> 138 fps cap`.
- `RFOM_MENU_FPS` caps menus over the game (default 90; `0` = off).
- Log: `[fps] target 60 fps: ...`.

## Window, focus and quitting

- **Alt+Enter** toggles borderless fullscreen / windowed. **Alt+Tab** is safe: with "In the background" = Pause, the game stops (no CPU / GPU use), mutes, releases the mouse cursor and continues exactly where it was when you come back. The log shows `[focus] paused (window in the background)` / `[focus] resumed`.
- The mouse is released whenever the window loses focus, and with **F8**. The cursor is only hidden while the game has it captured.
- **PC menu:** F10, holding Esc for 1 s or holding Start / Menu for 1.5 s opens the PC menu over the game (from the main menu, the pause menu or gameplay): **Resume**, **PC Options**, **Quit to Desktop**. Arrows / W S A D or the d-pad / left stick select and change, **Enter / Space / A** = OK, **Esc / Backspace / B** = back, **F10 / Start** = close; the mouse works too (left click = OK / next value, right click = previous value). The game is paused and muted, and does not see the keys, while it is up.
  - **PC Options**: *Frame rate limit* (30 … 165, Unlimited), *Anti-aliasing* (Off / AMD FSR 3.1 / NVIDIA DLSS / Intel XeSS), *Mouse sensitivity* (0.10–4.00), *Field of view* (82 PS3 / 90 / 100 / 110 / 120), *Button prompts* (Auto / Xbox / PlayStation). Mouse sensitivity, field of view and button prompts apply at once (the FOV as soon as the game next sets the camera's view — at the latest after a zoom, a checkpoint or a level load); frame rate and anti-aliasing are marked `*` and apply the next time the game starts. Changes are saved straight to `settings.ini`, `settings.bat` and `controls.txt`, so the launcher shows them too.
  - A small hint box (*PC menu: F10 or hold Esc / Start*) appears at the bottom while the game's own pause menu is up, and once ~12 s after start-up. `RFOM_MENU_PC_HINT=0` hides it.
  - `RFOM_MENU_PC=0` turns the PC menu off: the keys then open only the "Quit to desktop?" box.
- **Quit to desktop** (PC menu): a small "Quit to desktop?" box (**Enter / A** = yes, **Esc / B** = back to the PC menu). On yes (or Alt+F4 / the close button) the port waits until no save is in progress (up to 30 s), flushes the logs and exits. Log: `[quit] prompt shown`, `[quit] ... waiting for saves`, `[quit] clean exit`. `ESC_CLOSES=1` makes a single Esc quit at once (debug).
- Entries inside the game's own menus are not possible: the menus are scripted scene files of the original game, not code the port can extend; hence the PC menu is an overlay opened with the keys / long presses above, with the hint box shown alongside the pause menu.
- **Screenshots:** F12 or PrtScn saves the presented frame (with HUD) as a PNG in `screenshots\`. Log: `[screenshot] saved ...`. (The old RSXCAP debug capture on F12 now needs `RSX_CAP_F12=1`.)
- **Intro:** the game has no separate logo videos to skip. The opening front-end sequence is drawn by the game itself and can be skipped as usual with Enter / Space / Cross.

## Ultrawide (Hor+)

`RFOM_ASPECT` (launcher: Display → Aspect ratio; default `auto`):

- `auto` follows the shape of the game window (checked twice a second, so resizing a window or Alt+Enter is followed). On a 16:9 window nothing changes.
- `off` / `0` keeps the PS3's 16:9. `21:9`, `32:9`, `16:10`, any `a:b` or a decimal (1.2-4) force an aspect.
- How it works: the game's camera setter is given the wider aspect and a horizontal field of view widened by the same ratio, so the **vertical** view stays as on PS3 and the sides open up (Hor+); the game's culling follows the wider frustum. Narrower screens (16:10) keep the horizontal view and gain height. The level-of-detail fields keep their 16:9 values, so distant models don't switch to far LODs. Your field-of-view setting applies first and Hor+ on top.
- The image the game draws keeps its 16:9 pixel grid and is stretched to the screen's shape at present time, so on a 21:9 screen each pixel is ~1.3x wider. With DLSS / FSR output at 3840 px (the 1080p → 4K preset), a 2560x1080 or 3440x1440 screen still gets more horizontal pixels than it has.
- Only 16:9 cameras are widened. Movies, loading screens and other 2D screens (no 3D camera for 0.5 s) keep their 16:9 picture with black bars.
- **The HUD is stretched with the view** (it is not kept in a centred 16:9 area): the renderer cannot reliably tell the game's HUD draws from its 3D draws, so they are scaled together.
- Log: `[aspect] RFOM_ASPECT: auto (window aspect)`, `[aspect] window aspect 2.370 (2560x1080): Hor+ on 16:9 cameras`, `[aspect] Hor+: 16:9 camera (aspect 1.778) -> 2.370, horizontal FOV tan x1.333`, then `[aspect] Hor+ camera set N times/s`.

## Performance overlay

**F3** toggles a small overlay in a corner of the game window: the game's frame rate and frame time (with the worst frame of the last half second), the renderer's frame rate, draws per frame, and the CPU time per draw in µs (the same measurement as `RSX_DRAWPROF`, refreshed twice a second; measuring costs a little while the overlay is up and nothing while it is hidden). `RFOM_OVERLAY=1` shows it from the start, `RFOM_OVERLAY=0` disables F3; `RFOM_OVERLAY_POS=tl|tr|bl|br` picks the corner. With NVIDIA Reflex active it also shows the driver's PC latency (simulation start to present, render submit to present, GPU time; average of the last 64 frames).

The overlay and the port's notifications (toasts: "Screenshot saved", the movie-skip hint, trophies) are drawn by the renderer into the game's own image, after the F12 screenshot copy, so they show in every display mode (and in recordings of the game window) but never in F12 screenshots. They are pre-drawn once per change and cost nothing measurable. Toasts sit in the top corner opposite the overlay for 5 s, queue while the game is in the background, and use the same style as the Resistance 2 / 3 ports (dark panel, coloured letter badge). `RFOM_OSD=window` uses the old separate always-on-top windows instead (also the automatic fallback if the renderer can't set the pass up; the log then says `[osd] in-swapchain notifications unavailable, using overlay windows`, otherwise `[osd] in-swapchain notifications ready`). The start-up "Compiling shaders" panel is always a separate window (nothing is presented while it shows).

## Mods

`RFOM_MODS=1` (launcher: Advanced → Mods folder → On) loads replacement game files from the `mods` folder in the port's folder (`RFOM_MODS=<folder>` uses any other folder):

```
mods  My Texture Mod    USRDIR\<path of the file inside the game's USRDIR>
  Another Mod    USRDIR\...
```

- A file replaces the game file with the same path (case does not matter). Every file access of the game goes through the check (cellFs, sys_fs and the port's own loaders), and only files you add are changed; nothing in `gamefiles` is touched.
- Mods load in alphabetical order; when two mods have the same file, the later one wins (the log says so). A folder starting with `_` or `-` is skipped (an easy way to turn a mod off).
- Log: `[mods] <mod>: N file(s)`, `[mods] override: usrdir/... -> <file>`, `[mods] N mod(s), M file override(s) from <folder>` at start, and `[mods] using <file>` the first time the game opens each replaced file.

### Texture packs

Textures can also be replaced one by one, without touching the game's archives:

1. `RSX_TEXDUMP=1` (launcher: Advanced → Texture packs → Dump to texdump) writes every texture the game draws with, once, to `texdump\` (`RSX_TEXDUMP=<folder>` for another folder). The name is `<16-digit key>_<width>x<height>_<format>_m<mips>` and each texture is listed in `texdump\manifest.tsv`; the key is a hash of the texture's content (all mip levels) plus size, format and mip count, so it is the same on every run (not a memory address); colour textures are saved as `.png`, DXT1/3/5 textures as `.dds` with their mip levels. Dumping slows loading; turn it off when done.
2. Edit the files you want and copy them to `mods\textures\` (sub-folders are fine; only the leading 16-digit hash of the name matters).
3. `RSX_TEXPACK=1` (Advanced → Texture packs → Load mods\textures; `RSX_TEXPACK=<folder>` for another folder) loads them. The log says `[texpack] N replacement textures in ...` at start and `[texpack] replaced ...` as they are used.

A replacement must have the same width and height as the original; a `.dds` must also keep its DXT format (DXT2/DXT4 are accepted for DXT3/DXT5). A `.png` replaces a colour (A8R8G8B8) texture, and its mip levels are generated; a `.dds` brings its own levels (missing ones keep the game's). A file that does not match is skipped with a log line. Cube maps are not dumped or replaced. With both options off the upload path is unchanged.

## Trophies

Resistance: Fall of Man (2006) has no trophies: the game does not use the trophy library and its disc has no trophy data, so on the retail game nothing is ever unlocked. The launcher has fan-made **Achievements** instead (below).

The port still carries the same trophy support as the Resistance 2 / 3 ports, for a build or mod that does call the trophy library (`sceNpTrophyUnlockTrophy`): each unlock is logged (`[trophy] unlocked #N <name> (<grade>)`), recorded in `%USERPROFILE%\Saved Games\Resistance Fall of Man\trophies.json` (same format as R2 / R3; names and grades come from `gamefiles\PS3_GAME\TROPDIR\<id>_00\TROPHY.TRP` when present), and shown as a toast with the grade badge (P platinum, G gold, S silver, B bronze) drawn inside the game image (see *Performance overlay*), so it also shows in fullscreen. A trophy already in that record is never announced twice (`[trophy] #N already earned ...`), and the platinum is awarded when every trophy counting towards it is unlocked. `RFOM_TROPHY_TOAST=0` turns the toast off (unlocks are still recorded).

### Achievements (unofficial, fan-made)

The launcher's **Achievements** page (page 8, between Advanced and Saves & tools) shows 16 achievements made up for the port and labelled *Unofficial · fan-made*. They are not part of the game, never pop up in game, and are computed when you open the page from your profile save, `...\savedata\BCUS98107_PROFILE\PROFILE.SAV` (the folder the *Save location* setting picks; `PS3_GAMEDATA_ROOT` wins; a save not yet moved to Saved Games is read from `gamedata`). The file is only read, never written. Like the R2 / R3 Trophies pages there is a summary (earned / total, %, per grade) and a grade per achievement (P / G / S / B), with progress such as *7 / 20* for the counted ones. The save has no dates, so none are shown; the page shows when the save was last written.

| Achievement | Grade | Rule (from PROFILE.SAV) |
|---|---|---|
| First Mark, Sharpshooter, Veteran, Sentinel | B, B, S, S | 1 / 10 / 20 / 30 skill points |
| Perfectionist | G | all 43 skill points (the game's own *all skill points* rule) |
| Point Collector, High Scorer | B, S | 25 / 75 of the 126 skill-point points |
| Into the Fray, Pushing Back, Holding the Line, Liberator | B, B, S, G | 1 / 4 / 7 / 10 of the 11 campaign chapters finished (either campaign record) |
| Superhuman Ready, Superhuman | S, G | the game's *Superhuman unlocked* / *Superhuman completed* flags |
| Black Ops, Death's Head | S, S | the game's *Black Ops skin* / *Skull skin unlocked* flags |
| Fall of the Chimera | P | every other achievement |

Only what the save records reliably is used (decoded from the game's code: the skill point bit array, the point values and the per-chapter record). Not used: the skill point names (they are localised text), which difficulty a chapter was finished on (the save stores a number per chapter, but the mapping to Casual / Normal / Hard / Superhuman isn't confirmed), the journal / intel, and weapons. Before you have a save it says *Play the game to start earning achievements*.

## Saves

- Saves live in **`%USERPROFILE%\Saved Games\Resistance Fall of Man`** by default, so updating or reinstalling the port can't touch them. Your save files are in `...\dev_hdd0\home\00000001\savedata\` (`BCUS98107_*` folders).
- On the first start after this change, existing saves in the port's `gamedata` folder are **copied** there (nothing is overwritten; the old copy stays as a backup). Log: `[saves] folder: ...` and `[saves] copied N save file(s) ...`.
- `RFOM_SAVE_DIR=portable` (launcher: Save location → Game folder, or a `portable.txt` file next to `Launcher.exe`) keeps everything in `<port>\gamedata`; `RFOM_SAVE_DIR=<folder>` uses any folder; `PS3_GAMEDATA_ROOT` (advanced) overrides both.
- Launcher → **Open save folder** opens it. The game's own options are stored in the save profile, so they move with it.

## Shader precompile and rebuilding the shader cache

Shader caches are never shipped with the port: they are built on your PC, from the shaders your game uses, the first time each one is seen. The renderer lists every pipeline (PSO) it builds in `shadercache\pso_manifest_fe2.bin` (front end) and `pso_manifest_game2.bin` (game) and keeps the shaders as `.hlsl` source and compiled `.dxbc`. Launcher, Performance page, **SHADER CACHE**:

- **Compile shaders before playing** (default On, `RSX_PSO_PRECOMPILE=1`): right after the D3D12 device is created and before the first frame, every pipeline in the program's manifest is built, in parallel, and the game waits for it, so nothing seen before compiles during play. Each program (front end, game) does this for its own manifest. Progress: a centred **Compiling shaders N / M** panel with a progress bar over the game window (shown after 0.3 s) and log lines `[PSOWARM] shader precompile: building all N pipelines ... on T threads`, one every 10 %, then the `[PSOWARM] N pipelines pre-built in ... ms` summary. A `.dxbc` missing from `shadercache` is compiled again from its `.hlsl` (counted as "shaders recompiled from .hlsl"). Off: the pipelines are pre-built on one background thread while you play, as before (`RSX_PSO_PREWARM`).
- **Shader compile threads** (`RSX_PSO_PRECOMPILE_THREADS`, default Auto = every logical core but one, 1 to 64).
- **Object motion vectors** (launcher page Upscaling & Frame Gen, `RSX_DLSS_OBJMV`, default On): DLSS/FSR get per-object motion for moving characters and objects; Off = camera motion only (try it if moving objects smear or stutter).
- **Offline shader scan:** [shaderscan](https://github.com/miroslavdanutalexandru-alt/shaderscan) (GPL-2.0, its own repository) is downloaded by Setup / `build.bat` into `tools\shaderscan.exe`: `tools\get_shaderscan.ps1` fetches the release pinned in `tools\shaderscan.version` and checks its SHA-256 (offline: skipped; set `SHADERSCAN_EXE=<path>` to use your own build). It is not part of the port's source or Setup, and the launcher works without it. When `shaderscan.exe` is next to `Launcher.exe` (or in `tools\`), Play first runs `shaderscan --check --out shadercache` (after saving the settings: the stamp also records `RSX_RES_SCALE`, `RSX_FP_KSLOTS`, `RSX_ALPHATEST`, `FP_SWAP_RB` and `RSX_FP_DEBUG`, which change the generated shaders); if the cache isn't seeded it runs `shaderscan --game <folder> --out shadercache --backend r1d3d12 --progress`, which translates every shader program in the game files and FXC-compiles the `<key>.dxbc` / `.hlsl` pairs the game uses (about 1-2 minutes the first time; later runs skip existing files; needs `d3dcompiler_47.dll`). A progress panel shows it (Esc or **Skip for now** stops it; the game starts either way); output goes to `logs\shaderscan.log`. **Rebuild shader cache** also deletes `shaderscan.stamp`, so the next Play scans again.
- **Rebuild shader cache** (button, asks first; refused while `ResistanceFoM.exe` or `ResistanceFoMGame.exe` is running) deletes the compiled shaders and keeps what tells the game which ones to build. Deleted, from `shadercache\` and from its backup in `%LOCALAPPDATA%\ResistancePort\shadercache` (`run.bat` restores missing files from there):
  - `*.dxbc` that have a matching `.hlsl` (compiled shaders; a `.dxbc` without its source is kept, it could not be rebuilt)
  - `pso_gpu_stamp.txt` (the GPU and driver they were built for)

  Kept: `pso_manifest_*.bin` and `*.hlsl`. The compiled pipelines themselves live in the graphics driver's own shader cache (D3D12 has no app-side pipeline file here); the next start rebuilds every shader and pipeline, which also refills that cache.
- **GPU or driver change:** `shadercache\pso_gpu_stamp.txt` records the adapter's vendor, device, subsystem and revision ids, the driver (UMD) version and the GPU name. When they differ at start-up the log says `[PSOWARM] GPU or driver changed since the pipelines were built (was: ..., now: ...)` and the precompile runs before the first frame even with the option off.

## Performance

Measured in York (about 2,600 draws per frame), DLSS rendering at 1080p with 4K output, on an RTX 5070: **a locked 60 fps at about 5 µs of CPU per draw**. Low-end PCs benefit most from the performance work, because the per-draw cost is CPU time.

Performance fixes have been applied as far as realistically possible, all profile-driven, and carried over from the Resistance 2 port where they fit R1's own Direct3D 12 renderer:

- **Renderer (per draw):** a redundant-state filter (draws that change nothing skip the state re-apply), fragment-constant patch memoisation, pipeline (PSO) reuse in the pre-pass, cached descriptors, a disk shader / pipeline cache with background compilation and a backup in `%LOCALAPPDATA%\ResistancePort\shadercache`.
- **SPU / PPU runtime:** a fast SPU wait loop (short spin, then sleep), yielding instead of spinning on CPUs with fewer than 8 threads, priority boosts and core pinning only where there are enough cores, a high-resolution sleep timer, and the network HTTP poll (`cellHttpClientPollConnections`) implemented so the HTTP thread no longer busy-spins.
- **Performance overlay:** F3 (see *Performance overlay*).
- **Per-draw profiler:** `RSX_DRAWPROF=1` logs the CPU cost per draw by phase every 5 s:
  `[drawprof] 5.0 s: N draws (... guest draw calls, ... tex binds), D draws/frame, F fps | X us/draw = record (verts, consts, tex binds) + prepass (srv, pso, tex upload) + replay (rt, pso, state, draw)`. The target is under 6 µs per draw.

Performance switches (defaults are the recommended values; set them before `run.bat` or in `settings.bat`):

| Variable | Default | What it does |
|---|---|---|
| `RSX_DRAWPROF` | `0` | `1` = per-draw CPU profile in the log every 5 s. |
| `RSX_DRAW_FILTER` | `1` | Redundant-state filter. Log: `[draw] redundant state filter on`. |
| `RSX_FPK_MEMO` | `1` | Fragment-program constant memoisation. |
| `RSX_SHADERCACHE`, `RSX_PSO_ASYNC` | `1` | Disk shader cache; background pipeline compilation. |
| `SPU_FAST_WAIT` | `1` | Fast SPU wait loop. |
| `SPU_WAIT_YIELD` | auto (on below 8 logical CPUs) | Yield the core on every SPU poll. |
| `PS3_PRIO_BOOST` | auto (on with 8+ logical CPUs) | Raise the priority of the game's busy threads. Log: `[threads] host priority boosts ...`. |
| `RFOM_MAIN_CORE` | auto (on with 12+ logical CPUs) | Pin the main thread (and the RSX FIFO thread, `RFOM_FIFO_CPU`) to their own cores. Log: `[affinity] ...`. |
| `RFOM_SPU_WORKERS` | unset (3 SPUs, new wake protocol) | SPURS job scheduler. `0`/`1` = the old scheduler exactly. `4`-`6` or `auto` (one per free physical core) = *experimental* extra virtual SPUs for job lists that may run on any SPU. Log: `[spu-mt] ...` once at start. |
| `RFOM_SPU_WAKE` | `1` | Targeted wakes: a job hand-off wakes only its lane, a kick wakes only the scheduler thread (`0` = wake every lane on every kick, the old way). |
| `RFOM_SPU_AFFINITY` | `1` | Ideal-CPU hints spreading the SPU lane threads over the cores the main / FIFO threads do not use (physical cores first, then SMT siblings). |
| `RFOM_SPU_PRIO` | `1` | `1` = the SPURS scheduler thread above normal (with `PS3_PRIO_BOOST`); `2` = the lanes too; `0` = off. |
| `PS3_USLEEP_TIMER_CACHE` | `1` | Reuse a high-resolution timer for short sleeps. |

## Audio

When the frame rate drops, the game's own audio mixing (on the recompiled PPU/SPU threads) can fall behind real time. The port therefore keeps a buffer of mixed audio between the game and the sound card, as RPCS3 does with its "audio buffering" and "time stretching" options:

- **Buffer:** mixed audio goes into a ring buffer with a target of `PORT_AUDIO_BUFFER_MS` (100 ms by default). The sound card is kept topped up from it, about 30 ms at a time.
- **Time stretching:** when the buffer drops below `PORT_AUDIO_STRETCH_THRESHOLD` % of the target, playback slows down through [SoundTouch](https://www.surina.net/soundtouch/) without changing the pitch, so a slow stretch of frames sounds slightly slower instead of gapping. Tempo returns to 1.0 as the buffer refills.
- **Underrun:** if the buffer still runs dry (a hitch longer than the buffer), the last few milliseconds fade out to silence and fade back in when audio returns. No repeated blocks, no clicks.
- **Mixer thread:** runs at TIME_CRITICAL priority with Windows MMCSS "Pro Audio" scheduling. It also stays off the cores reserved for the main game thread and the RSX thread (`RFOM_MAIN_CPU`, `RFOM_FIFO_CPU`).
- **Late game audio:** when the game's next block of audio has not been written in time, the mixer waits up to `PORT_AUDIO_LATE_WAIT_MS` (20 ms) for it. The game's audio clock slips slightly and the buffer absorbs the slip. While the buffer is below target, the game's audio clock runs 3 % fast (`AUDIO_CATCHUP_PCT`) to refill it.

| Variable | Default | Effect |
|---|---|---|
| `PORT_AUDIO_STRETCH` | `1` | SoundTouch buffer and time stretching. `0` = the previous audio output path, unchanged. |
| `PORT_AUDIO_BUFFER_MS` | `100` (`150` with fewer than 12 logical CPUs) | Target buffer, 20–500 ms. More = fewer underruns, more latency. |
| `PORT_AUDIO_STRETCH_THRESHOLD` | `75` | Stretch below this % of the target buffer. |
| `PORT_AUDIO_STRETCH_MIN` | `0.5` | Slowest tempo while stretching (0.1–1). |
| `PORT_AUDIO_FADE_MS` | `5` | Fade-out / fade-in length on an underrun. |
| `PORT_AUDIO_DEVICE_MS` | `30` | Audio handed to the sound card at once. |
| `PORT_AUDIO_CPU` | – | Pin the mixer thread to one logical CPU; `-1` leaves its affinity alone. |
| `PORT_AUDIO_MMCSS` | `1` | `0` = no MMCSS "Pro Audio" scheduling for the mixer thread. |
| `PORT_AUDIO_LATE_WAIT_MS` | `20` | Longest wait for a late block of game audio (0–40, `0` = off). |
| `AUDIO_CATCHUP_PCT` | `3` | How much faster the game's audio clock runs while the buffer refills (0–20). |
| `PORT_AUDIO_LOG` | `5000` | Period of the `[audio]` log line in ms. `1` = every second (also with `R2_RSX_PROF=1`), `0` = off. |

**Reading the log.** At start-up, `logs\run.log` shows `[audio] output: ... SoundTouch stretch (...)` with the MMCSS state and the mixer thread's affinity. Every 5 seconds there is a line like:

```
[audio] 5.0 s: buffer min 82 avg 101 ms (target 100) | tempo min 1.00 avg 1.000 (stretched 0%) | underruns 0 (silence 0 ms, device dry 0) | ...
```

- `buffer min` well above 0 and `avg` near the target: healthy.
- `tempo min`/`avg` below 1.00 and `stretched` above 0 % during a frame-rate drop: the stretcher is covering it. Tempo should be back to 1.000 within a second or two.
- `underruns` and `device dry` should stay at 0. A non-zero count means a hitch longer than the buffer; try a larger `PORT_AUDIO_BUFFER_MS`.
- `ring overflow` at the end: the game produced audio faster than it was played, so the oldest audio was dropped (harmless after a long pause).

The offline test for this stage is `ps3recomp\libs\audio\tests\test_audio_out.cpp` (run it with `ps3recomp\libs\audio\tests\run_test_audio_out.ps1`).
## Crash and hang reports

- If the game crashes, a minidump and a line in `logs\crash\crashes.txt` are written (`[crash] reporter on` at start). Please send both with `logs\run.log`.
- A hang watchdog writes a dump if no frame is presented for 90 s (not while paused). `RFOM_HANG_S=<seconds>` changes the limit (minimum 10). `RFOM_CRASH_DUMPS=0` turns reports off, `=full` writes full-memory dumps.
- `FREEZE_RELAUNCH=1` restarts the game process after a freeze (pause-aware).

## PC extras

Set in the launcher (the Display, Graphics, Upscaling & Frame Gen, Controls, Interface & Notifications and Advanced pages, and the **Saves & tools** page) or in game with **F9** (or F10 > *More PC Options*). Changes made in game are saved to `settings.bat` / `settings.ini`, so the launcher shows them too.

| Variable | Default | What it does |
|---|---|---|
| `RSX_RESIZABLE` | `1` | The window can be resized or maximised; the swap chain follows the new size. `0` = fixed size. |
| `RSX_ALT_ENTER` | `1` | Alt+Enter switches between a window and borderless fullscreen. |
| `RFOM_SKIP_INTRO` | `1` | Skips the intro movie in the front end. (The game has no logo or legal videos; the intro skip is not yet verified.) |
| `RFOM_CUTSCENE_SKIP` | `1` | Hold Space / Enter or (A) / Start for 1 s to skip a playing movie. Only full-screen movies are skipped, not in-engine scenes. |
| `PORT_SKIP_HINT` (or `RFOM_SKIP_HINT`) | `1` | `0` hides the "Hold Space / A to skip" notification when a movie starts; holding still skips. Launcher: Interface & Notifications › *Show cutscene skip hint*. |
| `RFOM_TROPHY_TOAST` | `1` | `0` hides the achievement / trophy pop-up; unlocks are still recorded. Launcher: Interface & Notifications › *Achievement notifications*. |
| `RFOM_HDR` | off | `1` = HDR10 (PQ, Rec.2020), `scrgb` = scRGB (FP16). The final image is converted from SDR with the paper-white setting; falls back to SDR when the display or Windows HDR is off. |
| `RFOM_HDR_PAPER` | `203` | HDR paper white in nits (how bright the game's white is). |
| `RFOM_HDR_PEAK` | the display's peak (else `1000`) | Display peak in nits, sent as HDR metadata and used for the roll-off. |
| `RFOM_HDR_SAT` | `1` | HDR colour saturation, 0…2 (1 = as the game). |
| `RFOM_REFLEX` | off | `1` = On, `2` = On + Boost. On an NVIDIA GPU this is **NVIDIA Reflex Low Latency** through NVAPI (`tools/nvapi`): `NvAPI_D3D_SetSleepMode`, one `NvAPI_D3D_Sleep` per frame right after the port's frame limiter, and the latency markers (simulation start/end, render submit start/end, present start/end, input sample); log `[reflex] NVIDIA Reflex Low Latency ...`. Elsewhere (other GPUs, old drivers) it falls back to a waitable DXGI swap chain with at most one queued frame (`[reflex] NVAPI unavailable ...`). Never a frame limiter: Reflex's own interval is always 0 and the Frame rate limit (`RFOM_FPS`) still decides the cap. |
| `RFOM_PHOTO` | `1` | **F7** photo mode: pauses the game, hides the port's overlays, **F12** saves a clean screenshot. No free camera (see below). |
| `RFOM_DISCORD` | off | Discord Rich Presence through the local Discord pipe; does nothing when Discord isn't running. Application id: `RFOM_DISCORD_APPID` or `discord_appid.txt` next to run.bat. |
| `RFOM_SAVE_BACKUP` | `10` | Play backs up your saves (when they changed) to `save_backups` next to the saves folder, keeping the newest N; `0` = off. The Saves & tools page lists, backs up, restores and deletes backups and opens the folders. |
| `RFOM_COLORBLIND` | off | Colour-blind filter on the final image: `1` protanopia, `2` deuteranopia, `3` tritanopia (also live in F9). |
| `RFOM_CROSSHAIR` | off | An extra centre crosshair: `dot` or `cross`. |
| `RFOM_PAD_DISCONNECT_PAUSE` | `1` | Pauses when the controller in use disconnects; reconnect it or press Enter. |
| `PAD_SOUTHPAW` | off | Swaps the controller sticks. |
| `PAD_MAP_<BUTTON>` | — | Controller remap (e.g. `PAD_MAP_CROSS=SQUARE`); set on the Controls page or in F9 > Controller layout. Keyboard and mouse keys are on the Controls page as before. |
| `RFOM_BUILD_DIR` | `build` | Which folder run.bat starts the game from (a Launcher.exe in a sub-folder sets it to that folder). |

**Benchmark** (Saves & tools > *Run benchmark*, or `RFOM_BENCH=1`): the game continues your campaign by itself (`RFOM_BENCH_SCRIPT`, default `120:START,8:CROSS,8:CROSS,8:CROSS,8:CROSS`, frames:button; `none` = start a level yourself), waits `RFOM_BENCH_SETTLE` s (45) in the level, measures `RFOM_BENCH_SECONDS` (60), writes `logs\bench\RFOM_bench_<date>.txt` (average fps, 1% low fps, frame times, CPU µs per draw) plus `logs\bench\bench_history.csv`, and closes (`RFOM_BENCH_QUIT=0` keeps it running). Focus pause is off during a run.

Already covered elsewhere: the frame rate limit (any cap is safe here; the game uses its own frame time), automatic keyboard / controller prompts, the FOV slider, key binding and hot-plug.

Not included, and why: motion blur / film grain / depth-of-field toggles (the game's own switches for these aren't known yet, and skipping their draws is unverified); subtitle size / background (drawn by the game's own font code); hiding the game's HUD and a free camera in photo mode (no known camera hook); trophies (the game has none; the port's trophy toasts are ready for a build that has them, see *Trophies*); exclusive fullscreen (the front end and the game are two processes handing the screen over through a cover window, and a display-mode switch at each hand-over would flash and fight for focus; borderless fullscreen with tearing allowed already gets the same latency and G-Sync / FreeSync behaviour on Windows 10/11).

## Other options

| Variable | Default | What it does |
|---|---|---|
| `RSX_LOD_BIAS` | `auto` | Texture mip LOD bias, -2…2. `auto` = log2 of the upscaling ratio when FSR / DLSS upscaling is on (-1 at Performance), so textures keep output-resolution detail; 0 otherwise. |
| `RFOM_FOV_ANYCAM` | off | `1` = zoom-matched mouse speed follows any camera's FOV (old behaviour); by default only the player's view camera counts. |
| `RFOM_ZOOM_SENS` | `1.0` | Zoom-matched mouse speed when aiming / scoped: 1 = a mouse move covers the same part of the screen zoomed or not, 0 = off, up to 2. |
| `RFOM_KEY_PROMPTS` | `1` | `0` = no keyboard / Xbox prompt text (PlayStation prompts always). |
| `RFOM_PC_TEXT` | `1` | PC wording in the game's text, all languages: "PLAYSTATION®3" → "PC", "PLAYSTATION®Network" → "online service", HDD / save-icon / power-off warnings and "press and hold the PS button" → PC wording ("Press F10 to quit the game."), "wireless controller" → "controller". Only the text in memory changes (the game files aren't touched). Log: `[prompts] text file: N console wording(s) -> PC wording`. `0` = the original console wording. |
| `RFOM_SPEAKERS` | `6` | Speaker layout: 6 = 5.1, 8 = 7.1, 2 = the game's stereo layout. |
| `RFOM_GORE` | `1` | Full gore (as the US release); `0` = the console setting, `-1` = reduced. |
| `PS3_MUTE`, `PS3_NO_AUDIO` | off | Mute / disable audio. |

Not included, and why: MSAA (the renderer's render targets are shared with PS3-format readbacks and post-process passes that expect single-sample surfaces; FSR / DLSS anti-aliasing covers it); a higher-precision depth buffer (the game's depth is read back in PS3's 24-bit format by its own effects; changing it risks shadow and depth-test artefacts).

## DLSS and upscaling

- **Anti-aliasing → NVIDIA DLSS / DLAA** uses NVIDIA DLSS on GeForce RTX cards and falls back to FSR 3.1 on other GPUs (log `[DLSS] NGX unavailable ... using FSR 3.1`); if FSR cannot start either, the image is scaled normally (`[FSR] ... FSR off`). The DLSS SDK is downloaded by Setup into `tools\dlss` (if that fails, the port still builds without it).
- **Upscaling** (with FSR, DLSS or XeSS on): renders the 3D scene at 67 / 58 / 50 / 33 % and lets the upscaler rebuild it at the render resolution. The DLSS preset (DLAA, Quality, Balanced, Performance, Ultra Performance) follows the chosen ratio. Menus and HUD stay at full resolution.
- **Anti-aliasing → Intel XeSS** (`RSX_FSR=3`) uses Intel XeSS 2 (`libxess.dll`, next to the exe) with the same inputs as DLSS (colour, depth, motion vectors, jitter). It runs on any GPU: XMX on Intel Arc, DP4a on NVIDIA / AMD. The XeSS preset follows the Upscaling ratio (Native → Native AA, 67% → Ultra Quality, 58% → Quality, 50% → Balanced, 33% → Ultra Performance; log `[XeSS] <preset> at WxH -> WxH`). `RSX_XESS_QUALITY=aa|uqp|uq|q|b|p|up` asks for a specific preset (used when its input range covers the chosen Upscaling ratio, otherwise auto); `RSX_XESS_JITTER_SIGN=-1` flips the jitter sign if the image shimmers. If `libxess.dll` is missing, XeSS cannot start, or it fails while running, FSR 3.1 takes over (`[XeSS] ... using FSR 3.1` / `switching to FSR 3.1`).
- The shader cache is backed up to `%LOCALAPPDATA%\ResistancePort\shadercache` after each session and restored before launch, so reinstalling or updating doesn't bring back first-visit shader stutter.

### DLSS Frame Generation

**Launcher → Upscaling & Frame Gen → Frame Generation** (`RFOM_DLSS_FG=0|2|3|4|fsr3`, default Off) adds NVIDIA DLSS Frame Generation: 2x on an RTX 40, 2x / 3x / 4x (DLSS 4 Multi Frame Generation) on an RTX 50. It raises smoothness when the CPU is the limit (heavy areas of this port are CPU-bound) and adds a little input latency; 4x is for high-refresh displays. It runs through NVIDIA Streamline 2.14.16 (`rfom_dlssg.dll` plus the Streamline DLLs next to the game; Setup downloads the SDK into `tools\streamline` after you accept NVIDIA's licences).

- It needs **Anti-aliasing = NVIDIA DLSS** (it reuses DLSS's depth, motion vectors and jitter) and turns **NVIDIA Reflex** on (required; *On + Boost* is kept if chosen). It is off, with a `[DLSS-G] frame generation disabled: ...` line in the log, on GPUs without DLSS-G, when the DLLs are missing, with FSR / XeSS / no upscaler, with scRGB HDR, and in the front-end program (main menu); with `RFOM_FG_BACKEND=auto` (default) FSR 3 2x is then tried instead (see *FSR 3 Frame Generation*). 3x / 4x on a GPU that only supports 2x uses 2x (`[DLSS-G] 4x frame generation is not supported on this GPU -- using 2x`).
- It switches itself off for frames without a 3D scene (menus, .bik movies, loading screens), while the window is in the background or minimised, and while the window is resized, and back on two frames later. A camera cut or a level load resets its history.
- **The HUD is not generated.** The renderer knows which draws are the HUD (drawn into the final picture after the scene, no depth, alpha-blended, not reading the scene) and hands DLSS-G the picture without the HUD plus the HUD as its own layer, which DLSS-G lays over every generated frame, so text and crosshairs do not warp or ghost. Frames it cannot classify cleanly are generated whole (as without the split). The port's own overlay (F3, notifications) is always in that layer. `RFOM_FG_HUDSPLIT=0` turns the split off; `RFOM_FG_HUDDBG=1` shows the HUD-less picture, `2` the separated HUD over grey (works with DLSS on even without frame generation; for checking).
- **Frame rate limit:** it caps the *rendered* frames; the screen gets 2, 3 or 4 times as many. On a 144 Hz G-Sync / FreeSync screen use about 69 (2x), 46 (3x) or 34 (4x) with VSync Off, so the output stays just under the refresh rate. VSync 1/2 or 1/3 is not supported with frame generation and uses normal VSync.
- Log: `[DLSS-G] Streamline ready ...`, `[DLSS-G] frame generation on: 3x, 1920x1080, HUD kept out of generated frames`, and every 600 frames a status line with the frames presented per rendered frame and how many frames had the HUD separated. `RFOM_FG_SLLOG=1` adds Streamline's own log.

### FSR 3 Frame Generation (fallback)

On GPUs without DLSS Frame Generation (RTX 20 / 30, AMD, Intel) the **FSR 3 2x** choice (`RFOM_DLSS_FG=fsr3`) uses AMD FSR 3 frame generation (FidelityFX SDK 2.3.0, open source, MIT): one generated frame per rendered frame. `RFOM_FG_BACKEND` picks the backend: `auto` (default: DLSS-G when it can run, otherwise FSR 3 -- the DLSS choices fall back to FSR 3 2x with a `[FG] DLSS Frame Generation unavailable -- trying AMD FSR 3 frame generation (2x)` line), `dlss` (DLSS-G only) or `fsr3` (FSR 3 only, also on RTX 40 / 50).

- It needs any temporal anti-aliasing / upscaler (**FSR 3.1, NVIDIA DLSS or Intel XeSS**; their depth, motion vectors, jitter and resets are reused) and `amd_fidelityfx_framegeneration_dx12.dll` next to the game (Setup downloads it into `tools\ffx_bin`, ~40 MB, pinned by SHA-256; `build.bat` copies it). It is off, with a `[FSR3-FG] frame generation disabled: ...` line, without them, with scRGB HDR, in the front-end program, or if the GPU / driver cannot create it.
- Same rules as DLSS-G: off for frames without a 3D scene, in the background, minimised and while resizing; camera cuts reset it; the frame rate limit caps rendered frames (144 Hz G-Sync: about 69). Reflex stays as set in its own option.
- The HUD split is the same: FSR 3 interpolates the HUD-less picture and the HUD + overlay layer is drawn over each generated frame; the rendered frames are shown exactly as drawn. `RFOM_FG_HUDSPLIT` / `RFOM_FG_HUDDBG` work the same; `RFOM_FG_FSR3DBG=1` adds FSR 3's own reset / tear indicators.
- Log: `[FSR3-FG] AMD FSR 3 frame generation ready ...`, `[FSR3-FG] frame generation on: 2x, 1920x1080, HUD kept out of generated frames`, and every 6000 presents a count of generated frames and HUD-split frames.

## Steam Deck / Linux

The port is a Windows program. On Linux and the Steam Deck it runs through Proton (Wine). This has not been tested on a real Deck yet, so treat it as experimental.

1. Install and build the port on a Windows PC (Setup needs Windows: it uses winget and Visual Studio). No file picker is needed: the installer and Setup find the game on their own, or take a typed path. Then copy the whole port folder to the Deck, for example to `/home/deck/Games/ResistancePort` (or a microSD card).
2. In Desktop Mode, open Steam → **Games → Add a Non-Steam Game to My Library…**, click **Browse**, and pick `Launcher.exe` in the port folder.
3. Right-click the new entry → **Properties → Compatibility** → tick **Force the use of a specific Steam Play compatibility tool** and choose **Proton Experimental** (or Proton 9.0 or newer).
4. Launch options are not needed. `SteamDeck=1 %command%` forces the Deck preset on other Linux PCs.

What the port does under Proton:

- The launcher detects Proton and the Deck (log and the launcher's hardware line say `Steam Deck` or `Wine / Proton`). On the Deck, **Auto-detect** picks a Deck preset: PS3 resolution with FSR 3.1 anti-aliasing, 30 fps, 4x anisotropic filtering.
- The frame rate list gets a **40** option. Set the Deck's screen to 40 Hz (Quick Access → Performance → Framerate limit / refresh rate) to match it. The window list gets **1280x800** (the Deck's screen).
- DLSS is not used under Proton; FSR 3.1 is used instead (XeSS should work through vkd3d-proton but is untested). The Direct3D 12 renderer runs through Proton's vkd3d-proton.
- In the launcher, the controller's **A** or **Start** button starts the game. In game, Steam Input presents the Deck's controls as an Xbox controller.
- The black cover window between the main menu and levels is off under Proton (the window closes and reopens). `RFOM_SEAMLESS=1` turns it on.
- The update check works the same way and can be turned off on the launcher's Advanced page.

## Status

- The campaign has been played up to Cheddar Gorge. Later levels haven't been tested yet, so problems are possible there.
- Crashes and freezes are rare. Crash and hang reports are written automatically (see *Crash and hang reports*), and the port can restart the game process after a freeze.
- The game is mostly CPU-bound and uses little GPU. CPU use and temperatures are far lower than when emulating the game.

## Known issues

- **Performance drops in heavy areas.** These come from the game's SPU jobs: they already run on three SPU threads at once, but many are chained (each waits for the previous), with the main thread waiting on the chain, so the cost is mostly latency rather than core count. The scheduler wakes only the thread that has work (`RFOM_SPU_WAKE`) and spreads SPU threads over free cores; `RFOM_SPU_WORKERS=auto` (experimental) adds virtual SPUs. A faster CPU helps most.
- **Audio crackle when the frame rate drops** in intensive spots, for example coming out of the crater at the start of York. Audio mixing is one of the SPU jobs. The audio buffer and time stretching (see [Audio](#audio)) are meant to cover these drops; check the `[audio]` log line if it still happens.
- **Falling through the floor** has been seen a few times and hasn't been reproduced yet. A safeguard tries to put you back in place before the fall kills you.
- **Shadows can flicker** occasionally.
- **FSR can ghost** on some things, such as the sky, at rare moments. DLSS doesn't show this.
- **Out-of-bounds water** (off the dock in Grimsby, under the bridge in Manchester) shows some stray triangles.
- **Rollers:** on RPCS3 they crashed the game unless OpenGL was used. Their level hasn't been reached on this port yet.
- **Ultrawide HUD:** the HUD is stretched with the view, not kept in a centred 16:9 area.
- **Frame Generation is experimental and barely tested** (DLSS on RTX 40/50, FSR 3 elsewhere; off by default). Expect ghosting and HUD or menu artifacts; reports are welcome.
- **Two programs:** on the PS3 disc the main menu and the levels are separate programs, so the port restarts the game process when you load into a level from the main menu (and back). A black cover window stays on screen during the switch, and the new window opens at the same place, so it looks like one window. `RFOM_SEAMLESS=0` turns the cover off (the window then closes and reopens). Log: `[exitspawn] cover window up`, `[exitspawn] cover released after N ms`.

If you hit a problem, describe how to reproduce it and send `logs\run.log` from that session (plus `logs\crash\` if there was a crash).

## Roadmap

What's planned after this beta, and how it relates to the issues above: [docs/ROADMAP.md](docs/ROADMAP.md).

## How it works

- `ps3recomp/` is a modified copy of [ps3recomp](https://github.com/sp00nznet/ps3recomp) by sp00nznet (MIT licence). It holds the runtime, the PS3 system libraries (graphics, audio, input, file system, SPU scheduling) and the lifters that translate PowerPC and SPU code into C/C++.
- `project/` is the game-specific part of the port: its hooks, the D3D12 renderer integration, mouse look, the PC menu, the launcher and the debug tools.
- `tools/rfom_regen.py` runs the lifters on your decrypted `EBOOT.elf` and `game.elf`, then re-applies this port's own edits to the generated code (`tools/hooks/`).

## Credits and licences

- Original Resistance: Fall of Man recomp: [Psyykosis](https://github.com/Psyykosis/Resistance-Fall-of-Man-Recomp). This fork adds DLSS / FSR 3.1, the PC menu, ultrawide, the performance work and the other PC features above.
- ps3recomp, by Ned Heller / sp00nznet: MIT.
- SoundTouch 2.4.1 by Olli Parviainen (audio time stretching), in `ps3recomp/third_party/soundtouch/`: LGPL-2.1 (`COPYING.TXT` there).
- at3_standalone, FFmpeg's ATRAC3 / ATRAC3plus decoders made standalone by the PPSSPP project (PPSSPP commit `ebc151f`), in `ps3recomp/third_party/at3_standalone/`: LGPL-2.1-or-later (`COPYING.LGPLv2.1` there). Like SoundTouch it is linked statically; this repository is the source you need to relink it with a modified version.
- AMD FidelityFX SDK (FSR 3.1), the DLLs in `tools/ffx_bin/`: MIT, © Advanced Micro Devices. FSR 3 frame generation (`amd_fidelityfx_framegeneration_dx12.dll`, SDK 2.3.0, MIT) is downloaded by Setup into the same folder.
- NVIDIA DLSS SDK v310.9.16, downloaded during setup into `tools/dlss/` only after you accept NVIDIA's licence: NVIDIA RTX SDK licence (`tools/dlss/LICENSE.txt`), not part of this repository. It is used only by the separate DLSS bridge DLL that build.bat builds on your PC. This port uses NVIDIA DLSS; NVIDIA, GeForce RTX and NVIDIA DLSS are trademarks of NVIDIA Corporation.
- NVIDIA Streamline SDK 2.14.16 (DLSS Frame Generation), downloaded during setup into `tools/streamline/` only after you accept NVIDIA's licences, and not part of this repository: the SDK and `sl.*.dll` are MIT (`tools/streamline/license.txt`, third-party notices in `tools/streamline/3rd-party-licenses.md`); `nvngx_dlssg.dll` (DLSS Frame Generation) is under the NVIDIA RTX SDK licence (`tools/streamline/bin/x64/nvngx_dlss.license.txt`) and Reflex under `reflex.license.txt`. `build.bat` builds the bridge `rfom_dlssg.dll` and copies the unmodified DLLs next to the game. NVIDIA DLSS Frame Generation and NVIDIA Reflex are trademarks of NVIDIA Corporation.
- NVIDIA NVAPI SDK (headers and `nvapi64.lib`, statically linked for Reflex), in `tools/nvapi/`: MIT licence (`tools/nvapi/License.txt`).
- Intel XeSS SDK 2.1.1 (runtime `libxess.dll` and headers), downloaded during setup into `tools/xess/` only after you accept Intel's licence, and not part of this repository: Intel Simplified Software License (`tools/xess/LICENSE.txt`, third-party notices in `tools/xess/third-party-programs.txt`). `build.bat` copies the unmodified `libxess.dll` next to the game; reverse engineering it is not allowed. Needs the Visual C++ 2015-2022 runtime (14.40 or newer).
- FFmpeg 7.1 (the BtbN `win64-lgpl-shared-7.1` build: unmodified `avcodec-61`, `avutil-59` and `swresample-5` DLLs), downloaded during setup into `tools/ffmpeg_bin/` for the game's videos and audio (the H.264, MPEG-2 video and AC-3 decoders): LGPL-3.0-or-later (`tools/ffmpeg_bin/LICENSE.txt`; `tools/ffmpeg_bin/SOURCE.txt` says where it came from and where its source is). It is loaded at run time and is not part of this repository. The FFmpeg headers in `ffmpeg/include/` are LGPL-2.1-or-later (see each file).
- RPCS3 (`key_vault.cpp`), downloaded during setup and used only to decrypt your own copy.
- The port's own code: MIT, see `LICENSE`.

## Support

This port is free and always will be: no paid builds, no early access and no perks. It is a fan project made in my spare time.
If you enjoy it and feel like buying me a coffee, it's genuinely appreciated and keeps the late-night debugging going ☕
[buymeacoffee.com/damiro98](https://buymeacoffee.com/damiro98)

A tip supports my own time and tools, such as [shaderscan](https://github.com/miroslavdanutalexandru-alt/shaderscan). It is not a payment for the game or this port, and nothing is unlocked by it. You still need your own copy of the game.
This port is built on Psyykosis' original recomp and on ps3recomp; they deserve the credit (and any coffee) just as much.

*Resistance: Fall of Man* is © Sony Interactive Entertainment, developed by Insomniac Games. This is an unofficial fan project, not affiliated with or endorsed by Sony or Insomniac. Please own the game you play.
