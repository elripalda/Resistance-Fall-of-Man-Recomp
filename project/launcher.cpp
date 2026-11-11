// Resistance: Fall of Man -- PC port launcher (v2).
//
// One window: a sidebar (Display / Graphics / Upscaling & Frame Gen / Performance / Controls / Audio /
// Interface & Notifications / Advanced / Achievements / Saves & tools / About + Play) and a
// page of settings drawn by hand with GDI+ (no stock controls, so it looks the
// same everywhere and scales with the Windows DPI setting).
//
// Everything is saved the moment it changes:
//   settings.ini  -- the launcher's own choices
//   settings.bat  -- the same choices as `set` lines; run.bat calls it
//   controls.txt  -- keyboard & mouse bindings, read by the game at start
// so run.bat keeps working with or without the launcher.
//
// Build (build.bat does this):
//   clang-cl /nologo /O2 /EHsc project\launcher.cpp /Fe:Launcher.exe /link /SUBSYSTEM:WINDOWS
//            user32.lib gdi32.lib shell32.lib gdiplus.lib dwmapi.lib

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <algorithm>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <string>
#include <vector>
#include <functional>
#include <shlobj.h>
#include <tlhelp32.h>
#include <knownfolders.h>
#include "rfom_autoconfig.h"
#include "rfom_achievements.h"

#ifdef _MSC_VER
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#endif

using namespace Gdiplus;

// ===========================================================================
// Settings (settings.ini / settings.bat)
// ===========================================================================
struct Choice { const char* label; const char* value; };
struct Setting {
    const char* key;      // settings.ini key
    const char* title;
    const char* help;     // shown in the help strip while hovered
    std::vector<Choice> choices;
    int def;
    int sel;
};

enum { S_DISPLAY, S_WINDOW, S_FPS, S_VSYNC, S_RES, S_FSR, S_UPSCALE, S_ANISO, S_FILTER, S_SHARPEN,
       S_MONITOR, S_FOCUS, S_MENUFPS, S_QUIT, S_SAVES, S_PADGLYPHS,
       S_ASPECT, S_PRESENT, S_OVERLAY, S_MODS, S_TEXPACK,
       S_SENSY, S_ADS, S_AIMHOLD, S_DEADZONE, S_CURVE,
       S_PADBACKEND, S_GYRO, S_GYROSENS, S_GYROINVY, S_GYROSPACE, S_GYROCAL, S_LIGHTBAR, S_NSWAP, S_COUNT };
static const int S_INPUT_FIRST = S_SENSY;   // the Controls page's options (reset with the controls)
static std::vector<Setting> g_set = {
    { "display", "Display mode",
      "Borderless fills the screen and lets you Alt+Tab instantly. Windowed uses the window size below. "
      "Alt+Enter also switches in game.",
      { {"Borderless fullscreen", "borderless"}, {"Windowed", "windowed"} }, 0 },
    { "window", "Window size",
      "The size of the game window in Windowed mode. Not used in Borderless fullscreen.",
      { {"1280 x 720", "1280x720"}, {"1600 x 900", "1600x900"}, {"1920 x 1080", "1920x1080"},
        {"2560 x 1440", "2560x1440"}, {"3840 x 2160", "3840x2160"} }, 2 },
    { "fps", "Frame rate limit",
      "The game runs at the same speed at any limit (each frame advances game time by the real time it "
      "took). 60 is the most tested; 120 and above are smooth but some scripted moments are less tested. "
      "The PS3 ran at 30.",
      { {"30  (PS3 original)", "30"}, {"55  (even on 165 Hz)", "55"}, {"60", "60"},
        {"72", "72"}, {"82  (even on 165 Hz)", "82"}, {"90", "90"}, {"120", "120"},
        {"144", "144"}, {"165", "165"}, {"Just under refresh  (G-Sync / FreeSync)", "vrr"}, {"Unlimited", "0"} }, 2 },
    { "vsync", "VSync",
      "Off is smoothest with a FreeSync / G-Sync monitor. 'Every 3rd refresh' gives an even 55 fps on a 165 Hz screen.",
      { {"Off", "0"}, {"Every refresh", "1"}, {"Every 2nd refresh", "2"}, {"Every 3rd refresh", "3"} }, 0 },
    { "res", "Render resolution",
      "The resolution the game draws at. Higher is sharper (shadows too) but costs GPU time. "
      "Above your screen's resolution it acts as supersampling.",
      { {"1280 x 720  (PS3 original)", "1"}, {"1920 x 1080", "1.5"}, {"2560 x 1440", "2"},
        {"3200 x 1800", "2.5"}, {"3840 x 2160", "3"} }, 2 },
    { "fsr", "Anti-aliasing",
      "AMD FSR 3.1 smooths jagged edges and shimmer, on any GPU. NVIDIA DLSS (DLAA at native resolution) "
      "does the same with NVIDIA's AI model on GeForce RTX cards - sharper and steadier. Intel XeSS is Intel's "
      "AI upscaler and runs on any GPU (fastest on Intel Arc). If DLSS or XeSS can't start "
      "the game uses FSR 3.1; if that fails too, plain scaling.",
      { {"Off  (PS3 original)", "0"}, {"AMD FSR 3.1", "1"}, {"NVIDIA DLSS / DLAA", "2"}, {"Intel XeSS", "3"} }, 0 },
    { "upscale", "Upscaling",
      "With FSR, DLSS or XeSS on: draw the 3D scene at a lower resolution and let the upscaler rebuild it at the "
      "render resolution. Faster, slightly softer. Native = no upscaling (DLAA / FSR / XeSS native AA). "
      "XeSS picks its matching preset (Ultra Quality at 67%, Balanced at 50%, ...). "
      "The menus and HUD always stay at full resolution.",
      { {"Native  (anti-aliasing only)", "1"}, {"Quality  (67%)", "0.667"}, {"Balanced  (58%)", "0.58"},
        {"Performance  (50%)", "0.5"}, {"Ultra Performance  (33%)", "0.333"} }, 0 },
    { "aniso", "Texture filtering",
      "Keeps floors and walls sharp when seen at an angle. 16x costs almost nothing on a modern GPU.",
      { {"Trilinear  (PS3 original)", "0"}, {"Anisotropic 2x", "2"}, {"Anisotropic 4x", "4"},
        {"Anisotropic 8x", "8"}, {"Anisotropic 16x", "16"} }, 4 },
    { "filter", "Scaling",
      "How the image is resized to fit your screen when the sizes differ. Smooth suits most people; "
      "Sharp pixels gives a crisp, blocky look.",
      { {"Smooth", "linear"}, {"Sharp pixels", "point"} }, 0 },
    { "sharpen", "Sharpening",
      "Counteracts the softness of scaling and anti-aliasing. Off when the render resolution matches your "
      "screen and anti-aliasing is off.",
      { {"Off", "0"}, {"Low", "0.25"}, {"Medium", "0.5"}, {"High", "0.8"} }, 1 },
    { "monitor", "Monitor",
      "The screen the game opens on (borderless fullscreen fills that screen). Monitors are numbered in "
      "Windows' order with the main display first.",
      { {"Main display", "0"}, {"Monitor 2", "2"}, {"Monitor 3", "3"}, {"Monitor 4", "4"} }, 0 },
    { "focus", "In the background",
      "What happens when you Alt+Tab away or click another window. Pausing also frees the CPU and GPU for "
      "the other program; the game picks up exactly where it was.",
      { {"Pause and mute", "pause"}, {"Mute only", "mute"}, {"Keep running", "run"} }, 0 },
    { "menufps", "Menu frame rate",
      "Caps the frame rate while a menu (pause, options) is up over the game. Above about 90 fps the pause "
      "menu's dimming layer can flicker.",
      { {"30", "30"}, {"60", "60"}, {"90", "90"}, {"Same as the game", "0"} }, 2 },
    { "quit", "Quit keys",
      "F10, holding Esc for 1 second, or holding Start / Menu for 1.5 seconds asks \"Quit to desktop?\" "
      "(Enter / A = yes, Esc / B = no). The game waits for a save in progress before it closes.",
      { {"On", "1"}, {"Off  (Alt+F4 only)", "0"} }, 0 },
    { "saves", "Save location",
      "Saved Games keeps your progress outside the port's folder ("
      "%USERPROFILE%\\Saved Games\\Resistance Fall of Man), so updating or reinstalling the port can't touch it. "
      "Saves already in the game folder are copied there on the next start.",
      { {"Saved Games folder", "savedgames"}, {"Game folder  (portable)", "portable"} }, 0 },
    { "padglyphs", "Controller button prompts",
      "Which button names the game shows while you play with a controller. Auto follows the controller in use "
      "(PlayStation symbols for a DualShock / DualSense, Xbox buttons otherwise). Keyboard and mouse always show your keys. "
      "Also in the in-game PC menu (F10).",
      { {"Auto", ""}, {"Xbox", "xbox"}, {"PlayStation", "ps"} }, 0 },
    { "aspect", "Aspect ratio",
      "Auto fills an ultrawide (21:9, 32:9) or 16:10 window: the 3D view gets wider (Hor+, the vertical view is "
      "kept) instead of black bars. Movies and 2D screens stay 16:9. The HUD is stretched with the view. "
      "On a 16:9 screen Auto changes nothing.",
      { {"Auto  (window shape)", "auto"}, {"16:9  (PS3 original)", "off"}, {"16:10", "16:10"}, {"21:9", "21:9"},
        {"32:9", "32:9"} }, 0 },
    { "present", "Tearing",
      "With VSync off: 'Allowed' presents immediately (lowest latency; with G-Sync / FreeSync the screen follows "
      "the frame rate). 'Never' drops late frames instead of tearing.",
      { {"Allowed  (lowest latency, VRR)", "tearing"}, {"Never", "notear"} }, 0 },
    { "overlay", "Performance overlay",
      "F3 shows the frame rate, frame time, draws per frame and the CPU time per draw in a corner of the game.",
      { {"Off  (F3 shows it)", ""}, {"On at start  (F3 hides it)", "1"}, {"Disabled  (F3 does nothing)", "0"} }, 0 },
    { "mods", "Mods folder",
      "Loads replacement game files from the 'mods' folder: mods\\<Mod name>\\USRDIR\\<game file>. Mods load in "
      "alphabetical order; a folder starting with _ or - is skipped. The log lists every file replaced.",
      { {"Off", "0"}, {"On", "1"} }, 0 },
    { "texpack", "Texture packs",
      "Load: replaces game textures with the files in mods\\textures (PNG or DDS, named by the hash the dump "
      "gives them, same size as the original). Dump: writes every texture the game uses to the texdump folder "
      "once, to make a pack from. Dumping slows loading.",
      { {"Off", "0"}, {"Load mods\\textures", "load"}, {"Dump to texdump", "dump"} }, 0 },
    { "sensy", "Vertical sensitivity",
      "Up/down mouse speed relative to left/right.",
      { {"50%", "0.5"}, {"75%", "0.75"}, {"Same as horizontal", "1"}, {"125%", "1.25"}, {"150%", "1.5"}, {"200%", "2"} }, 2 },
    { "ads", "Zoomed sensitivity",
      "Extra mouse speed multiplier while zoomed or scoped (on top of the automatic zoom matching).",
      { {"50%", "0.5"}, {"75%", "0.75"}, {"Same", "1"}, {"125%", "1.25"}, {"150%", "1.5"} }, 2 },
    { "aimhold", "Zoom",
      "Toggle: press to zoom, press again to stop (the game's own). Hold: zoomed while the zoom button / key is held.",
      { {"Toggle  (PS3 original)", "0"}, {"Hold", "1"} }, 0 },
    { "deadzone", "Stick deadzone",
      "How far a controller stick must move before it counts. Smaller is more responsive; larger hides "
      "stick drift. Default is 24%, applied per axis.",
      { {"Default  (24%, per axis)", ""}, {"5%", "0.05"}, {"10%", "0.1"}, {"15%", "0.15"}, {"20%", "0.2"},
        {"25%", "0.25"}, {"30%", "0.3"} }, 0 },
    { "curve", "Stick response curve",
      "How stick travel maps to speed. Linear is direct; Precise is slow near the centre for fine aim; "
      "Fast reaches full speed sooner.",
      { {"Linear  (default)", "1"}, {"Fast", "0.75"}, {"Precise", "1.5"}, {"Very precise", "2"} }, 0 },
    // SDL3 controllers, gyro aiming, light bar (ps3recomp/libs/input/cellPad.c, pad_sdl3.c)
    { "padbackend", "Controller support",
      "SDL3 reads DualShock 4, DualSense, Switch Pro / Joy-Con and Xbox controllers directly: correct button "
      "prompts, rumble, light bar, gyro aiming -- no DS4Windows or Steam Input needed. XInput is the old path "
      "(Xbox controllers and wrappers only); it is also used automatically if SDL3 cannot start.",
      { {"SDL3", ""}, {"XInput", "xinput"} }, 0 },
    { "gyro", "Gyro aiming",
      "Turn the view by turning the controller (DualShock 4, DualSense, Switch Pro, Joy-Con). While zoomed: only "
      "while zoomed in / aiming down the sights. Keep the controller still for a second after it connects "
      "(calibration).",
      { {"Off", ""}, {"On", "1"}, {"While zoomed", "aim"} }, 0 },
    { "gyrosens", "Gyro sensitivity",
      "How far the view turns for each degree the controller turns.",
      { {"Default  (2x)", ""}, {"0.5x", "0.5"}, {"1x", "1"}, {"1.5x", "1.5"}, {"3x", "3"}, {"4x", "4"}, {"6x", "6"} }, 0 },
    { "gyroinvy", "Gyro vertical",
      "Invert the up / down gyro.",
      { {"Normal", ""}, {"Inverted", "1"} }, 0 },
    { "gyrospace", "Gyro turning axis",
      "Controller: turning left / right follows the controller's own axis (yaw). Player: follows gravity, so it "
      "works the same with the controller flat or upright.",
      { {"Controller", ""}, {"Player", "player"} }, 0 },
    { "gyrocal", "Gyro calibration",
      "Learns the gyro's resting drift when a controller connects (keep it still for a second).",
      { {"On connect", ""}, {"Off", "0"} }, 0 },
    { "lightbar", "Light bar",
      "DualShock 4 / DualSense light bar in the player's colour (blue).",
      { {"Player colour", ""}, {"Off", "0"} }, 0 },
    { "nswap", "Nintendo button layout",
      "Switch Pro / Joy-Con: By position confirms with the bottom button (B) as on PlayStation; Nintendo style "
      "confirms with A and shows Xbox-style letters.",
      { {"By position", ""}, {"Nintendo style (A confirms)", "1"} }, 0 },
};

