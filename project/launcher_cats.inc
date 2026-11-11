// Launcher categories: which options each settings page lists, and under which heading (the same pages as the
// Resistance 2 / 3 launchers).
//
// Options are looked up by their settings.ini key; "prefix*" matches every key that starts with prefix and a
// key the launcher does not have is skipped. "@name" is a control that is not a g_set option (the FOV slider,
// the mouse rows, the use button), drawn by layout_special() in launcher.cpp. Where an option is shown does not
// change what it does or what the reset buttons reset. Any option no group lists appears on the Advanced page
// under OTHER, so nothing can go missing.
struct CatGroup { int page; const char* head; const char* keys; const char* note; };
static const CatGroup g_cat[] = {
    { P_DISPLAY,  "SCREEN",                       "display window monitor resizable altenter aspect seamless", nullptr },
    { P_DISPLAY,  "FRAME RATE",                   "fps menufps vsync present", nullptr },
    { P_DISPLAY,  "HDR",                          "hdr*", nullptr },
    { P_GRAPHICS, "RESOLUTION",                   "res filter", nullptr },
    { P_GRAPHICS, "QUALITY",                      "aniso lod_bias shadowscale", nullptr },
    { P_GRAPHICS, "VIEW",                         "@fov", nullptr },
    { P_GRAPHICS, "ACCESSIBILITY",                "colorblind", nullptr },
    { P_UPSCALE,  "ANTI-ALIASING AND UPSCALING",  "fsr upscale dlss_preset sharpen fsr_sharp objmv", nullptr },
    { P_UPSCALE,  "FRAME GENERATION AND LATENCY", "dlss_fg reflex", nullptr },
    { P_PERF,     "SHADER CACHE",                 "precompile precompile_threads", nullptr },
    { P_CONTROLS, "MOUSE",                        "@mouse sensy ads aimhold grab_mouse crosshair", nullptr },
    { P_CONTROLS, "CONTROLLER",                   "@padint deadzone curve padbackend padglyphs nswap lightbar "
                                                  "pad_disconnect_pause pad_southpaw pad_p1",
      "Controllers work automatically. Their buttons are set in the game's own Options \xE2\x80\xBA Controls screen." },
    { P_CONTROLS, "GYRO",                         "gyro gyrosens gyroinvy gyrospace gyrocal", nullptr },
    { P_CONTROLS, "CONTROLLER BUTTONS",           "padmap_*",
      "Also in game: F10 \xE2\x80\xBA More PC Options \xE2\x80\xBA Controller layout. Key bindings for keyboard and "
      "mouse are above." },
    { P_AUDIO,    "SOUND",                        "speakers audio_buffer", nullptr },
    { P_AUDIO,    "IN THE BACKGROUND",            "focus", nullptr },
    { P_UI,       "NOTIFICATIONS",                "trophytoast", nullptr },
    { P_UI,       "MOVIES",                       "cutscene_skip skiphint", nullptr },
    { P_UI,       "OVERLAYS",                     "overlay overlay_pos", nullptr },
    { P_UI,       "PHOTO MODE AND DISCORD",       "photo discord", nullptr },
    { P_ADVANCED, "START-UP AND QUITTING",        "skipintro quit", nullptr },
    { P_ADVANCED, "GAMEPLAY",                     "gore dyndiff", nullptr },
    { P_ADVANCED, "MODS AND TEXTURES",            "mods texpack", nullptr },
    { P_ADVANCED, "SAVES",                        "saves savebackup", nullptr },
    { P_ADVANCED, "UPDATES",                      "updates", nullptr },
};
static const char* g_page_title[P_COUNT] = { "Display", "Graphics", "Upscaling & Frame Generation", "Performance",
                                             "Controls", "Audio", "Interface & Notifications", "Advanced",
                                             "Achievements", "Saves & tools", "About" };
static const char* g_page_intro[P_COUNT] = {
    "How the game window looks and how fast it runs.",
    "Image quality. Higher settings look better and cost more GPU time.",
    "Anti-aliasing, DLSS / FSR / XeSS upscaling, sharpening and frame generation.",
    "How the game builds its shaders.",
    "",
    "Sound output, and what the game does while it is in the background.",
    "Notifications, hints and overlays the game shows while you play.",
    "Start-up, gameplay, mods, saves and updates.",
    "", "", "" };

enum { CAT_FOV = -1, CAT_MOUSE = -2, CAT_PADINT = -3 };
static void layout_special(int id, float x, float& y, float w);   // launcher.cpp

// calls f(index into g_set, or a CAT_ id for "@name") for every entry of the space-separated key list, in order
template <class F> static void cat_each(const char* keys, F f)
{
    for (const char* p = keys; *p;) {
        while (*p == ' ') p++;
        const char* e = p; while (*e && *e != ' ') e++;
        if (e > p && *p == '@') {
            const std::string n(p + 1, e);
            f(n == "fov" ? CAT_FOV : n == "mouse" ? CAT_MOUSE : CAT_PADINT);
        } else if (e > p) {
            const bool pre = e[-1] == '*';
            const size_t n = (size_t)(e - p) - (pre ? 1 : 0);
            for (size_t i = 0; i < g_set.size(); i++) {
                const char* k = g_set[i].key;
                if (pre ? strncmp(k, p, n) == 0 : (strlen(k) == n && strncmp(k, p, n) == 0)) {
                    f((int)i);
                    if (!pre) break;
                }
            }
        }
        p = e;
    }
}
static bool cat_placed(int si)
{
    bool found = false;
    for (auto& g : g_cat) cat_each(g.keys, [&](int i) { if (i == si) found = true; });
    return found;
}
static void cat_group(float x, float& y, float w, const char* head, const char* note, const std::vector<int>& rows,
                      bool& first)
{
    if (rows.empty()) return;
    if (!first) y += SF(18);
    first = false;
    deco(D_H2, RectF(x, y, w, SF(22)), head); y += SF(26);
    if (note) { deco(D_NOTE, RectF(x, y, w, SF(40)), note); y += SF(44); }
    for (int si : rows) {
        if (si >= 0) layout_setting_row(x, y, w, si);
        else layout_special(si, x, y, w);
    }
}
// the groups of one page; the Advanced page also lists every option no group places (OTHER)
static void layout_cat_groups(float x, float& y, float w, int page, bool first = true)
{
    for (auto& g : g_cat) {
        if (g.page != page) continue;
        std::vector<int> rows;
        cat_each(g.keys, [&](int i) { rows.push_back(i); });
        cat_group(x, y, w, g.head, g.note, rows, first);
    }
    if (page == P_ADVANCED) {
        std::vector<int> rows;
        for (int i = 0; i < (int)g_set.size(); i++) if (!cat_placed(i)) rows.push_back(i);
        cat_group(x, y, w, "OTHER", nullptr, rows, first);
    }
}
