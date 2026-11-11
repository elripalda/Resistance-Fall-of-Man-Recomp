// Resistance: Fall of Man PC port: fan-made achievements for the launcher (the PS3 game has no trophies).
// Computed read-only from the game's profile save, PROFILE.SAV
// (<saves>\dev_hdd0\home\00000001\savedata\BCUS98107_PROFILE\PROFILE.SAV).
//
// PROFILE.SAV, as written by the game (decoded from its code, not guessed):
//   0x0000  16-byte header: "IGSD", then a version and a checksum that changes on every save
//   0x0010  the profile (0x3378 bytes, big-endian; reset by the game's profile reset, 0x002B3358)
//   profile +0x0C90 / +0x1E08  two campaign records of 0x1178 bytes. The game writes the second one
//           when its game mode is 2 (0x0001C7E0), the first one otherwise.
//     record +0x111C  int8[11], one per campaign chapter (chapter = (level - 20) / 10): -1 until the
//           chapter is finished, then the highest difficulty it was finished on (0x002B29A0 ->
//           0x002B24E8 store max(old, difficulty) when the player moves on to the next chapter).
//   profile +0x2F80  uint32 BE words, bit n = skill point n completed (word n>>5, bit n&31; 0x002B2740).
//     67 entries: those with a level >= 0 in the game's level table are the real skill points
//     (hasAllSkillPoints, 0x00035470); bit 14 = Superhuman completed, 64 = Superhuman unlocked,
//     65 = Black Ops skin unlocked, 66 = Skull skin unlocked (isSuperhumanCompleted etc.).
//   Point values and levels: the game's tables at 0x10041520 / 0x10041414.
// Not decoded (so not used): the skill point names (localised text), the difficulty numbering, the
// journal / intel and the other per-campaign data.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rfach {

enum { SAVE_SIZE = 0x10 + 0x3378, NUM_SKILL = 67, NUM_CHAPTERS = 11 };

static const signed char kSkillLevel[NUM_SKILL] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 2, -1, -1, -1, 2, 2, -1, -1, -1, -1, 3, -1, -1, 3, -1, -1, -1, 3, 4,
    4, 5, -1, -1, -1, -1, -1, 6, 6, -1, 6, 7, 7, 7, 7, 7, 8, 8, -1, 8, 9, 9, 10, 10, 11, 12, -1, 12, 12, 12, -1, -1, -1 };
static const unsigned char kSkillValue[NUM_SKILL] = {
    2, 3, 7, 1, 2, 2, 3, 2, 2, 3, 3, 3, 3, 5, 0, 3, 0, 0, 0, 3, 3, 0, 0, 0, 0, 3, 0, 0, 3, 0, 0, 0, 4, 4,
    2, 2, 2, 0, 0, 0, 0, 3, 2, 4, 3, 3, 2, 2, 3, 3, 3, 4, 2, 2, 3, 4, 3, 2, 4, 3, 0, 4, 2, 3, 0, 0, 0 };
enum { BIT_SUPERHUMAN_DONE = 14, BIT_SUPERHUMAN_UNLOCKED = 64, BIT_BLACKOPS_SKIN = 65, BIT_SKULL_SKIN = 66 };
enum { OFF_SKILL_BITS = 0x10 + 0x2F80, OFF_CAMPAIGN0 = 0x10 + 0x0C90, CAMPAIGN_SIZE = 0x1178, OFF_CHAPTERS = 0x111C };

struct Profile {
    bool valid = false;
    std::string error;                       // why valid is false
    bool bit[NUM_SKILL] = {};
    int skill_points = 0, skill_points_total = 0;   // real skill points earned / in the game
    int points = 0, points_total = 0;               // their point values
    int chapters[2][NUM_CHAPTERS] = {};             // per campaign record: -1 = not finished
    int chapters_done = 0;                          // finished in either record
};