// ---- hardware detection (rfom_autoconfig.h) ---------------------------------
static rfac::Hardware g_hwinfo;
static rfac::Settings g_rec;
static bool g_first_run = false;
static void detect_hardware()
{
    rfac::detect(g_hwinfo);
    g_rec = rfac::recommend(g_hwinfo);
}
// the recommended value of a setting, or nullptr when it has none
static const char* rec_value(int si)
{
    switch (si) {
    case S_FPS: return g_rec.fps;
    case S_VSYNC: return g_rec.vsync;
    case S_RES: return g_rec.res;
    case S_FSR: return g_rec.fsr;
    case S_UPSCALE: return g_rec.upscale;
    case S_ANISO: return g_rec.aniso;
    case S_FILTER: return g_rec.filter;
    case S_SHARPEN: return g_rec.sharpen;
    default: return nullptr;
    }
}

static char g_dir[MAX_PATH];
static const int FOV_MIN = 60, FOV_MAX = 150, FOV_DEF = 82;   // 82 = the game's own view
static int g_fov = FOV_DEF;

static std::string path_of(const char* name) { return std::string(g_dir) + "\\" + name; }

static void load_settings()
{
    for (auto& s : g_set) s.sel = s.def;
    g_fov = FOV_DEF;
    FILE* f = fopen(path_of("settings.ini").c_str(), "r");
    if (!f) { g_first_run = true; return; }
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* v = eq + 1;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(line, "fov")) { int x = atoi(v); if (x >= FOV_MIN && x <= FOV_MAX) g_fov = x; }
        for (auto& s : g_set)
            if (!strcmp(s.key, line))
                for (size_t i = 0; i < s.choices.size(); i++)
                    if (!strcmp(s.choices[i].value, v)) s.sel = (int)i;
    }
    fclose(f);
}
static const char* val(int i) { return g_set[i].choices[g_set[i].sel].value; }

static void lx_write_bat(FILE* f);        // launcher_extra_r1.inc
static bool save_settings()
{
    FILE* f = fopen(path_of("settings.ini").c_str(), "w");
    if (!f) return false;
    for (auto& s : g_set) fprintf(f, "%s=%s\n", s.key, s.choices[s.sel].value);
    fprintf(f, "fov=%d\n", g_fov);
    fclose(f);
    f = fopen(path_of("settings.bat").c_str(), "w");
    if (!f) return false;
    fprintf(f, "@echo off\nrem Written by Launcher.exe -- change these in the launcher.\n");
    fprintf(f, "set \"RSX_DISPLAY=%s\"\n", val(S_DISPLAY));
    fprintf(f, "set \"RSX_WINDOW=%s\"\n", val(S_WINDOW));
    fprintf(f, "set \"RSX_RES_SCALE=%s\"\n", val(S_RES));
    fprintf(f, "set \"CHILD_RFOM_FPS=%s\"\n", val(S_FPS));
    fprintf(f, "set \"RSX_VSYNC=%s\"\n", val(S_VSYNC));
    fprintf(f, "set \"RSX_ANISO=%s\"\n", val(S_ANISO));
    fprintf(f, "set \"RSX_TEXPACK=%d\"\n", !strcmp(val(S_TEXPACK), "load") ? 1 : 0);
    fprintf(f, "set \"RSX_TEXDUMP=%d\"\n", !strcmp(val(S_TEXPACK), "dump") ? 1 : 0);
    fprintf(f, "set \"RSX_FILTER=%s\"\n", val(S_FILTER));
    fprintf(f, "set \"RSX_SHARPEN=%s\"\n", val(S_SHARPEN));
    fprintf(f, "set \"RSX_FSR=%s\"\n", val(S_FSR));
    fprintf(f, "set \"RSX_UPSCALE=%s\"\n", val(S_UPSCALE));
    fprintf(f, "set \"RFOM_FOV=%d\"\n", g_fov);
    fprintf(f, "set \"RSX_MONITOR=%s\"\n", val(S_MONITOR));
    fprintf(f, "set \"RSX_FOCUS_PAUSE=%d\"\n", !strcmp(val(S_FOCUS), "pause") ? 1 : 0);
    fprintf(f, "set \"RSX_FOCUS_MUTE=%d\"\n", !strcmp(val(S_FOCUS), "run") ? 0 : 1);
    fprintf(f, "set \"RFOM_MENU_FPS=%s\"\n", val(S_MENUFPS));
    fprintf(f, "set \"RFOM_QUIT_PROMPT=%s\"\n", val(S_QUIT));
    fprintf(f, "set \"RFOM_PAD_GLYPHS=%s\"\n", val(S_PADGLYPHS));
    fprintf(f, "set \"RFOM_ASPECT=%s\"\n", val(S_ASPECT));
    fprintf(f, "set \"RFOM_PRESENT=%s\"\n", val(S_PRESENT));
    fprintf(f, "set \"RFOM_OVERLAY=%s\"\n", val(S_OVERLAY));
    fprintf(f, "set \"RFOM_MODS=%s\"\n", val(S_MODS));
    fprintf(f, "set \"PAD_MOUSE_SENS_Y=%s\"\n", val(S_SENSY));
    fprintf(f, "set \"RFOM_ADS_SENS=%s\"\n", val(S_ADS));
    fprintf(f, "set \"PAD_AIM_HOLD=%s\"\n", val(S_AIMHOLD));
    fprintf(f, "set \"PAD_DEADZONE=%s\"\n", val(S_DEADZONE));
    fprintf(f, "set \"PAD_CURVE=%s\"\n", !strcmp(val(S_CURVE), "1") ? "" : val(S_CURVE));
    fprintf(f, "set \"PAD_BACKEND=%s\"\n", val(S_PADBACKEND));
    fprintf(f, "set \"PAD_GYRO=%s\"\n", val(S_GYRO));
    fprintf(f, "set \"PAD_GYRO_SENS=%s\"\n", val(S_GYROSENS));
    fprintf(f, "set \"PAD_GYRO_INVERT_Y=%s\"\n", val(S_GYROINVY));
    fprintf(f, "set \"PAD_GYRO_SPACE=%s\"\n", val(S_GYROSPACE));
    fprintf(f, "set \"PAD_GYRO_CALIBRATE=%s\"\n", val(S_GYROCAL));
    fprintf(f, "set \"PAD_LIGHTBAR=%s\"\n", val(S_LIGHTBAR));
    fprintf(f, "set \"PAD_NINTENDO_SWAP=%s\"\n", val(S_NSWAP));
    lx_write_bat(f);
    if (!strcmp(val(S_SAVES), "portable")) fprintf(f, "set \"RFOM_SAVE_DIR=portable\"\n");
    else fprintf(f, "set \"RFOM_SAVE_DIR=\"\n");
    fclose(f);
    return true;
}

// ---- update check, Wine / Steam Deck extras (launcher_dist.inc) ----
#define DIST_REPO "miroslavdanutalexandru-alt/Resistance-Fall-of-Man-Recomp"
#define DIST_USER_DIR "Resistance Fall of Man"   // settings backup: Saved Games\<this>\config
#include "launcher_dist.inc"
#include "port_report.cpp"                  // "Report a problem" zip + "the game crashed last time" prompt

// ===========================================================================
// Keyboard & mouse controls (controls.txt, read by libs/input/cellPad.c)
// ===========================================================================
struct Act { const char* name; const char* pad; const char* group; const char* help; bool hidden; };
static const Act g_acts[] = {
    { "Move forward",        "LS_UP",      "Movement", "", false },
    { "Move back",           "LS_DOWN",    "Movement", "", false },
    { "Move left",           "LS_LEFT",    "Movement", "", false },
    { "Move right",          "LS_RIGHT",   "Movement", "", false },
    { "Jump",                "CROSS",      "Movement", "Also confirms in menus.", false },
    { "Crouch",              "L2",         "Movement", "", false },
    { "Fire",                "R1",         "Combat", "", false },
    { "Alternate fire",      "L1",         "Combat", "Each weapon's secondary mode (Bullseye tag, Auger shield...).", false },
    { "Zoom / aim",          "R3",         "Combat", "Toggles the weapon's zoom.", false },
    { "Reload",              "SQUARE",     "Combat", "", false },
    { "Melee",               "TRIANGLE",   "Combat", "Melee attack. Using switches and doors is a separate key below.", false },
    { "Weapon wheel",        "R2",         "Combat", "Hold to pick a weapon.", false },
    { "Throw grenade",       "CIRCLE",     "Combat", "", false },
    { "Next grenade type",   "DPAD_UP",    "Combat", "", false },
    { "Previous grenade type","DPAD_DOWN",  "Combat", "", false },
    { "Use / interact",      "INTERACT",   "Actions", "Switches, doors, ladders and vehicles. Separate from Melee so a stray "
                                                       "melee never misses a switch.", false },
    { "Find partner",        "DPAD_LEFT",  "Actions", "", false },
    { "Spotlight",           "DPAD_RIGHT", "Actions", "", false },
    { "Pause menu",          "START",      "Menus", "", false },
    { "Journal",             "SELECT",     "Menus", "Intel, objectives and the map.", false },
    { "Voice chat",          "L3",         "Menus", "", true },     // online only: kept in the file, not shown
    { "Look up",             "RS_UP",      "Look with keys", "Optional: turn the camera with keys as well as the mouse.", false },
    { "Look down",           "RS_DOWN",    "Look with keys", "Optional: turn the camera with keys as well as the mouse.", false },
    { "Look left",           "RS_LEFT",    "Look with keys", "Optional: turn the camera with keys as well as the mouse.", false },
    { "Look right",          "RS_RIGHT",   "Look with keys", "Optional: turn the camera with keys as well as the mouse.", false },
};
static const int NACT = sizeof g_acts / sizeof g_acts[0];
static const int NSLOT = 3;            // the file may hold 3; the page shows 2 (a 3rd is kept)
static const int NSHOW = 2;
static std::string g_bind[NACT][NSLOT];
static double g_msens = 1.0; static int g_minv = 0, g_mdirect = 1;
static std::string g_mrelease = "F8";
static double g_mdeg = 0.05, g_mdz = 0.28, g_msmooth = 0.03;
static const char* g_padint_opts[][2] = {
    {"NONE","Same as Melee  (as on PS3)"},{"SQUARE","Square"},{"CIRCLE","Circle"},{"CROSS","Cross"},
    {"TRIANGLE","Triangle"},{"L1","L1"},{"R1","R1"},{"L2","L2"},{"R2","R2"},{"L3","L3  (left stick click)"},
    {"R3","R3  (right stick click)"},{"DPAD_UP","D-pad up"},{"DPAD_DOWN","D-pad down"},{"DPAD_LEFT","D-pad left"},
    {"DPAD_RIGHT","D-pad right"},
};
static const int NPADINT = sizeof g_padint_opts / sizeof g_padint_opts[0];
static int g_padint = 0;

// the defaults that ship in controls.txt
static const char* g_default_binds[][2] = {
    {"W","LS_UP"},{"S","LS_DOWN"},{"A","LS_LEFT"},{"D","LS_RIGHT"},
    {"SPACE","CROSS"},{"SHIFT","L2"},{"C","L2"},
    {"MOUSE1","R1"},{"MOUSE3","L1"},{"MOUSE2","R3"},{"R","SQUARE"},{"MOUSE4","SQUARE"},{"MOUSE5","TRIANGLE"},
    {"E","R2"},{"G","CIRCLE"},{"WHEELUP","DPAD_UP"},{"WHEELDOWN","DPAD_DOWN"},
    {"F","INTERACT"},{"1","DPAD_LEFT"},{"T","DPAD_RIGHT"},
    {"ESC","START"},{"TAB","SELECT"},
    {"UP","RS_UP"},{"DOWN","RS_DOWN"},{"LEFT","RS_LEFT"},{"RIGHT","RS_RIGHT"},
};

