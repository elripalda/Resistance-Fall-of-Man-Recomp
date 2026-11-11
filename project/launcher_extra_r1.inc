// launcher_extra_r1.inc -- PC extras for the launcher (included by launcher.cpp):
//   * the options behind the game's "More PC Options" menu (rfom_pcextra.cpp): both write the same
//     settings.ini keys and settings.bat variables,
//   * controller button layout (PAD_MAP_*, PAD_SOUTHPAW; libs/input/cellPad.c XInput hook),
//   * the "Saves & tools" page: save list, back up / restore / open, automatic backup before Play
//     (keeps the newest N), and the built-in benchmark (RFOM_BENCH=1).
// Extra settings are appended to g_set after launcher_dist.inc's "updates" (so S_UPDATES stays S_COUNT);
// lx_write_bat() adds their settings.bat lines.
// Needs from launcher.cpp: g_set, Setting, Choice, path_of, toast, add, deco, SF, layout_setting_row, g_hw, g_dir.

static void play();
static std::string saves_folder(bool* exists);

struct LxOpt { int si; const char* env; int page; };     // page: where the option was first added (0 Display, 1 Graphics,
                                                         // 2 Controls, 3 shader cache, 4 upscaling, 5 audio, 6 gameplay,
                                                         // 7 mouse); the pages it is shown on come from launcher_cats.inc
static std::vector<LxOpt> g_lx;
static void lx_add(int page, const char* key, const char* title, const char* help, const char* env,
                   std::vector<Choice> ch, int def = 0)
{
    g_set.push_back({ key, title, help, std::move(ch), def, def });
    g_lx.push_back({ (int)g_set.size() - 1, env, page });
}

// A launcher started from a build subfolder (build_feat\Launcher.exe) works on the port folder above it and
// starts the programs next to it: run.bat uses RFOM_BUILD_DIR instead of build\.
static void lx_init_dirs()
{
    if (GetFileAttributesA(path_of("run.bat").c_str()) != INVALID_FILE_ATTRIBUTES) return;
    std::string here = g_dir, up = here.substr(0, here.find_last_of('\\'));
    if (up.empty() || GetFileAttributesA((up + "\\run.bat").c_str()) == INVALID_FILE_ATTRIBUTES) return;
    if (GetFileAttributesA((here + "\\ResistanceFoM.exe").c_str()) != INVALID_FILE_ATTRIBUTES)
        SetEnvironmentVariableA("RFOM_BUILD_DIR", here.c_str());
    snprintf(g_dir, MAX_PATH, "%s", up.c_str());
}

