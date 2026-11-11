/*
 * spurs_mt.h - host-side multithreading policy for the SPURS lane scheduler
 * (cellSpurs.c): how many SPU identities job lists may run on at once, which
 * logical CPUs the lane threads prefer, and the wake primitives that replaced
 * the single shared condition variable.
 *
 * Kept free of guest state so tests/spurs_mt can exercise it directly.
 *
 * Wake protocol (targeted mode):
 *   - every lane sleeps on its OWN condition variable (si->lock); whoever hands
 *     the lane work does so under si->lock and wakes only that lane;
 *   - the kernel (coordinator) thread sleeps on a kick counter with
 *     WaitOnAddress; a guest kick (ready-count store, API change, a lane that
 *     returned without picking new work) increments the counter and wakes it.
 *     The counter cannot lose a wake-up (WaitOnAddress compares before
 *     sleeping), and the kicker needs no lock.
 * The old protocol woke every lane plus the kernel on each kick: Fall of Man
 * kicks thousands of times a second, so each one cost ~7 host wake-ups that
 * then fought over si->lock.
 */
#ifndef PS3RECOMP_SPURS_MT_H
#define PS3RECOMP_SPURS_MT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SPURS_MT_MAX_CORES   = 64,
    SPURS_MT_REAL_SPUS   = 3,   /* what Fall of Man initializes SPURS with */
    SPURS_MT_MAX_SPUS    = 6,   /* job-list headers / PM contexts hold 6 SPU rows */
};

/* One physical core: the mask of its logical CPUs (processor group 0). */
typedef struct spurs_mt_topo {
    int      ncores;
    uint64_t core_mask[SPURS_MT_MAX_CORES];
} spurs_mt_topo;

typedef struct spurs_mt_cfg {
    int legacy;        /* 1: the pre-pool behaviour (shared CV wake storm, no placement) */
    int workers;       /* SPU identities job lists may run as at once (3..6) */
    int is_auto;       /* workers came from RFOM_SPU_WORKERS=auto */
} spurs_mt_cfg;

static inline int spurs_mt_popcount64(uint64_t m)
{
    int n = 0;
    for (; m; m &= m - 1) n++;
    return n;
}

/* Physical cores with no logical CPU in `reserved` (the main thread's and the
 * RSX FIFO thread's pinned cores). */
static inline int spurs_mt_free_cores(const spurs_mt_topo* t, uint64_t reserved)
{
    int n = 0;
    for (int i = 0; i < t->ncores; i++)
        if (t->core_mask[i] && !(t->core_mask[i] & reserved)) n++;
    return n;
}

/* Auto pool size: one SPU identity per free physical core, never fewer than
 * the console's 3 and never more than the 6 rows the job lists carry. */
static inline int spurs_mt_auto_workers(int free_cores)
{
    if (free_cores < SPURS_MT_REAL_SPUS) return SPURS_MT_REAL_SPUS;
    if (free_cores > SPURS_MT_MAX_SPUS) return SPURS_MT_MAX_SPUS;
    return free_cores;
}

/* RFOM_SPU_WORKERS:
 *   unset / ""  -> 3 SPU identities (console layout), new wake protocol
 *   0 or 1      -> legacy: the old scheduler exactly (shared wake, no hints)
 *   2..3        -> 3 identities, new wake protocol
 *   4..6 (>6 clamps to 6) -> virtual SPUs 3..N-1 for any-SPU job lists
 *   auto        -> spurs_mt_auto_workers(free physical cores)
 * Anything unparsable is treated as unset. */