static int act_of(const char* pad)
{
    for (int i = 0; i < NACT; i++) if (!_stricmp(g_acts[i].pad, pad)) return i;
    return -1;
}
static void bind_add(int a, const char* key)
{
    for (int k = 0; k < NSLOT; k++) if (!_stricmp(g_bind[a][k].c_str(), key)) return;
    for (int k = 0; k < NSLOT; k++) if (g_bind[a][k].empty()) { g_bind[a][k] = key; return; }
}
// keys fill the boxes from the left (a cleared first key lets the second move up)
static void ctl_compact()
{
    for (int a = 0; a < NACT; a++) {
        std::string keep[NSLOT]; int n = 0;
        for (int k = 0; k < NSLOT; k++) if (!g_bind[a][k].empty()) keep[n++] = g_bind[a][k];
        for (int k = 0; k < NSLOT; k++) g_bind[a][k] = k < n ? keep[k] : std::string();
    }
}
static void ctl_defaults()
{
    for (auto& r : g_bind) for (auto& k : r) k.clear();
    for (auto& d : g_default_binds) { int a = act_of(d[1]); if (a >= 0) bind_add(a, d[0]); }
    g_msens = 1.0; g_minv = 0; g_mdirect = 1; g_mrelease = "F8"; g_mdeg = 0.05; g_mdz = 0.28; g_msmooth = 0.03;
    g_padint = 0;
}
static void ctl_load()
{
    FILE* f = fopen(path_of("controls.txt").c_str(), "r");
    if (!f) { ctl_defaults(); return; }
    for (auto& r : g_bind) for (auto& k : r) k.clear();
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (char* h = strchr(line, '#')) *h = 0;
        char* eq = strchr(line, '='); if (!eq) continue;
        *eq = 0;
        char k[64] = "", v[64] = "";
        if (sscanf(line, " %63s", k) != 1 || sscanf(eq + 1, " %63s", v) != 1) continue;
        if (!_stricmp(k, "mouse_sensitivity")) { g_msens = atof(v); continue; }
        if (!_stricmp(k, "mouse_invert_y")) { g_minv = atoi(v); continue; }
        if (!_stricmp(k, "mouse_look")) { g_mdirect = _stricmp(v, "stick") ? 1 : 0; continue; }
        if (!_stricmp(k, "mouse_release_key")) { g_mrelease = v; continue; }
        if (!_stricmp(k, "mouse_degrees_per_count")) { g_mdeg = atof(v); continue; }
        if (!_stricmp(k, "mouse_deadzone")) { g_mdz = atof(v); continue; }
        if (!_stricmp(k, "mouse_smoothing")) { g_msmooth = atof(v); continue; }
        if (!_stricmp(k, "pad_interact")) {
            g_padint = 0;
            for (int i = 0; i < NPADINT; i++) if (!_stricmp(v, g_padint_opts[i][0])) g_padint = i;
            continue; }
        const int a = act_of(v);
        if (a >= 0) { for (char* p = k; *p; p++) *p = (char)toupper((unsigned char)*p); bind_add(a, k); }
    }
    fclose(f);
    if (g_msens < 0.1) g_msens = 0.1; if (g_msens > 4.0) g_msens = 4.0;
    // v153: older files had one "Action / melee" (TRIANGLE): those keys interact now, V melees
    const int am = act_of("TRIANGLE"), ai = act_of("INTERACT");
    bool any_i = false, any_m = false, v_used = false;
    for (int k = 0; k < NSLOT; k++) { any_i |= !g_bind[ai][k].empty(); any_m |= !g_bind[am][k].empty(); }
    for (int a = 0; a < NACT; a++) for (int k = 0; k < NSLOT; k++) v_used |= !_stricmp(g_bind[a][k].c_str(), "V");
    if (!any_i && any_m) {
        for (int k = 0; k < NSLOT; k++) { g_bind[ai][k] = g_bind[am][k]; g_bind[am][k].clear(); }
        if (!v_used) bind_add(am, "V");
    }
}
static bool ctl_save()
{
    ctl_compact();
    FILE* f = fopen(path_of("controls.txt").c_str(), "w");
    if (!f) return false;
    fprintf(f, "# Resistance PC port -- keyboard & mouse controls.\n"
               "# Edit these in Launcher.exe -> Controls, or by hand: KEY = ACTION, one per line.\n"
               "# KEYS:    A-Z 0-9 F1-F12 SPACE SHIFT CTRL ALT TAB ENTER ESC BACKSPACE UP DOWN LEFT RIGHT\n"
               "#          CAPSLOCK INSERT DELETE HOME END PGUP PGDN NUM0-NUM9\n"
               "#          GRAVE MINUS EQUALS LBRACKET RBRACKET BACKSLASH SEMICOLON APOSTROPHE COMMA PERIOD SLASH\n"
               "#          MOUSE1 (left) MOUSE2 (right) MOUSE3 (middle) MOUSE4 MOUSE5 WHEELUP WHEELDOWN\n"
               "# ACTIONS: LS_UP LS_DOWN LS_LEFT LS_RIGHT (move)  RS_UP RS_DOWN RS_LEFT RS_RIGHT (look)\n"
               "#          CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 L3 R3 START SELECT\n"
               "#          DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT\n"
               "#          INTERACT (use switches/doors/vehicles; TRIANGLE then only melees)\n"
               "# A key may be listed more than once to press several things.\n\n");
    for (int a = 0; a < NACT; a++)
        for (int k = 0; k < NSLOT; k++)
            if (!g_bind[a][k].empty())
                fprintf(f, "%-10s = %-10s # %s\n", g_bind[a][k].c_str(), g_acts[a].pad, g_acts[a].name);
    fprintf(f, "\n# Mouse\n");
    fprintf(f, "mouse_sensitivity = %.2f\n", g_msens);
    fprintf(f, "mouse_invert_y = %d\n", g_minv);
    fprintf(f, "mouse_look = %s        # direct: the mouse turns the camera 1:1 on foot; stick: old stick emulation\n", g_mdirect ? "direct" : "stick");
    fprintf(f, "mouse_degrees_per_count = %.3f   # direct look speed (x mouse_sensitivity, x the game's Look Sensitivity)\n", g_mdeg);
    fprintf(f, "mouse_deadzone = %.2f      # stick emulation only\n", g_mdz);
    fprintf(f, "mouse_smoothing = %.2f     # stick emulation only, seconds\n", g_msmooth);
    fprintf(f, "mouse_release_key = %s     # free / recapture the mouse cursor\n", g_mrelease.c_str());
    fprintf(f, "\n# Controller\n");
    fprintf(f, "pad_interact = %s     # button that uses things (the game's Action button then only melees); NONE = as on PS3\n",
            g_padint_opts[g_padint][0]);
    fclose(f);
    return true;
}

// key name for a captured input (names the game's controls.txt reader knows)
static const char* vk_name(int vk)
{
    static char one[2];
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { one[0] = (char)vk; one[1] = 0; return one; }
    if (vk >= VK_F1 && vk <= VK_F12) { static char fn[4]; snprintf(fn, sizeof fn, "F%d", vk - VK_F1 + 1); return fn; }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { static char nn[5]; snprintf(nn, sizeof nn, "NUM%d", vk - VK_NUMPAD0); return nn; }
    switch (vk) {
    case VK_SPACE: return "SPACE"; case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "SHIFT";
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "CTRL"; case VK_MENU: case VK_LMENU: case VK_RMENU: return "ALT";
    case VK_TAB: return "TAB"; case VK_RETURN: return "ENTER"; case VK_ESCAPE: return "ESC"; case VK_BACK: return "BACKSPACE";
    case VK_UP: return "UP"; case VK_DOWN: return "DOWN"; case VK_LEFT: return "LEFT"; case VK_RIGHT: return "RIGHT";
    case VK_CAPITAL: return "CAPSLOCK"; case VK_INSERT: return "INSERT"; case VK_DELETE: return "DELETE";
    case VK_HOME: return "HOME"; case VK_END: return "END"; case VK_PRIOR: return "PGUP"; case VK_NEXT: return "PGDN";
    case VK_OEM_3: return "GRAVE"; case VK_OEM_MINUS: return "MINUS"; case VK_OEM_PLUS: return "EQUALS";
    case VK_OEM_4: return "LBRACKET"; case VK_OEM_6: return "RBRACKET"; case VK_OEM_5: return "BACKSLASH";
    case VK_OEM_1: return "SEMICOLON"; case VK_OEM_7: return "APOSTROPHE"; case VK_OEM_COMMA: return "COMMA";
    case VK_OEM_PERIOD: return "PERIOD"; case VK_OEM_2: return "SLASH";
    }
    return NULL;
}
static std::string pretty(const std::string& k)
{
    static const struct { const char* n; const char* p; } t[] = {
        {"MOUSE1","Left mouse"},{"MOUSE2","Right mouse"},{"MOUSE3","Middle mouse"},{"MOUSE4","Mouse 4"},{"MOUSE5","Mouse 5"},
        {"WHEELUP","Wheel up"},{"WHEELDOWN","Wheel down"},{"SPACE","Space"},{"SHIFT","Shift"},{"CTRL","Ctrl"},{"ALT","Alt"},
        {"TAB","Tab"},{"ENTER","Enter"},{"ESC","Esc"},{"BACKSPACE","Backspace"},{"UP","Up arrow"},{"DOWN","Down arrow"},
        {"LEFT","Left arrow"},{"RIGHT","Right arrow"},{"CAPSLOCK","Caps Lock"},{"INSERT","Insert"},{"DELETE","Delete"},
        {"HOME","Home"},{"END","End"},{"PGUP","Page Up"},{"PGDN","Page Down"},
        {"GRAVE","`"},{"MINUS","-"},{"EQUALS","="},
        {"LBRACKET","["},{"RBRACKET","]"},{"BACKSLASH","\\"},{"SEMICOLON",";"},{"APOSTROPHE","'"},{"COMMA",","},{"PERIOD","."},{"SLASH","/"},
    };
    for (auto& e : t) if (!_stricmp(k.c_str(), e.n)) return e.p;
    if (k.size() == 4 && !_strnicmp(k.c_str(), "NUM", 3)) return std::string("Numpad ") + k[3];
    return k;
}

// ===========================================================================
// Drawing: palette, DPI, fonts
// ===========================================================================
static const Color C_SIDEBAR(255, 17, 18, 20), C_BG(255, 24, 25, 28), C_PANEL(255, 31, 33, 37),
    C_CTL(255, 40, 43, 48), C_CTL_HOT(255, 52, 56, 62), C_BORDER(255, 58, 62, 69),
    C_TEXT(255, 236, 236, 236), C_DIM(255, 150, 156, 164), C_FAINT(255, 104, 110, 118),
    C_ACCENT(255, 222, 130, 44), C_ACCENT_HOT(255, 240, 150, 62), C_ACCENT_DIM(255, 96, 62, 30),
    C_ON_ACCENT(255, 24, 18, 12), C_WARN(255, 236, 190, 90);

static double g_s = 1.0;                 // DPI scale
static int S(double v) { return (int)lround(v * g_s); }
static float SF(double v) { return (float)(v * g_s); }

static ULONG_PTR g_gdip;
static Font *f_title, *f_sub, *f_tiny, *f_h1, *f_h2, *f_body, *f_small, *f_btn, *f_play, *f_nav;
static std::wstring g_fam_ui = L"Segoe UI", g_fam_ui_semi = L"Segoe UI Semibold", g_fam_title = L"Segoe UI Semibold";

static bool family_ok(const wchar_t* n) { FontFamily ff(n); return ff.IsAvailable() != FALSE; }
static Font* mkfont(const std::wstring& fam, double px, int style)
{
    FontFamily ff(fam.c_str());
    if (!ff.IsAvailable()) { FontFamily gf(L"Tahoma"); return new Font(&gf, SF(px), style, UnitPixel); }
    return new Font(&ff, SF(px), style, UnitPixel);
}
static void make_fonts()
{
    Font* all[] = { f_title, f_sub, f_tiny, f_h1, f_h2, f_body, f_small, f_btn, f_play, f_nav };
    for (Font* f : all) delete f;
    if (!family_ok(L"Segoe UI")) { g_fam_ui = L"Tahoma"; }
    if (family_ok(L"Segoe UI Semibold")) g_fam_ui_semi = L"Segoe UI Semibold"; else g_fam_ui_semi = g_fam_ui;
    if (family_ok(L"Bahnschrift SemiBold Condensed")) g_fam_title = L"Bahnschrift SemiBold Condensed";
    else if (family_ok(L"Bahnschrift")) g_fam_title = L"Bahnschrift";
    else g_fam_title = g_fam_ui_semi;
    const bool semi = g_fam_ui_semi != g_fam_ui;
    f_title = mkfont(g_fam_title, 34, FontStyleBold * (g_fam_title == g_fam_ui));
    f_sub   = mkfont(g_fam_title, 15, FontStyleRegular);
    f_tiny  = mkfont(g_fam_ui, 11, FontStyleRegular);
    f_h1    = mkfont(g_fam_ui_semi, 24, semi ? FontStyleRegular : FontStyleBold);
    f_h2    = mkfont(g_fam_ui_semi, 13, semi ? FontStyleRegular : FontStyleBold);
    f_body  = mkfont(g_fam_ui, 14.5, FontStyleRegular);
    f_small = mkfont(g_fam_ui, 12.5, FontStyleRegular);
    f_btn   = mkfont(g_fam_ui_semi, 14, semi ? FontStyleRegular : FontStyleBold);
    f_play  = mkfont(g_fam_title, 24, FontStyleRegular);
    f_nav   = mkfont(g_fam_ui, 15, FontStyleRegular);
}

static void round_rect(GraphicsPath& p, RectF r, float rad)
{
    const float d = rad * 2;
    if (rad <= 0.5f) { p.AddRectangle(r); return; }
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    p.CloseFigure();
}
static void fill_rr(Graphics& g, RectF r, float rad, const Color& c)
{
    GraphicsPath p; round_rect(p, r, rad); SolidBrush b(c); g.FillPath(&b, &p);
}
static void stroke_rr(Graphics& g, RectF r, float rad, const Color& c, float w)
{
    GraphicsPath p; round_rect(p, r, rad); Pen pen(c, w); g.DrawPath(&pen, &p);
}
static std::wstring W(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w;
}
enum { AL_L = 0, AL_C = 1, AL_R = 2 };
static void text(Graphics& g, const std::wstring& t, Font* f, const Color& c, RectF r, int h = AL_L, bool vcenter = true,
                 bool wrap = false)
{
    StringFormat sf;
    sf.SetAlignment(h == AL_C ? StringAlignmentCenter : h == AL_R ? StringAlignmentFar : StringAlignmentNear);
    sf.SetLineAlignment(vcenter ? StringAlignmentCenter : StringAlignmentNear);
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    if (!wrap) sf.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush b(c);
    g.DrawString(t.c_str(), -1, f, r, &sf, &b);
}
static float text_h(Graphics& g, const std::wstring& t, Font* f, float w)
{
    RectF out; g.MeasureString(t.c_str(), -1, f, RectF(0, 0, w, 10000), &out);
    return out.Height;
}

// ===========================================================================
// Widgets: rebuilt by layout() for the current page, then painted / hit-tested
// ===========================================================================
enum WType { W_NAV, W_PLAY, W_BUTTON, W_DROP, W_SLIDER, W_TOGGLE, W_KEY, W_KEYX, W_ROW };
struct Widget {
    WType t; RectF r; int id = 0, sub = 0;
    bool scroll = true;          // lives in the scrolling content area
    std::string title, help;     // for the help strip
    bool disabled = false;
};
enum Page { P_DISPLAY, P_GRAPHICS, P_UPSCALE, P_PERF, P_CONTROLS, P_AUDIO, P_UI, P_ADVANCED, P_TROPHIES, P_TOOLS, P_ABOUT,
            P_COUNT };   // "Launcher.exe --page N" numbers (README)
static const char* g_page_name[P_COUNT] = { "Display", "Graphics", "Upscaling & Frame Gen", "Performance", "Controls",
                                            "Audio", "Interface & Notifications", "Advanced", "Achievements",
                                            "Saves & tools", "About" };
static float g_nav_end;   // bottom of the sidebar items (the art starts below them)
static int g_page = P_DISPLAY;
static std::vector<Widget> g_w;
static float g_scroll[P_COUNT], g_content_h = 0;
static int g_hot = -1, g_press = -1;
static RectF g_view;                     // content viewport (client coordinates)
static HWND g_hw;

// dropdown overlay
enum { DD_SETTING = 1, DD_PADINT, DD_MLOOK };
static struct { bool open = false; int kind = 0, idx = 0, hot = -1; RectF anchor, list; float item_h = 0; } g_dd;
// sliders
enum { SL_FOV = 1, SL_SENS };
static int g_drag = 0;                   // slider being dragged
// toggles
enum { TG_INVERT = 1 };
// buttons
enum { BT_RESET_CTL = 1, BT_RESET_GFX, BT_FOLDER, BT_LOGS, BT_WEB, BT_CONTROLS_TXT, BT_AUTO, BT_SAVES, BT_SHOTS, BT_REBUILD_SHADERS, BT_REPORT, BT_RESET_ALL };
// key capture
static int g_cap_a = -1, g_cap_k = -1;   // action/slot being captured (-2 = mouse release key)
static std::wstring g_toast; static DWORD g_toast_until = 0;