static void lx_register()
{
    const std::vector<Choice> on_def = { {"On", ""}, {"Off", "0"} };
    // "WINDOW, HDR AND EXTRAS" options
    lx_add(0, "resizable", "Resizable window",
           "Windowed mode: drag the window edges (or maximize) to resize it; the picture follows the new size.",
           "RSX_RESIZABLE", on_def);
    lx_add(0, "altenter", "Alt+Enter",
           "Alt+Enter switches between windowed and borderless fullscreen in game.", "RSX_ALT_ENTER", on_def);
    lx_add(0, "skipintro", "Skip intro movies",
           "Skips the opening intro movie if the game plays one when it starts (the same as pressing Start through it). "
           "The game has no logo videos to skip.", "RFOM_SKIP_INTRO", on_def);
    lx_add(0, "cutscene_skip", "Hold to skip movies",
           "Hold Space / Enter / A / Menu for a second to skip a story movie (the game's own skip).",
           "RFOM_CUTSCENE_SKIP", on_def);
    // rfom_pcextra.cpp: the "Hold Space / A to skip" notification when a movie starts (skipping is cutscene_skip)
    lx_add(0, "skiphint", "Show cutscene skip hint",
           "A short \"Hold Space / A to skip\" notification in the corner when a movie starts. Off hides it; "
           "holding the button still skips (Hold to skip movies).", "PORT_SKIP_HINT", on_def);
    // rfom_trophies.cpp
    lx_add(0, "trophytoast", "Achievement notifications",
           "A small notification with the grade badge in the corner of the game window when you earn an achievement "
           "(unofficial trophy). Off hides it; achievements are still recorded (see the Achievements page).",
           "RFOM_TROPHY_TOAST", on_def);
    lx_add(0, "hdr", "HDR output",
           "Experimental. Needs Windows HDR on (Settings > Display > Use HDR). Draws the final image into an HDR10 (PQ) "
           "or scRGB swap chain: the game's SDR picture stays at the paper-white brightness below and only highlights "
           "are expanded towards the peak. Auto turns HDR on only when Windows HDR is on for the game's display. Falls "
           "back to SDR otherwise. Screenshots are saved as normal SDR PNGs. Pros: brighter highlights and deeper "
           "contrast. Cons: needs a real HDR display; wrong brightness settings look washed out or clip highlights.",
           "RFOM_HDR", { {"Off", ""}, {"Auto", "auto"}, {"On  (HDR10)", "1"}, {"On  (scRGB)", "scrgb"} });
    lx_add(0, "hdr_paper", "HDR paper white",
           "How bright the HUD, menus and the game's normal image are in HDR, in nits (203 is the HDR reference white). "
           "Lower for a dark room, higher for a bright one.", "RFOM_HDR_PAPER",
           { {"203 nits", ""}, {"100 nits", "100"}, {"160 nits", "160"}, {"250 nits", "250"}, {"300 nits", "300"},
             {"400 nits", "400"} });
    lx_add(0, "hdr_peak", "HDR peak brightness",
           "How bright the brightest highlights get, in nits (also sent to the display as HDR metadata). Display = the "
           "peak Windows reports for the monitor. Set it equal to paper white to turn highlight expansion off.",
           "RFOM_HDR_PEAK",
           { {"Display", ""}, {"400 nits", "400"}, {"600 nits", "600"}, {"800 nits", "800"}, {"1000 nits", "1000"},
             {"1500 nits", "1500"}, {"2000 nits", "2000"} });
    lx_add(0, "hdr_sat", "HDR saturation",
           "Colour saturation of the HDR picture (1.0 = as the game).", "RFOM_HDR_SAT",
           { {"1.0", ""}, {"0.9", "0.9"}, {"1.1", "1.1"}, {"1.2", "1.2"} });
    lx_add(0, "reflex", "NVIDIA Reflex / low latency",
           "Cuts input lag when the GPU is the limit. On an NVIDIA GPU this is NVIDIA Reflex Low Latency (On + Boost "
           "also keeps the GPU clocks up); on other GPUs it keeps at most one frame queued (a waitable swap chain). "
           "It is not a frame limiter - the Frame rate limit still decides the cap.", "RFOM_REFLEX",
           { {"Off", ""}, {"On", "1"}, {"On + Boost", "2"} });
    lx_add(0, "photo", "Photo mode  (F7)",
           "F7 in game pauses, hides the port's overlays and lets you take clean F12 screenshots. No free camera "
           "(see the README).", "RFOM_PHOTO", on_def);
    lx_add(0, "discord", "Discord Rich Presence",
           "Shows \"Playing Resistance: Fall of Man\" in your Discord status. Needs the Discord app running and an "
           "application id in discord_appid.txt (or the RFOM_DISCORD_APPID variable); does nothing otherwise.",
           "RFOM_DISCORD", { {"Off", ""}, {"On", "1"} });
    lx_add(0, "savebackup", "Back up saves before playing",
           "Play copies your saved games to save_backups (next to the saves) first, only when they changed, keeping "
           "the newest ones. See the Saves & tools page.", "RFOM_SAVE_BACKUP",
           { {"On  (keep 10)", ""}, {"On  (keep 5)", "5"}, {"On  (keep 30)", "30"}, {"Off", "0"} });
    lx_add(0, "overlay_pos", "Overlay corner",
           "Which corner of the game window the F3 performance overlay sits in. Notifications (screenshot saved, "
           "movie skip hint) appear in the opposite top corner. Both are drawn into the game's own image, so they "
           "show in every display mode and in recordings, but never in F12 screenshots.", "RFOM_OVERLAY_POS",
           { {"Top left", ""}, {"Top right", "tr"}, {"Bottom left", "bl"}, {"Bottom right", "br"} });
    lx_add(0, "seamless", "Seamless level loads",
           "The game swaps to another program at some level loads; On holds a black cover over the window meanwhile, "
           "so the game window does not close and reopen. Pros: no flashing window or desktop between levels. "
           "Cons: none known; Off is the old behaviour if the window ever stays black.",
           "RFOM_SEAMLESS", { {"On", ""}, {"Off", "0"} });
    // "AUDIO" options (stubs.cpp rfom_speakers, cellAudio.c audio_buffer_ms)
    lx_add(5, "speakers", "Speaker layout",
           "The channel layout the game mixes for. Windows down-mixes to your real speakers or headphones. "
           "Pros of 5.1 / 7.1: proper surround on a surround system. Cons: on plain stereo Stereo can sound a little "
           "fuller, since nothing is down-mixed.", "RFOM_SPEAKERS",
           { {"5.1 surround", ""}, {"7.1 surround", "8"}, {"Stereo", "2"} });
    lx_add(5, "audio_buffer", "Audio buffer",
           "How much sound is buffered ahead. Auto is 100 ms (150 ms on CPUs with fewer than 12 threads). "
           "Pros of a larger buffer: no crackles or stretching when the CPU is busy. Cons: sound lags the picture a "
           "little more. Smaller: tighter sound, but may crackle on a busy CPU.", "PORT_AUDIO_BUFFER_MS",
           { {"Auto", ""}, {"80 ms", "80"}, {"100 ms", "100"}, {"150 ms", "150"}, {"200 ms", "200"} });
    // "GAMEPLAY" options (stubs.cpp)
    lx_add(6, "gore", "Blood and gore",
           "Full keeps the game's full-gore setting (blood), whatever region the console settings say. Console "
           "region leaves it to the game, which may pick reduced gore.", "RFOM_GORE",
           { {"Full", ""}, {"As the console region", "0"} });
    lx_add(6, "dyndiff", "Adaptive difficulty",
           "The PS3 game quietly makes enemies a little less deadly the longer you are stuck in a section and the "
           "more you die there. Pros of On: the original experience, hard spots ease up. Cons: the difficulty you "
           "picked is not what you get; Off keeps it fixed.", "RFOM_DYNDIFF",
           { {"On  (PS3 original)", ""}, {"Off  (fixed difficulty)", "0"} });
    // "MORE MOUSE OPTIONS" options
    lx_add(7, "grab_mouse", "Shake off grabs with the mouse",
           "When an enemy grabs you, shaking the mouse breaks free (scaled so vigorous shaking matches shaking both "
           "sticks). Pros: works with keyboard & mouse, where the movement keys barely counted. Cons: none; Off uses "
           "the game's own stick meter. A controller is not affected.", "RFOM_GRAB_MOUSE",
           { {"On", ""}, {"Off", "0"} });
    // "ACCESSIBILITY" options
    lx_add(1, "colorblind", "Colour-blind filter",
           "Shifts colours so they are easier to tell apart (daltonization of the final image). Also in game: "
           "F10 > More PC Options, or F9.", "RFOM_COLORBLIND",
           { {"Off", ""}, {"Protanopia  (red)", "1"}, {"Deuteranopia  (green)", "2"}, {"Tritanopia  (blue)", "3"} });
    lx_add(1, "crosshair", "Extra crosshair",
           "A small dot or cross at the screen centre while on foot, for when the game's reticle is hard to see.",
           "RFOM_CROSSHAIR", { {"Off", ""}, {"Dot", "dot"}, {"Cross", "cross"} });
    // "UPSCALING" options (rsx_fsr.inc)
    lx_add(4, "objmv", "Object motion vectors",
           "DLSS / FSR: On gives moving characters and objects their own motion. Pros: sharper moving objects, less "
           "ghosting and smearing. Cons: a small GPU cost. Off uses camera motion only: try it if moving objects "
           "smear, shuffle or stutter.",
           "RSX_DLSS_OBJMV", { {"On", "1"}, {"Off", "0"} });
    lx_add(4, "dlss_preset", "DLSS model",
           "Which DLSS model NVIDIA DLSS uses (only with Anti-aliasing = NVIDIA DLSS / DLAA on an RTX GPU). Default lets "
           "the driver choose. Pros of K (transformer): sharper, more stable detail and less ghosting. Cons: a "
           "little more GPU time than J (the older CNN model), which is cheaper but softer in motion.",
           "RSX_DLSS_PRESET", { {"NVIDIA default", ""}, {"K  (transformer)", "11"}, {"J  (CNN)", "10"} });
    lx_add(4, "dlss_fg", "Frame Generation",
           "Frame generation: raises smoothness when the CPU is the limit, adds a little input latency, needs an "
           "RTX 40/50; 4x is for high-refresh displays. DLSS choices need Anti-aliasing = NVIDIA DLSS (turns Reflex on; "
           "3x / 4x need an RTX 50, an RTX 40 uses 2x); without DLSS Frame Generation they fall back to FSR 3 2x. "
           "FSR 3 2x (AMD, open source) works on other GPUs too, with any temporal anti-aliasing (FSR 3.1, DLSS or "
           "XeSS). The HUD is kept out of the generated frames. Set the Frame rate limit to the rendered rate you "
           "want (144 Hz: about 69 for 2x, 46 for 3x, 34 for 4x).",
           "RFOM_DLSS_FG", { {"Off", ""}, {"DLSS 2x", "2"}, {"DLSS 3x", "3"}, {"DLSS 4x", "4"}, {"FSR 3 2x", "fsr3"} });
    lx_add(4, "fsr_sharp", "FSR 3.1 sharpening",
           "FSR 3.1's own sharpening (RCAS), only when FSR 3.1 is the anti-aliasing / upscaler. Separate from the "
           "Sharpening setting above, which works on the final picture. Pros: crisper textures from a lower render "
           "share. Cons: high values give halos and grain.", "RSX_FSR_SHARP",
           { {"Low  (0.25)", ""}, {"Off", "0"}, {"Medium  (0.5)", "0.5"}, {"High  (0.75)", "0.75"},
             {"Max  (1.0)", "1"} });
    lx_add(4, "lod_bias", "Texture detail (LOD bias)",
           "Auto sharpens texture mip selection to match the upscaler's output resolution, capped at your monitor's "
           "resolution (for example -1 at Performance; -0.6 for Ultra Performance at 4K shown on a 1080p screen) and "
           "leaves it alone without upscaling. Negative = sharper textures. Pros: crisper distant "
           "textures. Cons: too negative shimmers and aliases in motion; positive is blurrier.", "RSX_LOD_BIAS",
           { {"Auto", ""}, {"0  (as the game)", "0"}, {"-0.5", "-0.5"}, {"-1", "-1"}, {"-1.5", "-1.5"},
             {"+0.5  (softer)", "0.5"} });
    lx_add(4, "shadowscale", "Shadow map resolution",
           "Render scale of the shadow maps, separate from the main resolution (never above it). Auto = the main "
           "resolution up to 2x. Pros of lower: much less GPU time and VRAM in shadowed scenes at 3x/4x, where the "
           "shadow passes were drawn at the full supersampling factor. Cons: slightly softer, more stepped shadow "
           "edges. Same as resolution = the old behaviour (sharpest, slowest).", "RFOM_SHADOW_SCALE",
           { {"Auto  (up to 2x)", ""}, {"1x  (PS3)", "1"}, {"1.5x", "1.5"}, {"2x", "2"}, {"3x", "3"},
             {"Same as resolution", "8"} });
    // "SHADER CACHE" options (rsx_d3d12_backend.c rsx_pso_precompile_boot)
    lx_add(3, "precompile", "Compile shaders before playing",
           "Builds every graphics pipeline the game has used on this PC (the list in shadercache) while the game "
           "starts, before the first frame, so effects seen before never stutter in play. A \"Compiling shaders N / M\" "
           "panel shows the progress; it takes a few seconds, longer after a driver update or Rebuild shader cache. "
           "Off: they are built in the background while you play (can hitch briefly). After a GPU or driver change "
           "they are always rebuilt before play. Pros: no shader stutter for anything compiled. Cons: longer start-up "
           "(seconds normally, about a minute after a driver update or the first shader scan).",
           "RSX_PSO_PRECOMPILE", { {"On", "1"}, {"Off", ""} });
    lx_add(3, "precompile_threads", "Shader compile threads",
           "CPU threads used by Compile shaders before playing. Auto uses every logical core but one.",
           "RSX_PSO_PRECOMPILE_THREADS",
           { {"Auto  (all cores but one)", ""}, {"2", "2"}, {"4", "4"}, {"6", "6"}, {"8", "8"}, {"12", "12"}, {"16", "16"} });
    // "CONTROLLER BUTTONS" options
    lx_add(2, "pad_disconnect_pause", "Pause when the controller disconnects",
           "Pauses the game and shows a message when the controller you play with is unplugged or runs out of battery. "
           "Reconnect it, or press Enter, to continue.", "RFOM_PAD_DISCONNECT_PAUSE", on_def);
    lx_add(2, "pad_southpaw", "Swap sticks  (southpaw)", "Left stick looks, right stick moves.", "PAD_SOUTHPAW",
           { {"Off", ""}, {"On", "1"} });
    lx_add(2, "pad_p1", "Controller to play with",
           "With several controllers connected, which one is player 1 (in the order Windows connected them). Needs "
           "Controller support = SDL3 (the default); with XInput the first one is used. Pros: "
           "pick your pad without unplugging the others. Cons: the number can change when controllers reconnect.",
           "PAD_P1_CONTROLLER",
           { {"First connected", ""}, {"Controller 1", "1"}, {"Controller 2", "2"}, {"Controller 3", "3"},
             {"Controller 4", "4"} });
    static const char* keys[] = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS", "BACK", "START" };
    static const char* names[] = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "Left stick click", "Right stick click",
                                   "View", "Menu" };
    for (int i = 0; i < 12; i++) {
        std::vector<Choice> ch = { { "Default", "" } };
        for (int j = 0; j < 12; j++) {
            if (j == i) continue;
            char* l = (char*)malloc(48); snprintf(l, 48, "Acts as %s", names[j]);
            ch.push_back({ l, keys[j] });
        }
        char* key = (char*)malloc(24); snprintf(key, 24, "padmap_%s", keys[i]);
        for (char* c = key; *c; c++) *c = (char)tolower((unsigned char)*c);
        char* env = (char*)malloc(24); snprintf(env, 24, "PAD_MAP_%s", keys[i]);
        char* title = (char*)malloc(48); snprintf(title, 48, "Button %s", names[i]);
        lx_add(2, key, title,
               "What this controller button does in game (Xbox names; a PlayStation pad through Steam Input / DS4Windows "
               "maps the same way). Swapping two buttons needs both rows. Also in game: F10 > More PC Options > "
               "Controller layout.", env, std::move(ch));
    }
}