static inline spurs_mt_cfg spurs_mt_parse_workers(const char* env, int auto_workers)
{
    spurs_mt_cfg c;
    c.legacy = 0; c.workers = SPURS_MT_REAL_SPUS; c.is_auto = 0;
    if (!env || !*env) return c;
    if ((env[0] == 'a' || env[0] == 'A')) {
        c.is_auto = 1;
        c.workers = auto_workers < SPURS_MT_REAL_SPUS ? SPURS_MT_REAL_SPUS
                  : auto_workers > SPURS_MT_MAX_SPUS ? SPURS_MT_MAX_SPUS : auto_workers;
        return c;
    }
    if (env[0] < '0' || env[0] > '9') return c;
    const long v = strtol(env, NULL, 10);
    if (v <= 1) { c.legacy = 1; return c; }
    c.workers = v < SPURS_MT_REAL_SPUS ? SPURS_MT_REAL_SPUS
              : v > SPURS_MT_MAX_SPUS ? SPURS_MT_MAX_SPUS : (int)v;
    return c;
}

/* Lane count for an instance: the real SPUs, the extra "hog rule" lanes, and
 * one more lane per virtual SPU (each virtual SPU needs a lane of its own). */
static inline unsigned spurs_mt_lane_count(unsigned nspus, int extra, int workers, unsigned max_lanes)
{
    unsigned n = nspus + (unsigned)(extra > 0 ? extra : 0);
    if (workers > (int)nspus) n += (unsigned)workers - nspus;
    return n > max_lanes ? max_lanes : n;
}

/* Preferred logical CPU per lane. Lanes go to free physical cores first, one
 * each, starting after core 0 (Windows routes most interrupts there) and
 * wrapping to core 0 last; once every free core has a lane, the second
 * logical CPU (SMT sibling) of each free core; then round-robin. Reserved
 * cores are never used unless nothing else is left. Returns the number of
 * entries written (= nlanes), -1 entries meaning "no hint". */
static inline int spurs_mt_lane_cpus(const spurs_mt_topo* t, uint64_t allowed, uint64_t reserved,
                                     int nlanes, int* out)
{
    int order[SPURS_MT_MAX_CORES], nord = 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < t->ncores; i++) {
            const uint64_t m = t->core_mask[i] & allowed;
            if (!m || (m & reserved)) continue;
            const int is_core0 = (t->core_mask[i] & 1u) != 0;
            if (pass == 0 && is_core0) continue;
            if (pass == 1 && !is_core0) continue;
            order[nord++] = i;
        }
    if (!nord) {
        for (int i = 0; i < t->ncores; i++)
            if (t->core_mask[i] & allowed) order[nord++] = i;
    }
    int cpus[SPURS_MT_MAX_CORES * 2], ncpu = 0;
    for (int smt = 0; smt < 2; smt++)
        for (int k = 0; k < nord; k++) {
            uint64_t m = t->core_mask[order[k]] & allowed;
            for (int s = 0; s < smt && m; s++) m &= m - 1;
            if (!m) continue;
            int bit = 0;
            while (!((m >> bit) & 1u)) bit++;
            cpus[ncpu++] = bit;
        }
    for (int l = 0; l < nlanes; l++) out[l] = ncpu ? cpus[l % ncpu] : -1;
    return nlanes;
}

#if defined(_WIN32) || defined(PS3RECOMP_WIN32_COMPAT_H)   /* POSIX: win32_compat shims */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if defined(_MSC_VER)
#pragma comment(lib, "Synchronization.lib")   /* WaitOnAddress / WakeByAddressSingle */
#endif
#endif

/* Announce new work to the kernel thread. Lock-free; never loses a wake-up. */
static inline void spurs_mt_kick(volatile LONG* seq)
{
    InterlockedIncrement(seq);
    WakeByAddressSingle((PVOID)seq);
}

/* Kernel side: sleep until the counter moves past `seen` or `ms` elapses
 * (the poll pass still runs on the timeout -- the guest also posts work by
 * writing job-list memory, which nothing announces). */
static inline void spurs_mt_kick_wait(volatile LONG* seq, LONG seen, DWORD ms)
{
    if (*seq == seen) WaitOnAddress(seq, &seen, sizeof seen, ms);
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* PS3RECOMP_SPURS_MT_H */