static void toast(const std::string& s) { g_toast = W(s); g_toast_until = GetTickCount() + 5000; SetTimer(g_hw, 1, 5100, NULL); }

// ---- offline shader scan state (see the shaderscan block above click()) ----
enum { WM_SCAN_PROGRESS = WM_APP + 0x53, WM_SCAN_DONE };
static SRWLOCK g_scan_lock = SRWLOCK_INIT;
static struct {
    bool active = false, checked = false, quit = false;
    HANDLE thread = NULL, proc = NULL;    // proc: guarded by g_scan_lock
    bool cancel = false;                  // guarded by g_scan_lock
    bool ran = false;                     // the scan itself started (not only --check); guarded
    std::string phase, error;             // guarded
    unsigned done = 0, total = 0;         // guarded
    int exit_code = 0;
    DWORD t0 = 0;
    RectF skip_btn;
} g_scan;

static int dd_count(int kind, int idx)
{
    if (kind == DD_SETTING) return (int)g_set[idx].choices.size();
    if (kind == DD_PADINT) return NPADINT;
    return 2;
}
static std::string dd_label(int kind, int idx, int i)
{
    if (kind == DD_SETTING) return g_set[idx].choices[i].label;
    if (kind == DD_PADINT) return g_padint_opts[i][1];
    return i == 0 ? "Direct  (recommended)" : "Classic  (stick emulation)";
}
// the choice the hardware detection recommends gets a "recommended" tag in the box and in the list
static bool dd_recommended(int kind, int idx, int i)
{
    if (kind != DD_SETTING) return false;
    const char* r = rec_value(idx);
    return r && !strcmp(r, g_set[idx].choices[i].value);
}
static int dd_sel(int kind, int idx)
{
    if (kind == DD_SETTING) return g_set[idx].sel;
    if (kind == DD_PADINT) return g_padint;
    return g_mdirect ? 0 : 1;
}
static void dd_set(int kind, int idx, int i)
{
    if (kind == DD_SETTING) { g_set[idx].sel = i; save_settings(); }
    else if (kind == DD_PADINT) { g_padint = i; ctl_save(); }
    else { g_mdirect = i == 0; ctl_save(); }
}

// ---- layout ---------------------------------------------------------------
static const float SIDEBAR = 236, PAD = 40, FOOT = 76, ROWH = 52, CTLW = 340;

static Widget& add(WType t, RectF r, int id = 0, int sub = 0, bool scroll = true)
{
    g_w.push_back(Widget()); Widget& w = g_w.back(); w.t = t; w.r = r; w.id = id; w.sub = sub; w.scroll = scroll; return w;
}
struct Lay { float x, y, w; };          // content coordinates (unscrolled), unscaled-logical * s


struct Deco { int kind; RectF r; std::string s; };      // labels and headings (painted, not hit)
static std::vector<Deco> g_deco;
enum { D_H1, D_INTRO, D_H2, D_LABEL, D_LABEL_DIM, D_GROUP, D_NOTE, D_ABOUT_TITLE, D_ABOUT_TEXT, D_KEYCOL, D_PANEL, D_REC, D_TROPHY };

static void deco(int kind, RectF r, const std::string& s) { g_deco.push_back({ kind, r, s }); }

static void layout_setting_row(float x, float& y, float w, int si)
{
    Setting& s = g_set[si];
    const bool dis = (si == S_WINDOW && g_set[S_DISPLAY].sel == 0);
    Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), si);
    row.title = s.title; row.help = s.help; row.disabled = dis;
    deco(dis ? D_LABEL_DIM : D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), s.title);
    Widget& d = add(W_DROP, RectF(x + w - SF(CTLW), y + SF(9), SF(CTLW), SF(ROWH - 18)), DD_SETTING, si);
    d.title = s.title; d.help = s.help; d.disabled = dis;
    y += SF(ROWH);
}

static bool differs_from_recommended()
{
    for (int si = 0; si < S_COUNT; si++) {
        const char* r = rec_value(si);
        if (r && strcmp(r, g_set[si].choices[g_set[si].sel].value)) return true;
    }
    return false;
}
// "Recommended for your PC": what was detected, what it recommends, and the button that applies it
static void layout_recommend_panel(float x, float& y, float w)
{
    char hw[300], st[200];
    rfac::describe_hw(g_hwinfo, hw, sizeof hw);
    rfac::describe(g_rec, st, sizeof st);
    const float top = y;
    deco(D_PANEL, RectF(x - SF(12), top, w + SF(24), SF(150)), "");
    y += SF(12);
    deco(D_H2, RectF(x, y, w, SF(22)), "RECOMMENDED FOR YOUR PC"); y += SF(26);
    deco(D_REC, RectF(x, y, w, SF(24)), st); y += SF(26);
    deco(D_NOTE, RectF(x, y, w, SF(36)), std::string("This PC: ") + hw); y += SF(40);
    Widget& b = add(W_BUTTON, RectF(x, y, SF(250), SF(36)), BT_AUTO);
    b.title = "Auto-detect (recommended)";
    b.help = "Sets resolution, anti-aliasing / upscaling, filtering and frame rate for this PC (also the first-run "
             "defaults). RTX 4070 / 5070 class: the scene at 1080p, DLSS Performance to 4K (scaled to your screen). "
             "Controls are not changed.";
    deco(D_LABEL_DIM, RectF(x + SF(266), y, w - SF(266), SF(36)),
         differs_from_recommended() ? "Your settings differ from the recommendation." : "Using the recommended settings.");
    y = top + SF(150) + SF(22);
}
// ---- Achievements page (launcher_trophies.inc): unofficial, fan-made, computed read-only from PROFILE.SAV
#define LT_R1 1
#include "launcher_trophies.inc"
#include "launcher_extra_r1.inc"          // PC extras: options, Saves & tools page, benchmark

#include "launcher_cats.inc"              // which options each page lists

// the launcher_cats.inc "@name" rows: controls that are not g_set options
static void layout_special(int id, float x, float& y, float w)
{
    if (id == CAT_FOV) {
        Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), -1);
        row.title = "Field of view";
        row.help = "Horizontal field of view in degrees. 82\xC2\xB0 matches the PS3; 90-110\xC2\xB0 is typical on PC. "
                   "Zooming and scopes keep their own zoom.";
        deco(D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), "Field of view");
        Widget& sl = add(W_SLIDER, RectF(x + w - SF(CTLW), y + SF(12), SF(CTLW), SF(ROWH - 24)), SL_FOV);
        sl.title = row.title; sl.help = row.help;
        y += SF(ROWH);
    } else if (id == CAT_MOUSE) {
        const float kw = SF(150);
        {
            Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), -2);
            row.title = "Mouse sensitivity";
            row.help = "How fast the camera turns. It is also multiplied by the game's own Look Sensitivity option "
                       "(Options \xE2\x80\xBA Controls; the middle setting = 1x).";
            deco(D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), "Sensitivity");
            Widget& sl = add(W_SLIDER, RectF(x + w - SF(CTLW), y + SF(12), SF(CTLW), SF(ROWH - 24)), SL_SENS);
            sl.title = row.title; sl.help = row.help; y += SF(ROWH);
        }
        {
            Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), -3);
            row.title = "Invert vertical look";
            row.help = "Moving the mouse forward looks down. Works on top of the game's own Invert option.";
            deco(D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), "Invert vertical look");
            Widget& tg = add(W_TOGGLE, RectF(x + w - SF(48), y + SF(14), SF(48), SF(24)), TG_INVERT);
            tg.title = row.title; tg.help = row.help; y += SF(ROWH);
        }
        {
            Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), -4);
            row.title = "Mouse aiming";
            row.help = "Direct: the mouse turns the camera 1:1, like a PC shooter. Classic: the mouse pretends to "
                       "be the right stick, with the PS3's acceleration.";
            deco(D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), "Mouse aiming");
            Widget& d = add(W_DROP, RectF(x + w - SF(CTLW), y + SF(9), SF(CTLW), SF(ROWH - 18)), DD_MLOOK);
            d.title = row.title; d.help = row.help; y += SF(ROWH);
        }
        {
            Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), -5);
            row.title = "Free the mouse cursor";
            row.help = "Press in game to let the cursor leave the window (for example to use another program); "
                       "press again to capture it.";
            deco(D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), "Free the mouse cursor");
            Widget& kb = add(W_KEY, RectF(x + w - kw, y + SF(11), kw, SF(30)), -2, 0);
            kb.title = row.title; kb.help = row.help; y += SF(ROWH);
        }
    } else if (id == CAT_PADINT) {
        Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(ROWH)), -6);
        row.title = "Use / interact button";
        row.help = "On PS3 one button both melees and uses switches and doors. Pick a separate button here to "
                   "use things with it; the game's Action button then only melees.";
        deco(D_LABEL, RectF(x, y, w - SF(CTLW) - SF(16), SF(ROWH)), "Use / interact button");
        Widget& d = add(W_DROP, RectF(x + w - SF(CTLW), y + SF(9), SF(CTLW), SF(ROWH - 18)), DD_PADINT);
        d.title = row.title; d.help = row.help; y += SF(ROWH);
    }
}

static void layout()
{
    g_w.clear(); g_deco.clear();
    RECT cr; GetClientRect(g_hw, &cr);
    const float cw = (float)cr.right, ch = (float)cr.bottom;
    // sidebar
    // sidebar: the items share the height between the title and the Play button
    const float nav0 = SF(126);
    const float step = std::max(SF(30), std::min(SF(46), (ch - SF(108) - nav0) / P_COUNT));
    float ny = nav0;
    for (int p = 0; p < P_COUNT; p++) {
        Widget& n = add(W_NAV, RectF(SF(14), ny, SF(SIDEBAR - 28), step - SF(4)), p, 0, false);
        n.title = g_page_name[p]; ny += step;
    }
    g_nav_end = ny;
    Widget& play = add(W_PLAY, RectF(SF(20), ch - SF(96), SF(SIDEBAR - 40), SF(58)), 0, 0, false);
    play.title = "Play"; play.help = "Saves everything and starts the game.";

    g_view = RectF(SF(SIDEBAR), 0, cw - SF(SIDEBAR), ch - SF(FOOT));
    const float x = SF(SIDEBAR + PAD), w = std::min(cw - SF(SIDEBAR + 2 * PAD) - SF(10), SF(720));
    float y = SF(34);
    if (g_page != P_CONTROLS && g_page < P_TROPHIES) {
        deco(D_H1, RectF(x, y, w, SF(34)), g_page_title[g_page]); y += SF(36);
        deco(D_INTRO, RectF(x, y, w, SF(20)), g_page_intro[g_page]);
        y += SF(40);
        if (g_page == P_DISPLAY || g_page == P_GRAPHICS || g_page == P_UPSCALE) layout_recommend_panel(x, y, w);
        layout_cat_groups(x, y, w, g_page);
        if (g_page == P_PERF) {
            y += SF(26);
            Widget& rb = add(W_BUTTON, RectF(x, y, SF(230), SF(38)), BT_REBUILD_SHADERS);
            rb.title = "Rebuild shader cache";
            rb.help = "Deletes the compiled shaders (shadercache\\*.dxbc that have their .hlsl source, and the GPU / driver "
                      "record) but keeps the list of pipelines the game has used and the shader sources. The next start "
                      "compiles everything again (before play with Compile shaders before playing on). Use it after "
                      "graphics glitches that look shader related. Asks first.";
            y += SF(38);
        }
        if (g_page == P_DISPLAY || g_page == P_GRAPHICS || g_page == P_UPSCALE) {
            y += SF(26);
            Widget& b = add(W_BUTTON, RectF(x, y, SF(210), SF(38)), BT_RESET_GFX);
            b.title = "Reset to defaults";
            b.help = "Puts the display, graphics and upscaling settings, the field of view and the launcher's original "
                     "PC options back to their defaults (the recommendation for this PC). Controls are not changed.";
            y += SF(38);
        }
        if (g_page == P_ADVANCED) {
            y += SF(26);
            Widget& b2 = add(W_BUTTON, RectF(x, y, SF(190), SF(38)), BT_SAVES);
            b2.title = "Open save folder"; b2.help = "Opens the folder with your saved games (back it up from here).";
            y += SF(38);
        }
    } else if (g_page == P_CONTROLS) {
        deco(D_H1, RectF(x, y, w, SF(34)), "Controls"); y += SF(36);
        deco(D_INTRO, RectF(x, y, w, SF(20)), "Click a key box, then press the key or mouse button you want. "
                                             "Right-click a box to clear it.");
        y += SF(40);
        const float kw = SF(150), gap = SF(10);
        const float kx0 = x + w - 2 * kw - gap;
        deco(D_KEYCOL, RectF(kx0, y, kw, SF(20)), "KEY");
        deco(D_KEYCOL, RectF(kx0 + kw + gap, y, kw, SF(20)), "ALTERNATE");
        const char* grp = "";
        for (int a = 0; a < NACT; a++) {
            if (g_acts[a].hidden) continue;
            if (strcmp(grp, g_acts[a].group)) {
                grp = g_acts[a].group;
                if (a) y += SF(16);
                deco(D_H2, RectF(x, y, w - 2 * kw - gap, SF(22)), grp);
                std::string up = grp; for (auto& c : up) c = (char)toupper((unsigned char)c);
                g_deco.back().s = up;
                y += SF(28);
            }
            Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(40)), -100 - a);
            row.title = g_acts[a].name; row.help = g_acts[a].help;
            deco(D_LABEL, RectF(x, y, w - 2 * kw - gap - SF(10), SF(40)), g_acts[a].name);
            for (int k = 0; k < NSHOW; k++) {
                Widget& kb = add(W_KEY, RectF(kx0 + k * (kw + gap), y + SF(5), kw, SF(30)), a, k);
                kb.title = g_acts[a].name; kb.help = g_acts[a].help;
            }
            y += SF(40);
        }
        y += SF(12);
        layout_cat_groups(x, y, w, P_CONTROLS, false);   // mouse, controller, gyro, controller buttons
        y += SF(26);
        Widget& b = add(W_BUTTON, RectF(x, y, SF(210), SF(38)), BT_RESET_CTL);
        b.title = "Restore default controls"; b.help = "Puts every key, mouse and controller option back to its default.";
        Widget& b2 = add(W_BUTTON, RectF(x + SF(222), y, SF(190), SF(38)), BT_CONTROLS_TXT);
        b2.title = "Edit controls.txt"; b2.help = "Opens the controls file in Notepad, for anything the page doesn't cover "
                                                  "(one key pressing several buttons, for example).";
        y += SF(38);
    } else if (g_page == P_TROPHIES) {
        lt_layout(x, y, w);
    } else if (g_page == P_TOOLS) {
        lx_layout_tools(x, y, w);
    } else {
        deco(D_H1, RectF(x, y, w, SF(34)), "About"); y += SF(44);
        deco(D_ABOUT_TITLE, RectF(x, y, w, SF(24)), "Resistance: Fall of Man \xE2\x80\x94 native PC port"); y += SF(30);
        deco(D_ABOUT_TEXT, RectF(x, y, w, SF(48)),
             "A static recompilation of the PS3 game: its code was translated to run natively on Windows, with a "
             "Direct3D 12 renderer and keyboard & mouse controls. No emulator.");
        y += SF(58);
        deco(D_ABOUT_TEXT, RectF(x, y, w, SF(48)),
             "Work in progress. It has been tested on one PC and played from the start through Grimsby; later "
             "levels are untested and may crash or look wrong.");
        y += SF(52);
        {
            const std::string ver = dist_read_line(path_of("VERSION"));
            deco(D_ABOUT_TEXT, RectF(x, y, w, SF(44)),
                 "Version " + (ver.empty() ? std::string("unknown") : ver) +
                 (ver.find("beta") == std::string::npos ? " (beta)" : "") + ".  This port uses NVIDIA DLSS. "
                 "NVIDIA, GeForce RTX and NVIDIA DLSS are trademarks of NVIDIA Corporation. Third-party "
                 "licences: THIRD_PARTY_NOTICES.md.");
        }
        y += SF(54);
        deco(D_H2, RectF(x, y, w, SF(22)), "IN-GAME KEYS"); y += SF(30);
        static const char* keys[][2] = {
            { "Alt + Enter", "Switch between windowed and borderless" },
            { "F8", "Free / capture the mouse cursor (changeable on the Controls page)" },
            { "F12 / PrtScn", "Screenshot (PNG in the screenshots folder)" },
            { "F3", "Performance overlay: fps, frame time, draws per frame, CPU time per draw" },
            { "F10", "Quit to desktop (asks first). Also: hold Esc 1 s, or hold Start / Menu 1.5 s" },
            { "F9", "More PC Options: key bindings, controller layout, colour-blind filter, crosshair" },
            { "F7", "Photo mode: pause and hide the overlays, F12 for a clean screenshot" },
            { "Hold Space / A", "Skip a story movie (hold for a second; also Enter or Menu)" },
        };
        for (auto& k : keys) {
            deco(D_LABEL, RectF(x, y, SF(130), SF(30)), k[0]);
            deco(D_LABEL_DIM, RectF(x + SF(140), y, w - SF(140), SF(30)), k[1]);
            y += SF(32);
        }
        y += SF(24);
        deco(D_H2, RectF(x, y, w, SF(22)), "FOLDERS & LINKS"); y += SF(30);
        Widget& b1 = add(W_BUTTON, RectF(x, y, SF(170), SF(38)), BT_FOLDER);
        b1.title = "Open game folder"; b1.help = "The port's folder: settings, controls.txt and the build.";
        Widget& b2 = add(W_BUTTON, RectF(x + SF(182), y, SF(150), SF(38)), BT_LOGS);
        b2.title = "Open logs"; b2.help = "Log files from the last run. Attach logs\\run.log when reporting a bug.";
        Widget& b3 = add(W_BUTTON, RectF(x + SF(344), y, SF(170), SF(38)), BT_WEB);
        b3.title = "Project page"; b3.help = "github.com/Psyykosis/Resistance-Fall-of-Man-Recomp";
        y += SF(50);
        Widget& b4 = add(W_BUTTON, RectF(x, y, SF(170), SF(38)), BT_SAVES);
        b4.title = "Open save folder"; b4.help = "Your saved games (see Display \xE2\x80\xBA Save location).";
        Widget& b5 = add(W_BUTTON, RectF(x + SF(182), y, SF(170), SF(38)), BT_SHOTS);
        b5.title = "Open screenshots"; b5.help = "Screenshots taken with F12 / PrtScn.";
        Widget& br = add(W_BUTTON, RectF(x + SF(364), y, SF(190), SF(38)), BT_REPORT);
        br.title = "Report a problem";
        br.help = "Packs the last crash, logs, settings and PC info into a zip on your Desktop to attach to a bug report.";
        y += SF(38);
        y += SF(30);
        deco(D_H2, RectF(x, y, w, SF(22)), "RESET"); y += SF(30);
        Widget& b6 = add(W_BUTTON, RectF(x, y, SF(210), SF(38)), BT_RESET_ALL);
        b6.title = "Reset all to defaults";
        b6.help = "Puts every launcher setting back to its default: display and graphics (the recommendation for "
                  "your PC), PC options, keys, mouse and controller. Saved games are not touched. Asks first.";
        y += SF(38);
    }
    g_content_h = y + SF(40);
    const float maxs = std::max(0.0f, g_content_h - g_view.Height);
    g_scroll[g_page] = std::max(0.0f, std::min(g_scroll[g_page], maxs));
}

