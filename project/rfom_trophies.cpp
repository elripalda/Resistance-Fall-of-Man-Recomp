// Resistance: Fall of Man PC port: trophy notifications, the same as the Resistance 2 / 3 ports.
//
// The retail game has no trophy support (it predates PS3 trophies: the ELFs don't import
// sceNpTrophy and the disc has no TROPDIR), so on an unmodified copy this never fires. It is
// here so a game or mod that does unlock trophies through the runtime's sceNpTrophy HLE gets
// the same behaviour as R2 / R3. The HLE records unlocks (gamedata\trophies\<comm id>.json);
// this hooks it (g_trophy_unlock_hook) to:
//   - name the trophy from TROPHY.TRP (gamefiles\PS3_GAME\TROPDIR\<comm id>_00\TROPHY.TRP),
//   - mirror the unlock list to %USERPROFILE%\Saved Games\Resistance Fall of Man\trophies.json,
//   - log "[trophy] unlocked #N <name>" and show a toast with the grade badge (P/G/S/B), drawn
//     inside the swap chain (rsx_osd.inc). RFOM_TROPHY_TOAST=0 turns the toast off,
//   - never announce a trophy that the Saved Games record already has,
//   - award the platinum once every trophy that counts towards it is unlocked.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>
#include "rfom_trophy_data.h"
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

extern "C" int (*volatile g_trophy_unlock_hook)(const char* comm_id, int trophy_id, const unsigned char* unlocked, unsigned count);
extern "C" const char* ppu_vfs_root;
extern "C" void rfpf_toast(const char* title, const char* text, char badge);

namespace {

std::mutex s_mtx;
std::string s_loaded_comm;
std::vector<rft::Trophy> s_list;
std::string s_title;

std::string saved_games_dir()
{
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SavedGames, 0, nullptr, &p)) && p) {
        char b[MAX_PATH * 3];
        if (WideCharToMultiByte(CP_UTF8, 0, p, -1, b, sizeof b, nullptr, nullptr) > 0) out = b;
    }
    if (p) CoTaskMemFree(p);
    if (out.empty()) { const char* u = getenv("USERPROFILE"); out = std::string(u ? u : ".") + "\\Saved Games"; }
    out += "\\Resistance Fall of Man";
    return out;
}

void load(const char* comm)
{
    if (s_loaded_comm == comm) return;
    s_loaded_comm = comm;
    const std::string root = ppu_vfs_root && *ppu_vfs_root ? ppu_vfs_root : "gamefiles";
    std::string trp = root + "/PS3_GAME/TROPDIR/" + comm + "_00/TROPHY.TRP";
    if (GetFileAttributesA(trp.c_str()) == INVALID_FILE_ATTRIBUTES)
        trp = std::string("gamefiles/PS3_GAME/TROPDIR/") + comm + "_00/TROPHY.TRP";
    if (rft::load_trp(trp.c_str(), s_list, &s_title))
        fprintf(stderr, "[trophy] %zu trophies (%s) from %s\n", s_list.size(), s_title.c_str(), trp.c_str());
    else
        fprintf(stderr, "[trophy] %s not readable: unlocks are recorded without names\n", trp.c_str());
    const std::string sg = saved_games_dir() + "\\trophies.json";
    rft::merge_json_times(sg.c_str(), s_list);
    // only the Saved Games record decides "already earned": the runtime's own record has just
    // been written with this unlock
}

int hook(const char* comm_id, int id, const unsigned char* unlocked, unsigned count)
{
    std::lock_guard<std::mutex> lk(s_mtx);
    load(comm_id ? comm_id : "");
    std::vector<int> toast;
    const int plat = rft::on_unlock(s_list, id, unlocked, count, (long long)time(nullptr), toast);

    const std::string dir = saved_games_dir();
    SHCreateDirectoryExA(nullptr, dir.c_str(), nullptr);
    rft::write_json((dir + "\\trophies.json").c_str(), s_title.empty() ? "Resistance: Fall of Man" : s_title.c_str(),
                    comm_id ? comm_id : "", s_list);

    const char* e = getenv("RFOM_TROPHY_TOAST");
    const bool show = !(e && *e == '0');
    if (toast.empty()) fprintf(stderr, "[trophy] #%d already earned (Saved Games record), not announced again\n", id);
    for (int tid : toast) {
        const rft::Trophy* tr = nullptr;
        for (auto& x : s_list) if (x.id == tid) tr = &x;
        char head[96];
        snprintf(head, sizeof head, "%s trophy unlocked", tr ? rft::grade_name(tr->grade) : "Trophy");
        std::string body = tr ? tr->name : "Trophy #" + std::to_string(tid);
        if (tr && !tr->detail.empty()) body += " \xE2\x80\x94 " + tr->detail;
        fprintf(stderr, "[trophy] unlocked #%d %s (%s)\n", tid, tr ? tr->name.c_str() : "",
                tr ? rft::grade_name(tr->grade) : "?");
        if (show) rfpf_toast(head, body.c_str(), tr ? tr->grade : 'B');
    }
    return plat;
}

struct TrophyInit {
    TrophyInit() { g_trophy_unlock_hook = hook; }
} s_init;

} // namespace
