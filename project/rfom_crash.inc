/* Resistance port: crash and hang reports for play-testing (shared engine:
 * port_crash.h, also used by the Resistance 2 / 3 ports).
 *
 * A crash (an exception nobody handles, a stack overflow, abort(); the
 * guest-VM demand-paging handler runs first and is unaffected) writes
 * logs\crash\RFOM_<time>_<exe>_crash.dmp (small minidump) and
 * RFOM_<time>_<exe>_crash.txt (exception, faulting module + offset, lifted guest
 * function, every thread's native stack, the end of run.log, the settings),
 * adds a line to logs\crash\crashes.txt, then lets the process die as before
 * (after the filter that was installed before it). A hang -- the flip counter
 * stops once the game flips steadily; focus-loss pauses do not count -- writes
 * RFOM_<time>_<exe>_hang.dmp/.txt once per stall and keeps running
 * (FREEZE_RELAUNCH, debugtools.inc, still decides about restarts).
 *   RFOM_CRASH_DUMPS=0 / PORT_CRASH_DUMPS=0   both off
 *   RFOM_CRASH_DUMPS=full                     full-memory dumps (several GB)
 *   PORT_HANG_DUMP=1   strict watchdog: 20 s (PORT_HANG_S) without a flip
 *   PORT_HANG_DUMP=0   no watchdog; unset: RFOM_HANG_S (default 90) s
 * Both executables (ResistanceFoM.exe front end, ResistanceFoMGame.exe game)
 * include this, so the exe name is part of every file name and line. */
#ifdef _WIN32
#include "port_crash.h"

extern "C" volatile long rsx_focus_paused;     /* rsx_d3d12_backend.c: paused on focus loss */
extern "C" uint32_t ppu_prof_resolve_host(void* ra);   /* ppu_loader.cpp: host pc -> lifted function */

static uint64_t rfom_crash_flips(void) { return cellGcm_flip_request_count(); }
static int rfom_crash_paused(void) { return rsx_focus_paused != 0; }

static void rfom_crash_init(void)
{
    static char exe[64] = "game";
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, sizeof path);
    if (const char* b = strrchr(path, '\\')) {
        snprintf(exe, sizeof exe, "%s", b + 1);
        if (char* dot = strrchr(exe, '.')) *dot = 0;
    }
    PortCrashConfig c = {};
    c.tag = "RFOM";
    c.title = "Resistance: Fall of Man";
    c.env_prefix = "RFOM";
    c.exe_suffix = exe;
    c.main_progress = rfom_crash_flips;
    c.main_start_min = 200;          /* not before the game flips steadily (boot) */
    c.paused = rfom_crash_paused;
    c.vm_base = &vm_base;
    c.vm_size = 0x100010000ull;      /* VM_SIZE, defined further down */
    c.guest_resolve = ppu_prof_resolve_host;
    port_crash::init(c);
}
#else
static void rfom_crash_init(void) {}
#endif