// widget rect in client coordinates
static RectF wr(const Widget& w)
{
    RectF r = w.r;
    if (w.scroll) r.Y -= g_scroll[g_page];
    return r;
}
static bool visible(const Widget& w)
{
    if (!w.scroll) return true;
    RectF r = wr(w);
    return r.Y + r.Height > g_view.Y && r.Y < g_view.Y + g_view.Height;
}
static int hit(float x, float y)
{
    for (int i = (int)g_w.size() - 1; i >= 0; i--) {
        const Widget& w = g_w[i];
        if (w.scroll && (y < g_view.Y || y >= g_view.Y + g_view.Height)) continue;
        RectF r = wr(w);
        if (x >= r.X && x < r.X + r.Width && y >= r.Y && y < r.Y + r.Height) return i;
    }
    return -1;
}

// ---- painting -------------------------------------------------------------
// The sidebar art: an original picture (a town at dusk under an alien tower), drawn by tools/art/make_r1_art.py
// and embedded as RCDATA 2 in launcher.rc. Nothing from the game is shipped. The scaled, faded picture is kept
// between paints and redone when the size changes.
static Bitmap* g_art;
static Bitmap* g_art_scaled;
static void load_art()
{
    HRSRC r = FindResourceA(nullptr, MAKEINTRESOURCEA(2), MAKEINTRESOURCEA(10) /* RT_RCDATA */);
    if (!r) return;
    const DWORD n = SizeofResource(nullptr, r);
    HGLOBAL h = LoadResource(nullptr, r);
    const void* src = h ? LockResource(h) : nullptr;
    if (!src || !n) return;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!mem) return;
    memcpy(GlobalLock(mem), src, n);
    GlobalUnlock(mem);
    IStream* st = nullptr;
    if (FAILED(CreateStreamOnHGlobal(mem, TRUE, &st))) { GlobalFree(mem); return; }
    Bitmap* b = Bitmap::FromStream(st);
    // GDI+ may read the stream lazily: make a decoded copy and let the stream go
    if (b && b->GetLastStatus() == Ok && b->GetWidth() >= 64 && b->GetHeight() >= 64) {
        Bitmap* c = new Bitmap(b->GetWidth(), b->GetHeight(), PixelFormat32bppPARGB);
        { Graphics cg(c); cg.DrawImage(b, 0, 0, b->GetWidth(), b->GetHeight()); }
        g_art = c;
    }
    delete b;
    st->Release();
}
static void draw_sidebar_art(Graphics& g, float sw, float top, float bottom)
{
    if (!g_art) return;
    const int aw = (int)sw, ah = (int)(bottom - top);
    if (aw < 16 || ah < 16) return;
    if (!g_art_scaled || (int)g_art_scaled->GetWidth() != aw || (int)g_art_scaled->GetHeight() != ah) {
        delete g_art_scaled;
        g_art_scaled = new Bitmap(aw, ah, PixelFormat32bppPARGB);
        Graphics cg(g_art_scaled);
        cg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        cg.SetPixelOffsetMode(PixelOffsetModeHalf);
        // the tower and the town (in 480x1000 picture units), cropped around the tower (x = 307)
        const float kx = g_art->GetWidth() / 480.f, ky = g_art->GetHeight() / 1000.f;
        float cw = 480, chh = cw * ah / aw;
        if (chh > 1000) { chh = 1000; cw = chh * aw / ah; }
        const float cx = std::min(480.f - cw, std::max(0.f, 307 - cw / 2)), cy = (1000 - chh) * 0.55f;
        ImageAttributes ia; ia.SetWrapMode(WrapModeTileFlipXY);   // no dark fringe at the edges
        cg.DrawImage(g_art, RectF(0, 0, (REAL)aw, (REAL)ah), cx * kx, cy * ky, cw * kx, chh * ky, UnitPixel, &ia);
        // darkened, and faded in from the sidebar's colour under the menu
        const float fade = SF(140), dark = 0.15f;
        for (int y = 0; y < ah; y++) {
            float a = std::min(1.f, y / fade);
            a = a * a * (3 - 2 * a);
            SolidBrush b(Color((BYTE)(255 * (1 - a * (1 - dark))), C_SIDEBAR.GetR(), C_SIDEBAR.GetG(), C_SIDEBAR.GetB()));
            cg.FillRectangle(&b, 0, y, aw, 1);
        }
    }
    g.DrawImage(g_art_scaled, 0.f, top);
}

static void draw_chevron(Graphics& g, float cx, float cy, float sz, const Color& c, bool up = false)
{
    Pen p(c, SF(1.8)); p.SetLineJoin(LineJoinRound); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
    PointF pts[3] = { PointF(cx - sz, cy + (up ? sz / 2 : -sz / 2)), PointF(cx, cy + (up ? -sz / 2 : sz / 2)),
                      PointF(cx + sz, cy + (up ? sz / 2 : -sz / 2)) };
    g.DrawLines(&p, pts, 3);
}
static float slider_frac(int id)
{
    if (id == SL_FOV) return (float)(g_fov - FOV_MIN) / (FOV_MAX - FOV_MIN);
    return (float)((g_msens - 0.1) / (4.0 - 0.1));
}
static std::wstring slider_text(int id)
{
    wchar_t b[64];
    if (id == SL_FOV) swprintf(b, 64, g_fov == FOV_DEF ? L"%d\x00B0  (PS3)" : L"%d\x00B0", g_fov);
    else swprintf(b, 64, L"%.2fx", g_msens);
    return b;
}
static std::string key_text(int a, int k)
{
    if (a == -2) return g_mrelease.empty() ? "" : pretty(g_mrelease);
    return g_bind[a][k].empty() ? "" : pretty(g_bind[a][k]);
}