static void lx_write_bat(FILE* f)
{
    for (auto& o : g_lx) {
        const char* v = g_set[o.si].choices[g_set[o.si].sel].value;
        if (*v) fprintf(f, "set \"%s=%s\"\n", o.env, v);
    }
}
static const char* lx_val(const char* key)
{
    for (auto& s : g_set) if (!strcmp(s.key, key)) return s.choices[s.sel].value;
    return "";
}

// ---- saves -----------------------------------------------------------------------------------------
static std::string lx_save_dir()
{
    bool ex = false; std::string p = saves_folder(&ex);
    return ex ? p : p + "\\dev_hdd0\\home\\00000001\\savedata";
}
// next to dev_hdd0 (Saved Games\Resistance Fall of Man, or gamedata when portable), so it survives a reinstall
static std::string lx_backup_root()
{
    std::string sd = lx_save_dir();
    const std::string tail = "\\dev_hdd0\\home\\00000001\\savedata";
    if (sd.size() > tail.size()) sd.resize(sd.size() - tail.size());
    return sd + "\\save_backups";
}
struct LxEntry { std::string name, title; ULONGLONG newest = 0, bytes = 0; int files = 0; };
static void lx_scan(const std::string& dir, LxEntry& e)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { lx_scan(dir + "\\" + fd.cFileName, e); continue; }
        const ULONGLONG t = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
        e.newest = std::max(e.newest, t);
        e.bytes += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        e.files++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
