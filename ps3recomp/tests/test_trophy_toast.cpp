// rfom_trophy_data.h: the bookkeeping behind the port's trophy toasts.
#include "rfom_trophy_data.h"
#include <cstdio>
#include <vector>

static int s_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); s_fail++; } } while (0)

static std::vector<rft::Trophy> sample()
{
    std::vector<rft::Trophy> l;
    rft::Trophy p; p.id = 0; p.grade = 'P'; p.name = "Plat"; l.push_back(p);
    rft::Trophy g; g.id = 1; g.grade = 'G'; g.pid = 0; g.name = "Gold"; l.push_back(g);
    rft::Trophy b; b.id = 2; b.grade = 'B'; b.pid = 0; b.name = "Bronze"; l.push_back(b);
    return l;
}

int main()
{
    std::vector<int> toast;
    unsigned char flags[3] = {0, 0, 0};

    {   // a new unlock is announced once; a repeat is not
        auto l = sample();
        flags[1] = 1;
        CHECK(rft::on_unlock(l, 1, flags, 3, 100, toast) == -1);
        CHECK(toast.size() == 1 && toast[0] == 1);
        CHECK(l[1].unlocked == 100);
        CHECK(rft::on_unlock(l, 1, flags, 3, 200, toast) == -1);
        CHECK(toast.empty());
        CHECK(l[1].unlocked == 100);   // first unlock time kept
    }
    {   // the last trophy towards the platinum awards it, both announced
        auto l = sample();
        flags[1] = 1; flags[2] = 0;
        rft::on_unlock(l, 1, flags, 3, 100, toast);
        flags[2] = 1;
        CHECK(rft::on_unlock(l, 2, flags, 3, 110, toast) == 0);
        CHECK(toast.size() == 2 && toast[0] == 2 && toast[1] == 0);
        CHECK(l[0].unlocked == 110);
        CHECK(rft::on_unlock(l, 2, flags, 3, 120, toast) == -1);   // platinum only once
        CHECK(toast.empty());
    }
    {   // the Saved Games record survives a reset of the runtime's own record: no second toast
        auto l = sample();
        flags[0] = 0; flags[1] = 1; flags[2] = 0;
        rft::on_unlock(l, 1, flags, 3, 100, toast);
        CHECK(rft::write_json("test_trophy_toast.json", "Test", "NPWR00000", l));
        auto l2 = sample();
        rft::merge_json_times("test_trophy_toast.json", l2);
        CHECK(l2[1].unlocked == 100 && l2[2].unlocked == 0);
        CHECK(rft::on_unlock(l2, 1, flags, 3, 300, toast) == -1);
        CHECK(toast.empty());
        CHECK(l2[1].unlocked == 100);
        std::remove("test_trophy_toast.json");
    }
    {   // no TROPHY.TRP: the id is still recorded and announced
        std::vector<rft::Trophy> l;
        unsigned char f5[6] = {0, 0, 0, 0, 0, 1};
        CHECK(rft::on_unlock(l, 5, f5, 6, 100, toast) == -1);
        CHECK(l.size() == 1 && l[0].id == 5 && l[0].unlocked == 100 && l[0].name == "Trophy 5");
        CHECK(toast.size() == 1 && toast[0] == 5);
    }
    if (s_fail) { std::printf("%d failure(s)\n", s_fail); return 1; }
    std::printf("trophy_toast: all passed\n");
    return 0;
}