static void paint(HDC hdc)
{
    RECT cr; GetClientRect(g_hw, &cr);
    Bitmap bmp(cr.right, cr.bottom, PixelFormat32bppPARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    layout();
    const float cw = (float)cr.right, ch = (float)cr.bottom;
    { SolidBrush b(C_BG); g.FillRectangle(&b, 0.f, 0.f, cw, ch); }

    // ---- content (clipped to the viewport) ----
    g.SetClip(g_view);
    const float sc = g_scroll[g_page];
    for (int i = 0; i < (int)g_w.size(); i++) {      // row highlights under the labels
        const Widget& w = g_w[i];
        if (w.t != W_ROW || !visible(w)) continue;
        RectF r = wr(w);
        bool lit = i == g_hot && !w.disabled;
        if (!lit && g_hot >= 0 && g_w[g_hot].scroll && g_w[g_hot].t != W_ROW && !g_w[g_hot].disabled) {
            RectF h = wr(g_w[g_hot]);
            lit = h.Y >= r.Y && h.Y + h.Height <= r.Y + r.Height + 0.5f && h.X >= r.X;
        }
        if (lit) fill_rr(g, r, SF(8), Color(255, 31, 33, 37));
    }
    for (const Deco& d : g_deco) {
        RectF r = d.r; r.Y -= sc;
        if (r.Y + r.Height < g_view.Y || r.Y > g_view.Y + g_view.Height) continue;
        switch (d.kind) {
        case D_H1: text(g, W(d.s), f_h1, C_TEXT, r); break;
        case D_INTRO: text(g, W(d.s), f_small, C_DIM, r); break;
        case D_H2: {
            text(g, W(d.s), f_h2, C_ACCENT, r);
            break; }
        case D_KEYCOL: text(g, W(d.s), f_tiny, C_FAINT, r, AL_C); break;
        case D_LABEL: text(g, W(d.s), f_body, C_TEXT, r); break;
        case D_LABEL_DIM: text(g, W(d.s), f_body, C_FAINT, r); break;
        case D_NOTE: text(g, W(d.s), f_small, C_DIM, r, AL_L, false, true); break;
        case D_ABOUT_TITLE: text(g, W(d.s), f_btn, C_TEXT, r); break;
        case D_ABOUT_TEXT: text(g, W(d.s), f_body, C_DIM, r, AL_L, false, true); break;
        case D_PANEL: fill_rr(g, r, SF(8), C_PANEL); break;
        case D_REC: text(g, W(d.s), f_body, C_TEXT, r, AL_L, true, false); break;
        case D_TROPHY: lt_paint(g, d.s, r); break;
        }
    }
    for (int i = 0; i < (int)g_w.size(); i++) {
        const Widget& w = g_w[i];
        if (!w.scroll || !visible(w)) continue;
        RectF r = wr(w);
        const bool hot = i == g_hot && !w.disabled;
        switch (w.t) {
        case W_ROW: break;
        case W_DROP: {
            const bool open = g_dd.open && g_dd.kind == w.id && (w.id != DD_SETTING || g_dd.idx == w.sub);
            fill_rr(g, r, SF(6), w.disabled ? C_PANEL : (hot || open) ? C_CTL_HOT : C_CTL);
            if (open) stroke_rr(g, r, SF(6), C_ACCENT, SF(1.2));
            const std::string lab = dd_label(w.id, w.sub, dd_sel(w.id, w.sub));
            const bool rec = dd_recommended(w.id, w.sub, dd_sel(w.id, w.sub));
            const float tagw = rec ? SF(96) : 0;
            text(g, W(lab), f_body, w.disabled ? C_FAINT : C_TEXT, RectF(r.X + SF(14), r.Y, r.Width - SF(44) - tagw, r.Height));
            if (rec) text(g, L"recommended", f_tiny, w.disabled ? C_FAINT : C_ACCENT,
                          RectF(r.X + r.Width - SF(36) - tagw, r.Y, tagw, r.Height), AL_R);
            draw_chevron(g, r.X + r.Width - SF(20), r.Y + r.Height / 2, SF(5), w.disabled ? C_FAINT : C_DIM, open);
            break; }
        case W_SLIDER: {
            const float f = slider_frac(w.id);
            const float tx = r.X + SF(8), tw = r.Width - SF(100), ty = r.Y + r.Height / 2;
            fill_rr(g, RectF(tx, ty - SF(2), tw, SF(4)), SF(2), C_CTL_HOT);
            fill_rr(g, RectF(tx, ty - SF(2), tw * f, SF(4)), SF(2), C_ACCENT);
            if (w.id == SL_FOV) {      // marker at the PS3 value
                const float px = tx + tw * (float)(FOV_DEF - FOV_MIN) / (FOV_MAX - FOV_MIN);
                SolidBrush tb(C_FAINT); g.FillRectangle(&tb, px - SF(0.75), ty + SF(6), SF(1.5), SF(5));
                text(g, L"PS3", f_tiny, C_FAINT, RectF(px - SF(20), ty + SF(10), SF(40), SF(14)), AL_C);
            }
            const float kr = (hot || g_drag == w.id) ? SF(9) : SF(8);
            SolidBrush kb(g_drag == w.id ? C_ACCENT_HOT : C_TEXT);
            g.FillEllipse(&kb, tx + tw * f - kr, ty - kr, 2 * kr, 2 * kr);
            text(g, slider_text(w.id), f_body, C_TEXT, RectF(r.X + r.Width - SF(84), r.Y, SF(84), r.Height), AL_R);
            break; }
        case W_TOGGLE: {
            const bool on = w.id == TG_INVERT ? g_minv != 0 : false;
            fill_rr(g, r, r.Height / 2, on ? C_ACCENT : (hot ? C_CTL_HOT : C_CTL));
            if (!on) stroke_rr(g, r, r.Height / 2, C_BORDER, SF(1));
            const float d = r.Height - SF(8);
            SolidBrush kb(on ? C_ON_ACCENT : C_DIM);
            g.FillEllipse(&kb, on ? r.X + r.Width - d - SF(4) : r.X + SF(4), r.Y + SF(4), d, d);
            break; }
        case W_KEY: {
            const bool cap = (w.id == g_cap_a && w.sub == g_cap_k);
            const std::string t = key_text(w.id, w.sub);
            if (cap) fill_rr(g, r, SF(6), C_ACCENT_DIM);
            else if (t.empty()) { if (hot) fill_rr(g, r, SF(6), C_CTL); stroke_rr(g, r, SF(6), hot ? C_BORDER : Color(255, 44, 47, 52), SF(1)); }
            else fill_rr(g, r, SF(6), hot ? C_CTL_HOT : C_CTL);
            if (cap) stroke_rr(g, r, SF(6), C_ACCENT, SF(1.4));
            if (cap) text(g, L"Press a key\x2026", f_small, C_ACCENT_HOT, r, AL_C);
            else if (t.empty()) text(g, L"\x2014", f_body, C_FAINT, r, AL_C);
            else text(g, W(t), f_body, C_TEXT, r, AL_C);
            break; }
        case W_BUTTON: {
            fill_rr(g, r, SF(6), hot ? C_CTL_HOT : C_CTL);
            stroke_rr(g, r, SF(6), C_BORDER, SF(1));
            text(g, W(w.title), f_btn, C_TEXT, r, AL_C);
            break; }
        default: break;
        }
    }
    g.ResetClip();

    // scrollbar
    if (g_content_h > g_view.Height + 1) {
        const float trackH = g_view.Height - SF(16);
        const float th = std::max(SF(40), trackH * g_view.Height / g_content_h);
        const float ty = g_view.Y + SF(8) + (trackH - th) * (g_scroll[g_page] / (g_content_h - g_view.Height));
        fill_rr(g, RectF(cw - SF(10), ty, SF(5), th), SF(2.5), Color(140, 120, 126, 134));
    }

    // ---- help strip ----
    {
        RectF fr(SF(SIDEBAR), ch - SF(FOOT), cw - SF(SIDEBAR), SF(FOOT));
        SolidBrush b(C_PANEL); g.FillRectangle(&b, fr);
        Pen ln(Color(255, 44, 47, 52), SF(1)); g.DrawLine(&ln, fr.X, fr.Y, fr.X + fr.Width, fr.Y);
        const float x = SF(SIDEBAR + PAD), w = fr.Width - SF(2 * PAD);
        std::wstring t1, t2;
        Color c1 = C_TEXT;
        if (g_cap_a != -1) {
            t1 = L"Press the key or mouse button to use";
            t2 = L"Any key works, including Esc. Click anywhere else to cancel.";
            c1 = C_ACCENT_HOT;
        } else if (!g_toast.empty() && GetTickCount() < g_toast_until) {
            t1 = g_toast; c1 = C_WARN;
        } else if (g_hot >= 0 && !g_w[g_hot].help.empty() && g_w[g_hot].t != W_NAV) {
            t1 = W(g_w[g_hot].title); t2 = W(g_w[g_hot].help);
        } else if (g_hot >= 0 && g_w[g_hot].t == W_KEY) {
            t1 = W(g_w[g_hot].title); t2 = L"Click to change. Right-click to clear.";
        } else {
            t2 = g_page == P_CONTROLS ? L"Hover over anything to see what it does. Changes are saved automatically."
               : L"Hover over a setting to see what it does. Changes are saved automatically and apply the next "
                 L"time the game starts.";
        }
        if (!t1.empty()) text(g, t1, f_btn, c1, RectF(x, fr.Y + SF(12), w, SF(22)));
        if (!t2.empty()) text(g, t2, f_small, C_DIM, RectF(x, fr.Y + (t1.empty() ? SF(14) : SF(36)), w, SF(36)),
                              AL_L, false, true);
    }

    // ---- sidebar ----
    {
        SolidBrush b(C_SIDEBAR); g.FillRectangle(&b, 0.f, 0.f, SF(SIDEBAR), ch);
        draw_sidebar_art(g, SF(SIDEBAR), std::max(SF(285), g_nav_end - SF(40)), ch);
        {   // the title, shrunk if the font is wider than the sidebar
            const float avail = SF(SIDEBAR - 46);
            RectF m; g.MeasureString(L"RESISTANCE", -1, f_title, PointF(0, 0), &m);
            if (m.Width > avail) {
                FontFamily ff; f_title->GetFamily(&ff);
                Font fsmall(&ff, f_title->GetSize() * avail / m.Width, f_title->GetStyle(), UnitPixel);
                text(g, L"RESISTANCE", &fsmall, C_TEXT, RectF(SF(24), SF(34), SF(SIDEBAR - 30), SF(44)));
            } else text(g, L"RESISTANCE", f_title, C_TEXT, RectF(SF(24), SF(34), SF(SIDEBAR - 30), SF(44)));
        }
        text(g, L"FALL OF MAN", f_sub, C_ACCENT, RectF(SF(26), SF(76), SF(SIDEBAR - 30), SF(20)), AL_L, true);
        {   // a thin accent rule after the subtitle, as on the other ports' launchers
            RectF m; g.MeasureString(L"FALL OF MAN", -1, f_sub, PointF(0, 0), &m);
            const float lx = SF(26) + m.Width + SF(8), rx = SF(SIDEBAR - 22);
            if (rx > lx + SF(10)) { Pen ln(Color(160, C_ACCENT.GetR(), C_ACCENT.GetG(), C_ACCENT.GetB()), SF(1));
                                    g.DrawLine(&ln, lx, SF(87), rx, SF(87)); }
        }
        text(g, L"PC port \x00B7 work in progress", f_tiny, C_FAINT, RectF(SF(26), SF(98), SF(SIDEBAR - 30), SF(18)));
        for (int i = 0; i < (int)g_w.size(); i++) {
            const Widget& w = g_w[i];
            if (w.t == W_NAV) {
                const bool sel = w.id == g_page, hot = i == g_hot;
                if (sel) fill_rr(g, w.r, SF(8), Color(255, 34, 36, 40));
                else if (hot) fill_rr(g, w.r, SF(8), Color(255, 26, 28, 31));
                if (sel) fill_rr(g, RectF(w.r.X, w.r.Y + SF(10), SF(3), w.r.Height - SF(20)), SF(1.5), C_ACCENT);
                text(g, W(w.title), f_nav, sel ? C_TEXT : hot ? Color(255, 205, 208, 212) : C_DIM,
                     RectF(w.r.X + SF(18), w.r.Y, w.r.Width - SF(18), w.r.Height));
            } else if (w.t == W_PLAY) {
                const bool hot = i == g_hot, down = i == g_press;
                fill_rr(g, w.r, SF(8), down ? C_ACCENT : hot ? C_ACCENT_HOT : C_ACCENT);
                text(g, L"PLAY", f_play, C_ON_ACCENT, w.r, AL_C);
            }
        }
    }

    // ---- dropdown list (on top of everything) ----
    if (g_dd.open) {
        RectF lr = g_dd.list;
        { GraphicsPath p; round_rect(p, RectF(lr.X + SF(2), lr.Y + SF(4), lr.Width, lr.Height), SF(8));
          SolidBrush sh(Color(110, 0, 0, 0)); g.FillPath(&sh, &p); }
        fill_rr(g, lr, SF(8), Color(255, 44, 47, 53));
        stroke_rr(g, lr, SF(8), C_BORDER, SF(1));
        const int n = dd_count(g_dd.kind, g_dd.idx), cur = dd_sel(g_dd.kind, g_dd.idx);
        for (int i = 0; i < n; i++) {
            RectF ir(lr.X + SF(4), lr.Y + SF(4) + i * g_dd.item_h, lr.Width - SF(8), g_dd.item_h);
            if (i == g_dd.hot) fill_rr(g, ir, SF(5), C_CTL_HOT);
            if (i == cur) fill_rr(g, RectF(ir.X + SF(4), ir.Y + ir.Height / 2 - SF(7), SF(3), SF(14)), SF(1.5), C_ACCENT);
            const bool rec = dd_recommended(g_dd.kind, g_dd.idx, i);
            const float tagw = rec ? SF(96) : 0;
            text(g, W(dd_label(g_dd.kind, g_dd.idx, i)), f_body, i == cur ? C_TEXT : Color(255, 214, 216, 220),
                 RectF(ir.X + SF(16), ir.Y, ir.Width - SF(20) - tagw, ir.Height));
            if (rec) text(g, L"recommended", f_tiny, C_ACCENT, RectF(ir.X + ir.Width - SF(8) - tagw, ir.Y, tagw, ir.Height), AL_R);
        }
    }

    // ---- offline shader scan progress (on top of everything) ----
    if (g_scan.active) {
        std::string phase; unsigned d, t; bool ran;
        AcquireSRWLockShared(&g_scan_lock);
        phase = g_scan.phase; d = g_scan.done; t = g_scan.total; ran = g_scan.ran;
        ReleaseSRWLockShared(&g_scan_lock);
        if (ran || GetTickCount() - g_scan.t0 >= 400) {   // a quick "already done" check never flashes the panel
            SolidBrush dim(Color(175, 6, 8, 10)); g.FillRectangle(&dim, 0.f, 0.f, cw, ch);
            const float pw = std::min(cw - SF(80), SF(580)), ph = SF(196);
            RectF pr((cw - pw) / 2, (ch - ph) / 2, pw, ph);
            fill_rr(g, pr, SF(10), C_PANEL); stroke_rr(g, pr, SF(10), C_BORDER, SF(1));
            const float ix = pr.X + SF(24), iw = pr.Width - SF(48);
            text(g, L"Preparing shaders from your game files", f_h2, C_TEXT, RectF(ix, pr.Y + SF(18), iw, SF(28)));
            std::string sub = !ran ? std::string("Checking the shader cache...")
                            : t ? phase + "  " + std::to_string(std::min(d, t)) + " / " + std::to_string(t)
                                : (phase.empty() ? std::string("scan") : phase) + "...";
            text(g, W(sub), f_body, C_DIM, RectF(ix, pr.Y + SF(52), iw, SF(22)));
            RectF br(ix, pr.Y + SF(84), iw, SF(8));
            fill_rr(g, br, SF(4), C_CTL);
            if (t) fill_rr(g, RectF(br.X, br.Y, std::max(SF(8), br.Width * std::min(d, t) / t), br.Height), SF(4), C_ACCENT);
            else {   // total not known yet: a moving segment
                const float seg = br.Width / 4, pos = (float)((GetTickCount() / 8) % 1000) / 1000.f * (br.Width + seg) - seg;
                const float l = std::max(br.X, br.X + pos), r = std::min(br.X + br.Width, br.X + pos + seg);
                if (r > l) fill_rr(g, RectF(l, br.Y, r - l, br.Height), SF(4), C_ACCENT);
            }
            text(g, L"Once per install and after Rebuild shader cache: the shaders in your game files are added to the "
                    L"shader cache, so Compile shaders before playing covers the whole game. The game starts when done.",
                 f_small, C_FAINT, RectF(ix, pr.Y + SF(102), iw, SF(40)), AL_L, false, true);
            g_scan.skip_btn = RectF(pr.X + pr.Width - SF(24) - SF(150), pr.Y + ph - SF(50), SF(150), SF(34));
            fill_rr(g, g_scan.skip_btn, SF(7), C_CTL); stroke_rr(g, g_scan.skip_btn, SF(7), C_BORDER, SF(1));
            text(g, L"Skip for now", f_btn, C_TEXT, g_scan.skip_btn, AL_C);
        } else g_scan.skip_btn = RectF();
    }

    Graphics screen(hdc);
    screen.DrawImage(&bmp, 0, 0);
}

// ---- interaction ----------------------------------------------------------
static void invalidate() { InvalidateRect(g_hw, NULL, FALSE); }

static void open_dropdown(const Widget& w)
{
    RECT cr; GetClientRect(g_hw, &cr);
    RectF a = wr(w);
    g_dd.open = true; g_dd.kind = w.id; g_dd.idx = w.sub; g_dd.anchor = a;
    g_dd.item_h = SF(32);
    const int n = dd_count(w.id, w.sub);
    const float h = n * g_dd.item_h + SF(8);
    // over the box, with the current choice where the box is (like a Windows menu)
    const int cur = dd_sel(w.id, w.sub);
    float y = a.Y + (a.Height - g_dd.item_h) / 2 - SF(4) - cur * g_dd.item_h;
    y = std::max(SF(8), std::min(y, (float)cr.bottom - SF(8) - h));
    g_dd.list = RectF(a.X, y, a.Width, h);
    g_dd.hot = dd_sel(w.id, w.sub);
}
static void close_dropdown() { g_dd.open = false; g_dd.hot = -1; }
static int dd_hit(float x, float y)
{
    const RectF& lr = g_dd.list;
    if (x < lr.X || x >= lr.X + lr.Width || y < lr.Y + SF(4) || y >= lr.Y + lr.Height - SF(4)) return -1;
    const int i = (int)((y - lr.Y - SF(4)) / g_dd.item_h);
    return i < dd_count(g_dd.kind, g_dd.idx) ? i : -1;
}

static void slider_set(int id, float x)
{
    for (const Widget& w : g_w) if (w.t == W_SLIDER && w.id == id) {
        RectF r = wr(w);
        const float tx = r.X + SF(8), tw = r.Width - SF(100);
        float f = (x - tx) / tw; f = std::max(0.0f, std::min(1.0f, f));
        if (id == SL_FOV) g_fov = FOV_MIN + (int)lround(f * (FOV_MAX - FOV_MIN));
        else { int st = (int)lround((0.1 + f * 3.9) * 20); g_msens = std::max(2, std::min(80, st)) / 20.0; }
    }
}
static void slider_commit(int id) { if (id == SL_FOV) save_settings(); else ctl_save(); }

static void capture_done(const char* key)
{
    if (g_cap_a == -1) return;
    if (key) {
        if (g_cap_a == -2) { g_mrelease = key; }
        else {
            // one key, one action: a key taken from another action moves here
            std::string from;
            for (int a = 0; a < NACT; a++) for (int k = 0; k < NSLOT; k++)
                if (!_stricmp(g_bind[a][k].c_str(), key) && !(a == g_cap_a && k == g_cap_k)) {
                    g_bind[a][k].clear(); if (!g_acts[a].hidden) from = g_acts[a].name; }
            g_bind[g_cap_a][g_cap_k] = key;
            if (!from.empty()) toast(pretty(key) + " was removed from \"" + from + "\" and now does \"" + g_acts[g_cap_a].name + "\".");
            else if (!_stricmp(key, g_mrelease.c_str()))
                toast("Note: " + pretty(key) + " also frees the mouse cursor in game.");
        }
        ctl_save();
    }
    g_cap_a = g_cap_k = -1;
    invalidate();
}

static void apply_recommended()
{
    for (int si = 0; si < S_COUNT; si++) {
        const char* r = rec_value(si);
        if (!r) continue;
        for (size_t i = 0; i < g_set[si].choices.size(); i++)
            if (!strcmp(g_set[si].choices[i].value, r)) g_set[si].sel = (int)i;
    }
    save_settings();
}
static void restore_graphics()
{
    for (int si = 0; si < S_INPUT_FIRST; si++) g_set[si].sel = g_set[si].def;
    g_fov = FOV_DEF;
    apply_recommended();   // the defaults ARE the recommended settings for this PC
    toast("Display, Graphics and Upscaling settings restored to their defaults (recommended for this PC).");
}
static std::string saves_folder(bool* exists)
{
    std::string root;
    if (!strcmp(val(S_SAVES), "portable")) root = path_of("gamedata");
    else {
        PWSTR sg = nullptr; char buf[MAX_PATH * 2] = "";
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SavedGames, 0, NULL, &sg)) && sg)
            WideCharToMultiByte(CP_ACP, 0, sg, -1, buf, sizeof buf, NULL, NULL);
        if (sg) CoTaskMemFree(sg);
        root = std::string(buf) + "\\Resistance Fall of Man";
    }
    const std::string sd = root + "\\dev_hdd0\\home\\00000001\\savedata";
    *exists = GetFileAttributesA(sd.c_str()) != INVALID_FILE_ATTRIBUTES;
    return *exists ? sd : root;
}