// PARAM.SFO: SUB_TITLE (or TITLE) of a PS3 save
static std::string lx_sfo_title(const std::string& dir)
{
    FILE* f = fopen((dir + "\\PARAM.SFO").c_str(), "rb");
    if (!f) return "";
    std::vector<unsigned char> b(65536);
    const size_t n = fread(b.data(), 1, b.size(), f); fclose(f);
    if (n < 20 || memcmp(b.data(), "\0PSF", 4)) return "";
    auto u32 = [&](size_t o) { return o + 4 <= n ? (unsigned)b[o] | b[o + 1] << 8 | b[o + 2] << 16 | (unsigned)b[o + 3] << 24 : 0u; };
    auto u16 = [&](size_t o) { return o + 2 <= n ? (unsigned)(b[o] | b[o + 1] << 8) : 0u; };
    const unsigned kt = u32(8), dt = u32(12), cnt = u32(16);
    std::string sub, title;
    for (unsigned i = 0; i < cnt && i < 64; i++) {
        const size_t ent = 20 + i * 16;
        const size_t ko = kt + u16(ent), dof = dt + u32(ent + 12), len = u32(ent + 4);
        if (ko >= n || dof + len > n) continue;
        const char* k = (const char*)&b[ko];
        std::string v((const char*)&b[dof], strnlen((const char*)&b[dof], len));
        if (!strcmp(k, "SUB_TITLE")) sub = v; else if (!strcmp(k, "TITLE")) title = v;
    }
    return !sub.empty() ? sub : title;
}
static std::vector<LxEntry> lx_list(const std::string& root, bool saves)
{
    std::vector<LxEntry> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((root + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == '.') continue;
        LxEntry e; e.name = fd.cFileName;
        lx_scan(root + "\\" + e.name, e);
        if (saves) e.title = lx_sfo_title(root + "\\" + e.name);
        out.push_back(e);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [&](const LxEntry& a, const LxEntry& b) { return saves ? a.name < b.name : a.name > b.name; });
    return out;
}
static bool lx_copy_tree(const std::string& from, const std::string& to)
{
    CreateDirectoryA(to.c_str(), NULL);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((from + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        const std::string a = from + "\\" + fd.cFileName, b = to + "\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ok = lx_copy_tree(a, b) && ok;
        else ok = CopyFileA(a.c_str(), b.c_str(), FALSE) && ok;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return ok;
}
static void lx_delete_tree(const std::string& dir)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            const std::string p = dir + "\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) lx_delete_tree(p);
            else { SetFileAttributesA(p.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileA(p.c_str()); }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}
// a backup is save_backups\<YYYY-MM-DD_HHMMSS>_<kind>\<the savedata folders>; kind = auto / manual / before-restore
static std::string lx_backup(const char* kind)
{
    LxEntry cur; lx_scan(lx_save_dir(), cur);
    if (!cur.files) return "";
    SHCreateDirectoryExA(NULL, lx_backup_root().c_str(), NULL);
    SYSTEMTIME t; GetLocalTime(&t);
    char name[96];
    snprintf(name, sizeof name, "%04u-%02u-%02u_%02u%02u%02u_%s", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, kind);
    const std::string dst = lx_backup_root() + "\\" + name;
    if (!lx_copy_tree(lx_save_dir(), dst)) { lx_delete_tree(dst); return ""; }
    return name;
}
// before Play: back up when the saves changed since the newest backup; keep the newest N automatic ones
static void lx_auto_backup()
{
    const char* v = lx_val("savebackup");
    if (!strcmp(v, "0")) return;
    const int keep = *v ? std::max(1, atoi(v)) : 10;
    LxEntry cur; lx_scan(lx_save_dir(), cur);
    if (!cur.files) return;
    auto bk = lx_list(lx_backup_root(), false);
    for (auto& b : bk) if (b.newest >= cur.newest && b.files == cur.files && b.bytes == cur.bytes) return;   // unchanged
    lx_backup("auto");
    bk = lx_list(lx_backup_root(), false);      // newest first
    int n = 0;
    for (auto& b : bk) {
        if (b.name.size() < 5 || b.name.compare(b.name.size() - 5, 5, "_auto") != 0) continue;
        if (++n > keep) lx_delete_tree(lx_backup_root() + "\\" + b.name);
    }
}
static std::vector<LxEntry> g_lx_saves, g_lx_backups;
static DWORD g_lx_loaded = 0;
static void lx_refresh(bool force)
{
    if (!force && g_lx_loaded && GetTickCount() - g_lx_loaded < 3000) return;
    g_lx_loaded = GetTickCount();
    g_lx_saves = lx_list(lx_save_dir(), true);
    g_lx_backups = lx_list(lx_backup_root(), false);
}
static std::string lx_when(ULONGLONG ft)
{
    if (!ft) return "";
    FILETIME f = { (DWORD)ft, (DWORD)(ft >> 32) }, l; SYSTEMTIME s;
    FileTimeToLocalFileTime(&f, &l); FileTimeToSystemTime(&l, &s);
    char b[64]; snprintf(b, sizeof b, "%04u-%02u-%02u %02u:%02u", s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute);
    return b;
}
static std::string lx_size(ULONGLONG b) { char s[32]; snprintf(s, sizeof s, b > 1048576 ? "%.1f MB" : "%.0f KB", b > 1048576 ? b / 1048576.0 : b / 1024.0); return s; }

// ---- benchmark ---------------------------------------------------------------------------------------
static std::string lx_last_bench()
{
    WIN32_FIND_DATAA fd; std::string best; FILETIME bt = {};
    HANDLE h = FindFirstFileA(path_of("logs\\bench\\RFOM_bench_*.txt").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return "";
    do { if (CompareFileTime(&fd.ftLastWriteTime, &bt) > 0) { bt = fd.ftLastWriteTime; best = fd.cFileName; } } while (FindNextFileA(h, &fd));
    FindClose(h);
    FILE* f = fopen(path_of(("logs\\bench\\" + best).c_str()).c_str(), "r");
    if (!f) return "";
    std::string avg, low, usd, line; char b[256];
    while (fgets(b, sizeof b, f)) {
        line = b; line.erase(line.find_last_not_of("\r\n ") + 1);
        auto v = [&](const char* k) { return line.compare(0, strlen(k), k) == 0 ? line.substr(line.find_first_not_of(' ', strlen(k))) : std::string(); };
        if (!v("avg fps").empty()) avg = v("avg fps");
        if (!v("1% low fps").empty()) low = v("1% low fps");
        if (!v("us/draw").empty()) usd = v("us/draw");
    }
    fclose(f);
    return "Last run (" + lx_when(((ULONGLONG)bt.dwHighDateTime << 32) | bt.dwLowDateTime) + "):  " + avg + " fps average,  " +
           low + " fps 1% low,  " + usd + " \xC2\xB5s per draw";
}

enum { BT_X_BACKUP = 100, BT_X_OPEN_SAVES, BT_X_OPEN_BACKUPS, BT_X_BENCH, BT_X_BENCH_LOGS, BT_X_RESTORE, BT_X_DELETE };

static void lx_layout_tools(float x, float& y, float w)
{
    lx_refresh(false);
    deco(D_H1, RectF(x, y, w, SF(34)), "Saves & tools"); y += SF(36);
    deco(D_INTRO, RectF(x, y, w, SF(20)), "Back up and restore saved games, and measure performance.");
    y += SF(40);
    deco(D_H2, RectF(x, y, w, SF(22)), "SAVED GAMES"); y += SF(30);
    if (g_lx_saves.empty()) { deco(D_NOTE, RectF(x, y, w, SF(22)), "No saved games yet."); y += SF(30); }
    for (size_t i = 0; i < g_lx_saves.size(); i++) {
        const LxEntry& e = g_lx_saves[i];
        Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(34)), -300 - (int)i);
        row.title = e.name; row.help = e.title.empty() ? e.name : e.title + "  (" + e.name + ")";
        deco(D_LABEL, RectF(x, y, w - SF(260), SF(34)), e.title.empty() ? e.name : e.title);
        deco(D_LABEL_DIM, RectF(x + w - SF(250), y, SF(250), SF(34)), lx_when(e.newest) + "  \xC2\xB7  " + lx_size(e.bytes));
        y += SF(36);
    }
    y += SF(8);
    Widget& b1 = add(W_BUTTON, RectF(x, y, SF(150), SF(38)), BT_X_BACKUP);
    b1.title = "Back up now"; b1.help = "Copies all saved games to save_backups (kept until you delete them).";
    Widget& b2 = add(W_BUTTON, RectF(x + SF(162), y, SF(170), SF(38)), BT_SAVES);
    b2.title = "Open save folder"; b2.help = "The game's save folder (see Display \xE2\x80\xBA Save location).";
    Widget& b3 = add(W_BUTTON, RectF(x + SF(344), y, SF(190), SF(38)), BT_X_OPEN_BACKUPS);
    b3.title = "Open backups folder"; b3.help = lx_backup_root();
    y += SF(56);
    deco(D_H2, RectF(x, y, w, SF(22)), "BACKUPS"); y += SF(30);
    if (g_lx_backups.empty()) { deco(D_NOTE, RectF(x, y, w, SF(22)), "No backups yet. Play makes one automatically (Advanced page, "
                                                                     "\"Back up saves before playing\")."); y += SF(30); }
    for (size_t i = 0; i < g_lx_backups.size() && i < 40; i++) {
        const LxEntry& e = g_lx_backups[i];
        Widget& row = add(W_ROW, RectF(x - SF(12), y, w + SF(24), SF(40)), -400 - (int)i);
        row.title = e.name; row.help = "Backup " + e.name + ": " + std::to_string(e.files) + " files, " + lx_size(e.bytes) + ".";
        deco(D_LABEL, RectF(x, y, w - SF(230), SF(40)), e.name);
        Widget& r = add(W_BUTTON, RectF(x + w - SF(220), y + SF(4), SF(110), SF(32)), BT_X_RESTORE, (int)i);
        r.title = "Restore"; r.help = "Replaces your current saved games with this backup (they are backed up first).";
        Widget& d = add(W_BUTTON, RectF(x + w - SF(100), y + SF(4), SF(100), SF(32)), BT_X_DELETE, (int)i);
        d.title = "Delete"; d.help = "Deletes this backup.";
        y += SF(42);
    }
    y += SF(24);
    deco(D_H2, RectF(x, y, w, SF(22)), "BENCHMARK"); y += SF(30);
    deco(D_NOTE, RectF(x, y, w, SF(60)), "Starts the game with your settings and continues your campaign from the title "
                                         "screen by itself; in the level it waits 45 s, measures for 60 s, writes logs\\bench\\ "
                                         "and closes. Don't touch the keyboard or controller while it runs (about 5 minutes).");
    y += SF(66);
    Widget& b4 = add(W_BUTTON, RectF(x, y, SF(170), SF(38)), BT_X_BENCH);
    b4.title = "Run benchmark"; b4.help = "Average fps, 1% low fps and CPU time per draw call, to logs\\bench\\. Needs a saved "
                                          "campaign to continue (or set RFOM_BENCH_SCRIPT=none and start a level yourself).";
    Widget& b5 = add(W_BUTTON, RectF(x + SF(182), y, SF(170), SF(38)), BT_X_BENCH_LOGS);
    b5.title = "Open results"; b5.help = "logs\\bench\\: one report per run and bench_history.csv.";
    y += SF(46);
    const std::string last = lx_last_bench();
    if (!last.empty()) { deco(D_LABEL_DIM, RectF(x, y, w, SF(28)), last); y += SF(30); }
}

// returns true when the button was ours
static bool lx_click(int id, int sub)
{
    if (id == BT_X_BACKUP) {
        const std::string n = lx_backup("manual");
        toast(n.empty() ? "Nothing to back up (no saved games yet)." : "Saved games backed up to save_backups\\" + n);
        lx_refresh(true);
    } else if (id == BT_X_OPEN_BACKUPS) {
        SHCreateDirectoryExA(NULL, lx_backup_root().c_str(), NULL);
        ShellExecuteA(g_hw, "open", lx_backup_root().c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else if (id == BT_X_RESTORE || id == BT_X_DELETE) {
        lx_refresh(true);
        if (sub < 0 || sub >= (int)g_lx_backups.size()) return true;
        const std::string name = g_lx_backups[sub].name, src = lx_backup_root() + "\\" + name;
        if (id == BT_X_DELETE) {
            if (MessageBoxA(g_hw, ("Delete the backup " + name + "?").c_str(), "Saves", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                lx_delete_tree(src); toast("Backup deleted.");
            }
        } else if (MessageBoxA(g_hw, ("Replace your saved games with the backup " + name + "?\n\nYour current saves are backed "
                                      "up first (\"before-restore\"). Don't do this while the game is running.").c_str(),
                               "Restore saves", MB_YESNO | MB_ICONWARNING) == IDYES) {
            lx_backup("before-restore");
            const std::string sd = lx_save_dir();
            for (auto& s : lx_list(sd, true)) lx_delete_tree(sd + "\\" + s.name);
            SHCreateDirectoryExA(NULL, sd.c_str(), NULL);
            toast(lx_copy_tree(src, sd) ? "Saved games restored from " + name + "." : "Restore failed - see save_backups.");
        }
        lx_refresh(true);
    } else if (id == BT_X_BENCH) {
        if (MessageBoxA(g_hw, "Run the benchmark now?\n\nThe game starts, continues your campaign by itself, measures for 60 "
                              "seconds in the level and closes. Don't press anything while it runs.", "Benchmark",
                        MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return true;
        SetEnvironmentVariableA("RFOM_BENCH", "1");      // the game also turns pause-on-focus-loss off for the run
        play();
    } else if (id == BT_X_BENCH_LOGS) {
        CreateDirectoryA(path_of("logs").c_str(), NULL); CreateDirectoryA(path_of("logs\\bench").c_str(), NULL);
        ShellExecuteA(g_hw, "open", path_of("logs\\bench").c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else return false;
    return true;
}
