// rfom_achievements.h (the launcher's fan-made Achievements page): PROFILE.SAV decoding and the
// achievement rules, on synthetic saves laid out like the game's.
#include "rfom_achievements.h"
#include <cstdio>
#include <vector>

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static std::vector<unsigned char> blank_save()
{
    std::vector<unsigned char> d(rfach::SAVE_SIZE, 0);
    memcpy(d.data(), "IGSD", 4);
    d[0x10] = 6; d[0x11] = 9; d[0x12] = 8;
    for (int s = 0; s < 2; s++)       // a fresh profile: no chapter finished
        memset(&d[rfach::OFF_CAMPAIGN0 + s * rfach::CAMPAIGN_SIZE + rfach::OFF_CHAPTERS], 0xFF, rfach::NUM_CHAPTERS);
    return d;
}
static void set_bit(std::vector<unsigned char>& d, int n)
{
    d[rfach::OFF_SKILL_BITS + (n >> 5) * 4 + 3 - ((n & 31) >> 3)] |= (unsigned char)(1u << (n & 7));
}
static const rfach::Achievement* find(const std::vector<rfach::Achievement>& a, const char* name)
{
    for (const auto& x : a) if (!strcmp(x.name, name)) return &x;
    return nullptr;
}

int main()
{
    rfach::Profile p;
    // rejected: wrong size, wrong magic
    std::vector<unsigned char> d = blank_save();
    CHECK(!rfach::parse(d.data(), d.size() - 1, p) && !p.valid);
    d[0] = 'X'; CHECK(!rfach::parse(d.data(), d.size(), p)); d[0] = 'I';

    // fresh profile: nothing earned
    CHECK(rfach::parse(d.data(), d.size(), p));
    CHECK(p.skill_points == 0 && p.skill_points_total == 43 && p.points == 0 && p.points_total == 126);
    CHECK(p.chapters_done == 0);
    auto a = rfach::evaluate(p);
    CHECK(a.size() >= 10 && a.size() <= 25);
    for (const auto& x : a) CHECK(!x.earned);

    // skill point 8 (word 0, bit 8: big-endian 00 00 01 00, as in a real save) = 1 point of value 2
    set_bit(d, 8);
    CHECK(d[rfach::OFF_SKILL_BITS + 2] == 0x01);
    rfach::parse(d.data(), d.size(), p);
    CHECK(p.skill_points == 1 && p.points == 2 && p.bit[8]);
    a = rfach::evaluate(p);
    CHECK(find(a, "First Mark")->earned && !find(a, "Sharpshooter")->earned && find(a, "Sharpshooter")->have == 1);

    // bit 14 (Superhuman completed) and 64..66 are flags, not skill points
    set_bit(d, 14); set_bit(d, 64); set_bit(d, 65); set_bit(d, 66);
    rfach::parse(d.data(), d.size(), p);
    CHECK(p.skill_points == 1);
    a = rfach::evaluate(p);
    CHECK(find(a, "Superhuman")->earned && find(a, "Superhuman Ready")->earned);
    CHECK(find(a, "Black Ops")->earned && find(a, "Death's Head")->earned);

    // chapters: finished in either campaign record (value >= 0 = the difficulty it was finished on)
    d[rfach::OFF_CAMPAIGN0 + rfach::OFF_CHAPTERS + 0] = 0;
    d[rfach::OFF_CAMPAIGN0 + rfach::OFF_CHAPTERS + 1] = 2;
    d[rfach::OFF_CAMPAIGN0 + rfach::CAMPAIGN_SIZE + rfach::OFF_CHAPTERS + 1] = 1;
    d[rfach::OFF_CAMPAIGN0 + rfach::CAMPAIGN_SIZE + rfach::OFF_CHAPTERS + 5] = 3;
    rfach::parse(d.data(), d.size(), p);
    CHECK(p.chapters_done == 3 && p.chapters[0][0] == 0 && p.chapters[1][1] == 1 && p.chapters[0][2] == -1);
    a = rfach::evaluate(p);
    CHECK(find(a, "Into the Fray")->earned && !find(a, "Pushing Back")->earned);

    // everything: every real skill point and 10 chapters -> the platinum
    for (int i = 0; i < rfach::NUM_SKILL; i++) set_bit(d, i);
    for (int c = 0; c < 10; c++) d[rfach::OFF_CAMPAIGN0 + rfach::OFF_CHAPTERS + c] = 1;
    rfach::parse(d.data(), d.size(), p);
    CHECK(p.skill_points == 43 && p.points == 126);
    a = rfach::evaluate(p);
    for (const auto& x : a) CHECK(x.earned);
    CHECK(a.back().grade == 'P');

    // missing file
    CHECK(!rfach::load("no_such_dir\\PROFILE.SAV", p) && !p.valid);

    printf(fails ? "rfom_achievements: %d failure(s)\n" : "rfom_achievements: all passed\n", fails);
    return fails ? 1 : 0;
}