static void play_launch()
{
    lx_auto_backup();
    if (GetFileAttributesA(path_of("build\\ResistanceFoM.exe").c_str()) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(g_hw, "The game is not built yet. Run Setup.bat first.", "Launcher", MB_ICONERROR);
        return;
    }
    if ((INT_PTR)ShellExecuteA(g_hw, "open", path_of("run.bat").c_str(), NULL, g_dir, SW_HIDE) <= 32) {
        MessageBoxA(g_hw, "Could not start run.bat. Is the game built? (Run Setup.bat first.)", "Launcher", MB_ICONERROR);
        return;
    }
    DestroyWindow(g_hw);
}

// ---- offline shader scan (shaderscan.exe; contract: scratch\shaderscan_launcher_contract.md) ----
// When shaderscan.exe is next to the launcher (or in tools\), Play first lets it add the shaders found in the game
// files to shadercache\ (R1 D3D12 back end: FXC-compiled <key>.dxbc + .hlsl, keyed like the game; once: it
// leaves shaderscan.stamp there, which Rebuild shader cache deletes), then starts the game, whose start-up PSO
// prebuild ("Compile shaders before playing") uses them. The scan
// runs on a worker thread and the window keeps pumping messages; Skip for now kills it (it runs again next time).
static std::string shaderscan_exe()
{
    for (const char* p : { "shaderscan.exe", "tools\\shaderscan.exe" })
        if (GetFileAttributesA(path_of(p).c_str()) != INVALID_FILE_ATTRIBUTES) return path_of(p);
    return {};
}
static void scan_line(const std::string& l, FILE* log)
{
    if (l.empty()) return;
    char ph[32] = ""; unsigned d = 0, t = 0;
    const bool prog = sscanf(l.c_str(), "@progress %31s %u %u", ph, &d, &t) == 3 ||
                      sscanf(l.c_str(), "[%31[^]]] %u/%u", ph, &d, &t) == 3;
    bool changed = false;
    AcquireSRWLockExclusive(&g_scan_lock);
    if (prog) { changed = g_scan.phase != ph; g_scan.phase = ph; g_scan.done = d; g_scan.total = t; }
    else if (!strncmp(l.c_str(), "@error ", 7)) g_scan.error = l.substr(7);
    else if (!strncmp(l.c_str(), "shaderscan: ", 12)) g_scan.error = l.substr(12);
    ReleaseSRWLockExclusive(&g_scan_lock);
    if (log && (!prog || changed || d == t)) { fprintf(log, "%s\n", l.c_str()); fflush(log); }
    if (prog) {
        static DWORD last = 0;
        const DWORD now = GetTickCount();
        if (changed || d == t || now - last >= 50) { last = now; PostMessageA(g_hw, WM_SCAN_PROGRESS, 0, 0); }
    }
}
// Runs one shaderscan command with stdout + stderr on a pipe; returns its exit code (-1: could not run / timed out).
static int scan_exec(std::string cl, DWORD timeout_ms, FILE* log)
{
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {}; si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = si.hStdError = wr;
    PROCESS_INFORMATION pi = {};
    if (log) { fprintf(log, "> %s\n", cl.c_str()); fflush(log); }
    if (!CreateProcessA(NULL, &cl[0], NULL, NULL, TRUE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, NULL, g_dir, &si, &pi)) {
        if (log) fprintf(log, "could not start it (error %lu)\n", GetLastError());
        CloseHandle(rd); CloseHandle(wr); return -1;
    }
    CloseHandle(wr); CloseHandle(pi.hThread);
    AcquireSRWLockExclusive(&g_scan_lock);
    g_scan.proc = pi.hProcess;
    if (g_scan.cancel) TerminateProcess(pi.hProcess, 1);
    ReleaseSRWLockExclusive(&g_scan_lock);
    bool timed_out = false;
    if (timeout_ms != INFINITE && WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1); timed_out = true;
    }
    std::string line; char buf[4096]; DWORD n = 0;
    while (ReadFile(rd, buf, sizeof buf, &n, NULL) && n)
        for (DWORD i = 0; i < n; i++) {
            if (buf[i] == '\r' || buf[i] == '\n') { scan_line(line, log); line.clear(); }
            else if (line.size() < 4096) line += buf[i];
        }
    scan_line(line, log);
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    AcquireSRWLockExclusive(&g_scan_lock);
    g_scan.proc = NULL;
    ReleaseSRWLockExclusive(&g_scan_lock);
    CloseHandle(pi.hProcess);
    if (log) { fprintf(log, "exit %d%s\n", (int)code, timed_out ? " (timed out)" : ""); fflush(log); }
    return timed_out ? -1 : (int)code;
}
static DWORD WINAPI scan_thread(LPVOID)
{
    const std::string exe = shaderscan_exe(), out = path_of("shadercache");
    CreateDirectoryA(path_of("logs").c_str(), NULL);
    FILE* log = fopen(path_of("logs\\shaderscan.log").c_str(), "w");
    // --check (reads ..\settings.bat, so it runs after save_settings()): 0 = this version already seeded the cache, 1 = run it; anything else (or > 10 s) = skip this time
    int rc = scan_exec("\"" + exe + "\" --check --out \"" + out + "\"", 10000, log);
    bool cancelled;
    AcquireSRWLockShared(&g_scan_lock); cancelled = g_scan.cancel; ReleaseSRWLockShared(&g_scan_lock);
    if (rc == 1 && !cancelled) {
        AcquireSRWLockExclusive(&g_scan_lock); g_scan.ran = true; g_scan.phase = "scan"; ReleaseSRWLockExclusive(&g_scan_lock);
        PostMessageA(g_hw, WM_SCAN_PROGRESS, 0, 0);
        rc = scan_exec("\"" + exe + "\" --game \"" + g_dir + "\" --out \"" + out + "\" --backend r1d3d12 --progress", INFINITE, log);
    } else rc = 0;
    g_scan.exit_code = rc;
    if (log) fclose(log);
    PostMessageA(g_hw, WM_SCAN_DONE, 0, 0);
    return 0;
}
static void scan_cancel()
{
    AcquireSRWLockExclusive(&g_scan_lock);
    g_scan.cancel = true;
    if (g_scan.proc) TerminateProcess(g_scan.proc, 1);
    ReleaseSRWLockExclusive(&g_scan_lock);
}
static void scan_finish()
{
    if (g_scan.thread) { WaitForSingleObject(g_scan.thread, INFINITE); CloseHandle(g_scan.thread); g_scan.thread = NULL; }
    KillTimer(g_hw, 2);
    g_scan.active = false;
    if (g_scan.quit) { DestroyWindow(g_hw); return; }
    play_launch();    // the game starts also when the scan failed or was skipped (logs\shaderscan.log has the details)
}

static void play()
{
    if (g_scan.active) return;
    save_settings(); ctl_save();          // first: --check compares the stamp with settings.bat (RSX_RES_SCALE ...)
    if (!g_scan.checked && !shaderscan_exe().empty()) {
        g_scan.checked = true;
        g_scan.cancel = g_scan.ran = g_scan.quit = false;
        g_scan.phase.clear(); g_scan.error.clear(); g_scan.done = g_scan.total = 0;
        g_scan.t0 = GetTickCount();
        g_scan.active = true;
        g_scan.thread = CreateThread(NULL, 0, scan_thread, NULL, 0, NULL);
        if (g_scan.thread) { SetTimer(g_hw, 2, 100, NULL); InvalidateRect(g_hw, NULL, FALSE); return; }
        g_scan.active = false;
    }
    play_launch();
}