inline bool parse(const unsigned char* d, size_t n, Profile& p)
{
    p = Profile();
    if (!d || n != SAVE_SIZE || memcmp(d, "IGSD", 4) != 0) {
        p.error = "PROFILE.SAV is not in the format this launcher knows.";
        return false;
    }
    for (int i = 0; i < NUM_SKILL; i++) {
        const unsigned char* w = d + OFF_SKILL_BITS + (i >> 5) * 4;
        const uint32_t v = (uint32_t)w[0] << 24 | (uint32_t)w[1] << 16 | (uint32_t)w[2] << 8 | w[3];
        p.bit[i] = (v >> (i & 31)) & 1;
        if (kSkillLevel[i] < 0) continue;
        p.skill_points_total++; p.points_total += kSkillValue[i];
        if (p.bit[i]) { p.skill_points++; p.points += kSkillValue[i]; }
    }
    for (int c = 0; c < NUM_CHAPTERS; c++) {
        bool any = false;
        for (int s = 0; s < 2; s++) {
            p.chapters[s][c] = (signed char)d[OFF_CAMPAIGN0 + s * CAMPAIGN_SIZE + OFF_CHAPTERS + c];
            any |= p.chapters[s][c] >= 0;
        }
        p.chapters_done += any;
    }
    p.valid = true;
    return true;
}

// Never writes: the file is opened read-only, shared so the game can keep writing it.
inline bool load(const char* path, Profile& p)
{
    p = Profile();
    FILE* f = fopen(path, "rb");
    if (!f) { p.error = "No save yet."; return false; }
    std::vector<unsigned char> d(SAVE_SIZE + 1);
    const size_t n = fread(d.data(), 1, d.size(), f);
    fclose(f);
    return parse(d.data(), n, p);
}

struct Achievement {
    const char* name;
    const char* detail;
    char grade;                // P G S B, like trophies
    bool earned;
    int have, need;            // progress (need 0 = yes/no)
};

// The fan-made set. Only what PROFILE.SAV records reliably; never awarded in game.
inline std::vector<Achievement> evaluate(const Profile& p)
{
    std::vector<Achievement> a;
    auto count = [&](const char* n, const char* d, char g, int have, int need) { a.push_back({ n, d, g, have >= need, have < need ? have : need, need }); };
    auto flag = [&](const char* n, const char* d, char g, bool on) { a.push_back({ n, d, g, on, 0, 0 }); };
    const int sp = p.skill_points, spn = p.skill_points_total ? p.skill_points_total : 43;
    count("First Mark", "Earn a skill point.", 'B', sp, 1);
    count("Sharpshooter", "Earn 10 skill points.", 'B', sp, 10);
    count("Veteran", "Earn 20 skill points.", 'S', sp, 20);
    count("Sentinel", "Earn 30 skill points.", 'S', sp, 30);
    count("Perfectionist", "Earn every skill point.", 'G', sp, spn);
    count("Point Collector", "Collect 25 skill point points.", 'B', p.points, 25);
    count("High Scorer", "Collect 75 skill point points.", 'S', p.points, 75);
    count("Into the Fray", "Finish a campaign chapter.", 'B', p.chapters_done, 1);
    count("Pushing Back", "Finish 4 campaign chapters.", 'B', p.chapters_done, 4);
    count("Holding the Line", "Finish 7 campaign chapters.", 'S', p.chapters_done, 7);
    count("Liberator", "Finish 10 campaign chapters.", 'G', p.chapters_done, 10);
    flag("Superhuman Ready", "Unlock the Superhuman difficulty.", 'S', p.bit[BIT_SUPERHUMAN_UNLOCKED]);
    flag("Superhuman", "Complete the campaign on Superhuman.", 'G', p.bit[BIT_SUPERHUMAN_DONE]);
    flag("Black Ops", "Unlock the Black Ops skin.", 'S', p.bit[BIT_BLACKOPS_SKIN]);
    flag("Death's Head", "Unlock the Skull skin.", 'S', p.bit[BIT_SKULL_SKIN]);
    int all = 0;
    for (const auto& x : a) all += x.earned;
    count("Fall of the Chimera", "Earn every other achievement.", 'P', all, (int)a.size());
    return a;
}

} // namespace rfach