// "Rebuild shader cache": deletes the compiled shaders, keeps what tells the game which ones to build
// (pso_manifest_*.bin, *.hlsl). In shadercache\ and run.bat's backup of it (%LOCALAPPDATA%\ResistancePort\shadercache,
// copied back into a game folder where files are missing).
static bool shc_game_running()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe; pe.dwSize = sizeof pe;
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe))
        found = !_wcsicmp(pe.szExeFile, L"ResistanceFoM.exe") || !_wcsicmp(pe.szExeFile, L"ResistanceFoMGame.exe");
    CloseHandle(snap);
    return found;
}
static void rebuild_shader_cache()
{
    if (shc_game_running()) {
        MessageBoxA(g_hw, "The game is running. Close it first, then rebuild the shader cache.", "Rebuild shader cache", MB_ICONINFORMATION);
        return;
    }
    if (MessageBoxA(g_hw, "Delete the compiled shaders?\n\n"
                    "The list of pipelines the game has used and the shader sources are kept: the next time you play, every "
                    "shader is compiled again (with \"Compile shaders before playing\" on, before play begins - this can take "
                    "a minute).\n\n"
                    "Deleted from shadercache\\ and its backup in %LOCALAPPDATA%\\ResistancePort\\shadercache:\n"
                    "  *.dxbc (compiled shaders) that have a matching .hlsl source\n"
                    "  pso_gpu_stamp.txt (the GPU / driver they were built for)\n"
                    "  shaderscan.stamp (with shaderscan.exe installed, the game files are scanned for shaders again)\n"
                    "Kept: pso_manifest_*.bin and *.hlsl.",
                    "Rebuild shader cache", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    std::vector<std::string> roots = { path_of("shadercache") };
    char la[MAX_PATH];
    if (GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH)) roots.push_back(std::string(la) + "\\ResistancePort\\shadercache");
    int n = 0, failed = 0; unsigned long long bytes = 0;
    auto del = [&](const std::string& p, const WIN32_FIND_DATAA* fd) {
        if (DeleteFileA(p.c_str())) { n++; if (fd) bytes += ((unsigned long long)fd->nFileSizeHigh << 32) | fd->nFileSizeLow; }
        else if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) failed++;
    };
    for (const std::string& r : roots) {
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((r + "\\*.dxbc").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string name = fd.cFileName, src = r + "\\" + name.substr(0, name.size() - 5) + ".hlsl";
                if (GetFileAttributesA(src.c_str()) == INVALID_FILE_ATTRIBUTES) continue;   // no source: cannot be rebuilt
                del(r + "\\" + name, &fd);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        const std::string st = r + "\\pso_gpu_stamp.txt";
        if (GetFileAttributesA(st.c_str()) != INVALID_FILE_ATTRIBUTES) del(st, nullptr);
        const std::string ss = r + "\\shaderscan.stamp";      // offline shader scan runs again on the next Play
        if (GetFileAttributesA(ss.c_str()) != INVALID_FILE_ATTRIBUTES) del(ss, nullptr);
    }
    char b[200];
    if (failed) snprintf(b, sizeof b, "Shader cache: %d file(s) deleted, %d could not be deleted (in use?).", n, failed);
    else if (n) snprintf(b, sizeof b, "Shader cache cleared (%d file(s), %.1f MB). Shaders are rebuilt the next time you play.", n, bytes / 1048576.0);
    else snprintf(b, sizeof b, "The shader cache was already empty. Shaders are built the next time you play.");
    toast(b);
}

static void click(int i, float x, float y)
{
    const Widget& w = g_w[i];
    if (w.disabled) return;
    switch (w.t) {
    case W_NAV: if (g_page != w.id) { g_page = w.id; close_dropdown(); } break;
    case W_PLAY: play(); return;
    case W_DROP: open_dropdown(w); break;
    case W_SLIDER: g_drag = w.id; SetCapture(g_hw); slider_set(w.id, x); break;
    case W_TOGGLE: if (w.id == TG_INVERT) { g_minv = !g_minv; ctl_save(); } break;
    case W_KEY: g_cap_a = w.id; g_cap_k = w.sub; break;
    case W_BUTTON:
        if (lx_click(w.id, w.sub)) {}
        else if (w.id == BT_RESET_CTL) {
            if (MessageBoxA(g_hw, "Put every key, mouse and controller option back to its default?", "Controls",
                            MB_YESNO | MB_ICONQUESTION) == IDYES) {
                ctl_defaults(); ctl_save();
                for (int si = S_INPUT_FIRST; si < S_COUNT; si++) g_set[si].sel = g_set[si].def;
                save_settings();
                toast("Controls restored to their defaults."); }
        } else if (w.id == BT_RESET_GFX) {
            if (MessageBoxA(g_hw, "Put the Display, Graphics and Upscaling settings back to their defaults?", "Settings",
                            MB_YESNO | MB_ICONQUESTION) == IDYES) restore_graphics();
        } else if (w.id == BT_RESET_ALL) {
            if (MessageBoxA(g_hw, "Put every launcher setting (display, graphics, PC options, keys, mouse and controller) "
                            "back to its default? Saved games are not touched.", "Reset",
                            MB_YESNO | MB_ICONQUESTION) == IDYES) {
                ctl_defaults(); ctl_save();
                for (auto& s : g_set) s.sel = s.def;   // includes the extras and the update check
                restore_graphics();                    // FOV + this PC's recommendation, saves
                toast("Everything reset to defaults (graphics: the recommendation for this PC)."); }
        } else if (w.id == BT_AUTO) {
            apply_recommended();
            char st[200]; rfac::describe(g_rec, st, sizeof st);
            toast(std::string("Applied: ") + st);
        } else if (w.id == BT_SAVES) {
            bool ex = false; const std::string p = saves_folder(&ex);
            if (!ex && strcmp(val(S_SAVES), "portable") &&
                GetFileAttributesA(path_of("gamedata\\dev_hdd0\\home\\00000001\\savedata").c_str()) != INVALID_FILE_ATTRIBUTES) {
                toast("Your saves move to Saved Games the next time the game starts; until then they are in the game folder.");
                ShellExecuteA(g_hw, "open", path_of("gamedata\\dev_hdd0\\home\\00000001\\savedata").c_str(), NULL, NULL, SW_SHOWNORMAL);
            } else {
                SHCreateDirectoryExA(NULL, p.c_str(), NULL);
                if (!ex) toast("No saves yet - this is where they will be.");
                ShellExecuteA(g_hw, "open", p.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        } else if (w.id == BT_SHOTS) { CreateDirectoryA(path_of("screenshots").c_str(), NULL);
                                       ShellExecuteA(g_hw, "open", path_of("screenshots").c_str(), NULL, NULL, SW_SHOWNORMAL); }
        else if (w.id == BT_REBUILD_SHADERS) rebuild_shader_cache();
        else if (w.id == BT_FOLDER) ShellExecuteA(g_hw, "open", g_dir, NULL, NULL, SW_SHOWNORMAL);
        else if (w.id == BT_REPORT) port_report_ui(g_hw);
        else if (w.id == BT_LOGS) { CreateDirectoryA(path_of("logs").c_str(), NULL);
                                    ShellExecuteA(g_hw, "open", path_of("logs").c_str(), NULL, NULL, SW_SHOWNORMAL); }
        else if (w.id == BT_WEB) ShellExecuteA(g_hw, "open", "https://github.com/Psyykosis/Resistance-Fall-of-Man-Recomp", NULL, NULL, SW_SHOWNORMAL);
        else if (w.id == BT_CONTROLS_TXT) { ctl_save(); ShellExecuteA(g_hw, "open", "notepad.exe", path_of("controls.txt").c_str(), NULL, SW_SHOWNORMAL); }
        break;
    default: break;
    }
    (void)y;
    invalidate();
}

static void set_hot(int i)
{
    if (i == g_hot) return;
    g_hot = i; invalidate();
}

static void scroll_by(float d)
{
    if (g_content_h <= g_view.Height) return;
    g_scroll[g_page] = std::max(0.0f, std::min(g_scroll[g_page] + d, g_content_h - g_view.Height));
    invalidate();
}

static void make_scale(HWND hw)
{
    UINT dpi = 96;
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    static GetDpiForWindowFn fn = (GetDpiForWindowFn)(void*)GetProcAddress(GetModuleHandleA("user32.dll"), "GetDpiForWindow");
    if (fn && hw) dpi = fn(hw);
    else { HDC dc = GetDC(NULL); dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(NULL, dc); }
    g_s = dpi / 96.0;
    make_fonts();
}

static LRESULT CALLBACK wndproc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
    if (g_scan.active) {   // offline shader scan running: only Skip for now / closing the window
        switch (msg) {
        case WM_LBUTTONDOWN:
            if (g_scan.skip_btn.Contains((float)GET_X_LPARAM(lp), (float)GET_Y_LPARAM(lp))) scan_cancel();
            return 0;
        case WM_KEYDOWN: if (wp == VK_ESCAPE) scan_cancel(); return 0;
        case WM_MOUSEMOVE: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN: case WM_LBUTTONUP:
        case WM_MOUSEWHEEL: case WM_CHAR: return 0;
        case WM_CLOSE: g_scan.quit = true; scan_cancel(); return 0;
        default: break;
        }
    }
    switch (msg) {
    case WM_SCAN_PROGRESS: invalidate(); return 0;
    case WM_SCAN_DONE: scan_finish(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hw, &ps); paint(dc); EndPaint(hw, &ps); return 0; }
    case WM_SIZE: invalidate(); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mi = (MINMAXINFO*)lp;
        mi->ptMinTrackSize.x = S(860); mi->ptMinTrackSize.y = S(600); return 0; }
    case 0x02E0: {   // WM_DPICHANGED
        g_s = HIWORD(wp) / 96.0; make_fonts();
        RECT* r = (RECT*)lp;
        SetWindowPos(hw, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        invalidate(); return 0; }
    case WM_TIMER:
        if (wp == 2) { invalidate(); return 0; }   // shader scan progress animation
        KillTimer(hw, 1); g_toast.clear(); invalidate(); return 0;
    case WM_MOUSEMOVE: {
        const float x = (float)GET_X_LPARAM(lp), y = (float)GET_Y_LPARAM(lp);
        TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, hw, 0 }; TrackMouseEvent(&tme);
        if (g_drag) { slider_set(g_drag, x); invalidate(); return 0; }
        if (g_dd.open) { const int h = dd_hit(x, y); if (h != g_dd.hot) { g_dd.hot = h; invalidate(); } return 0; }
        layout(); set_hot(hit(x, y));
        return 0; }
    case WM_MOUSELEAVE: if (!g_drag) set_hot(-1); return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN: {
        const float x = (float)GET_X_LPARAM(lp), y = (float)GET_Y_LPARAM(lp);
        layout();
        if (g_cap_a != -1) {
            // a click on the box being set binds that button; a click anywhere else cancels
            int i = hit(x, y);
            const bool on_box = i >= 0 && g_w[i].t == W_KEY && g_w[i].id == g_cap_a && g_w[i].sub == g_cap_k;
            if (!on_box && msg == WM_LBUTTONDOWN) { g_cap_a = g_cap_k = -1; invalidate(); return 0; }
            capture_done(msg == WM_LBUTTONDOWN ? "MOUSE1" : msg == WM_RBUTTONDOWN ? "MOUSE2" : msg == WM_MBUTTONDOWN ? "MOUSE3"
                         : GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? "MOUSE4" : "MOUSE5");
            return msg == WM_XBUTTONDOWN ? TRUE : 0;
        }
        if (g_dd.open) {
            const int h = dd_hit(x, y);
            if (h >= 0 && msg == WM_LBUTTONDOWN) dd_set(g_dd.kind, g_dd.idx, h);
            close_dropdown(); invalidate(); return 0;
        }
        const int i = hit(x, y);
        if (msg == WM_RBUTTONDOWN && i >= 0 && g_w[i].t == W_KEY) {     // right-click clears
            if (g_w[i].id == -2) { g_mrelease = "F8"; toast("The cursor key can't be empty: it's back to F8."); }
            else g_bind[g_w[i].id][g_w[i].sub].clear();
            ctl_save(); invalidate(); return 0;
        }
        if (msg == WM_LBUTTONDOWN && i >= 0) {
            if (g_w[i].t == W_PLAY) { g_press = i; SetCapture(hw); invalidate(); return 0; }
            click(i, x, y);
        }
        return 0; }
    case WM_LBUTTONUP: {
        const float x = (float)GET_X_LPARAM(lp), y = (float)GET_Y_LPARAM(lp);
        if (g_drag) { const int d = g_drag; g_drag = 0; ReleaseCapture(); slider_commit(d); invalidate(); return 0; }
        if (g_press >= 0) {
            const int p = g_press; g_press = -1; ReleaseCapture(); layout();
            if (hit(x, y) == p) { click(p, x, y); return 0; }
            invalidate();
        }
        return 0; }
    case WM_MOUSEWHEEL: {
        const int d = GET_WHEEL_DELTA_WPARAM(wp);
        if (g_cap_a != -1) { capture_done(d > 0 ? "WHEELUP" : "WHEELDOWN"); return 0; }
        if (g_dd.open) return 0;
        scroll_by(-d / 120.0f * SF(60));
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; ScreenToClient(hw, &pt);
        layout(); set_hot(hit((float)pt.x, (float)pt.y));
        return 0; }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        if (g_cap_a != -1) {
            const char* n = vk_name((int)wp);
            if (n) capture_done(n);
            return 0;
        }
        if (wp == VK_ESCAPE && g_dd.open) { close_dropdown(); invalidate(); return 0; }
        if (wp == VK_PRIOR) { scroll_by(-g_view.Height * 0.8f); return 0; }
        if (wp == VK_NEXT)  { scroll_by(g_view.Height * 0.8f); return 0; }
        if (wp == VK_HOME)  { scroll_by(-1e6f); return 0; }
        if (wp == VK_END)   { scroll_by(1e6f); return 0; }
        if (wp == VK_RETURN && !g_dd.open) { play(); return 0; }
        if (msg == WM_SYSKEYDOWN) break;
        return 0; }
    case WM_SYSKEYUP: case WM_SYSCHAR: if (g_cap_a != -1) return 0; break;
    case WM_SYSCOMMAND: if ((wp & 0xFFF0) == SC_KEYMENU && g_cap_a != -1) return 0; break;   // F10 / Alt while capturing
    case WM_KILLFOCUS: if (g_cap_a != -1) { g_cap_a = g_cap_k = -1; invalidate(); } if (g_dd.open) { close_dropdown(); invalidate(); } return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

static void dark_titlebar(HWND hw)
{
    BOOL on = TRUE;
    if (FAILED(DwmSetWindowAttribute(hw, 20, &on, sizeof on)))   // DWMWA_USE_IMMERSIVE_DARK_MODE
        DwmSetWindowAttribute(hw, 19, &on, sizeof on);            // older Windows 10 builds
    COLORREF cap = RGB(17, 18, 20);
    DwmSetWindowAttribute(hw, 35, &cap, sizeof cap);              // DWMWA_CAPTION_COLOR (Windows 11)
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmd, int show)
{
    // per-monitor DPI awareness, so text is sharp at 125% / 150% scaling
    typedef BOOL (WINAPI *SetCtx)(HANDLE);
    if (SetCtx f = (SetCtx)(void*)GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext"))
        f((HANDLE)-4);   // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    else SetProcessDPIAware();

    GetModuleFileNameA(NULL, g_dir, MAX_PATH);
    if (char* sl = strrchr(g_dir, '\\')) *sl = 0;
    lx_init_dirs();                         // launcher_extra_r1.inc: build_feat\Launcher.exe -> the port folder
    dist_init();                            // launcher_dist.inc: "updates" option, Wine / Deck choices
    lx_register();                          // launcher_extra_r1.inc: the extras' settings (after "updates")
    // test hook: "Launcher.exe --resave" reads settings.ini and writes settings.ini and settings.bat again (no window)
    if (cmd && strstr(cmd, "--resave")) { load_settings(); return save_settings() ? 0 : 1; }
    load_settings();
    detect_hardware();
    if (g_first_run) apply_recommended();   // first start: this PC's recommended settings
    ctl_load();

    GdiplusStartupInput gsi; GdiplusStartup(&g_gdip, &gsi, NULL);
    load_art();

    WNDCLASSEXA wc = { sizeof wc };
    wc.lpfnWndProc = wndproc; wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1)); if (!wc.hIcon) wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.lpszClassName = "RfomLauncher2";
    RegisterClassExA(&wc);

    make_scale(NULL);
    // 940 x 640 (logical), shrunk to fit the work area
    RECT wa; SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, S(940), S(640) };
    AdjustWindowRect(&r, style, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    ww = std::min(ww, (int)(wa.right - wa.left)); wh = std::min(wh, (int)(wa.bottom - wa.top));
    g_hw = CreateWindowExA(0, wc.lpszClassName, "Resistance: Fall of Man", style,
                           wa.left + (wa.right - wa.left - ww) / 2, wa.top + (wa.bottom - wa.top - wh) / 2,
                           ww, wh, NULL, NULL, inst, NULL);
    make_scale(g_hw);
    dark_titlebar(g_hw);
    // test hook: "Launcher.exe --page N" opens on that page (used for screenshots)
    if (cmd && strstr(cmd, "--page ")) { int p = atoi(strstr(cmd, "--page ") + 7); if (p >= 0 && p < P_COUNT) g_page = p; }
    if (cmd && strstr(cmd, "--scroll ")) g_scroll[g_page] = (float)atof(strstr(cmd, "--scroll ") + 9);
    ShowWindow(g_hw, show);
    UpdateWindow(g_hw);
    dist_start(g_hw);                       // launcher_dist.inc: update check, Deck controller
    port_report_start(g_hw, g_dir);         // port_report.cpp

    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageA(&m); }
    delete g_art_scaled; delete g_art;   // GDI+ objects go before GDI+ shuts down
    GdiplusShutdown(g_gdip);
    return 0;
}
