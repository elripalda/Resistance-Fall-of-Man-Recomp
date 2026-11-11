/*
 * spurs_mt.h: SPU lane pool sizing, CPU placement, and the targeted wake
 * protocol cellSpurs.c's lane scheduler uses (per-lane condition variable for
 * hand-offs, kernel kick counter for new work).
 *
 * The threaded part runs a model of that scheduler -- one kernel thread
 * selecting work for idle lanes, lanes that self-dispatch after each job and
 * otherwise kick the kernel, a producer that posts work and kicks -- over a
 * random dependency graph, and checks that every job runs exactly once, only
 * after all of its dependencies, that no kick is lost (the kernel's fallback
 * timeout is long, so a lost kick shows up as a stall), and that the lanes
 * really overlap.
 */
#include "../libs/spurs/spurs_mt.h"
#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail = 1; } } while (0)

static void test_parse(void)
{
    spurs_mt_cfg c;
    c = spurs_mt_parse_workers(NULL, 4);   CHECK(!c.legacy && c.workers == 3 && !c.is_auto);
    c = spurs_mt_parse_workers("", 4);     CHECK(!c.legacy && c.workers == 3);
    c = spurs_mt_parse_workers("0", 4);    CHECK(c.legacy);
    c = spurs_mt_parse_workers("1", 4);    CHECK(c.legacy);
    c = spurs_mt_parse_workers("2", 4);    CHECK(!c.legacy && c.workers == 3);
    c = spurs_mt_parse_workers("4", 4);    CHECK(!c.legacy && c.workers == 4);
    c = spurs_mt_parse_workers("6", 4);    CHECK(c.workers == 6);
    c = spurs_mt_parse_workers("12", 4);   CHECK(c.workers == 6);
    c = spurs_mt_parse_workers("auto", 4); CHECK(!c.legacy && c.is_auto && c.workers == 4);
    c = spurs_mt_parse_workers("Auto", 9); CHECK(c.workers == 6);
    c = spurs_mt_parse_workers("auto", 1); CHECK(c.workers == 3);
    c = spurs_mt_parse_workers("x", 4);    CHECK(!c.legacy && c.workers == 3);
    CHECK(spurs_mt_auto_workers(2) == 3);
    CHECK(spurs_mt_auto_workers(4) == 4);
    CHECK(spurs_mt_auto_workers(14) == 6);
    CHECK(spurs_mt_lane_count(3, 3, 3, 10) == 6);
    CHECK(spurs_mt_lane_count(3, 3, 6, 10) == 9);
    CHECK(spurs_mt_lane_count(3, 3, 6, 8) == 8);
    CHECK(spurs_mt_lane_count(3, 0, 4, 10) == 4);
}

static void test_placement(void)
{
    /* 6 cores / 12 threads (Ryzen 5 5600X); main thread on CPU 2, FIFO on CPU 4 */
    spurs_mt_topo t; memset(&t, 0, sizeof t);
    t.ncores = 6;
    for (int i = 0; i < 6; i++) t.core_mask[i] = 3ull << (2 * i);
    const uint64_t reserved = t.core_mask[1] | t.core_mask[2];
    CHECK(spurs_mt_free_cores(&t, reserved) == 4);
    int cpus[9];
    spurs_mt_lane_cpus(&t, 0xFFF, reserved, 9, cpus);
    const int want[9] = { 6, 8, 10, 0, 7, 9, 11, 1, 6 };
    for (int i = 0; i < 9; i++) CHECK(cpus[i] == want[i]);
    for (int i = 0; i < 9; i++) CHECK(!((reserved >> cpus[i]) & 1u));

    /* 4 cores, no SMT, nothing reserved */
    memset(&t, 0, sizeof t); t.ncores = 4;
    for (int i = 0; i < 4; i++) t.core_mask[i] = 1ull << i;
    int c4[6];
    spurs_mt_lane_cpus(&t, 0xF, 0, 6, c4);
    const int w4[6] = { 1, 2, 3, 0, 1, 2 };
    for (int i = 0; i < 6; i++) CHECK(c4[i] == w4[i]);

    /* everything reserved: fall back to all allowed cores rather than none */
    memset(&t, 0, sizeof t); t.ncores = 2; t.core_mask[0] = 3; t.core_mask[1] = 0xC;
    int c2[4];
    spurs_mt_lane_cpus(&t, 0xF, 0xF, 4, c2);
    CHECK(c2[0] == 0 && c2[1] == 2 && c2[2] == 1 && c2[3] == 3);

    /* the process affinity mask is honoured */
    int c1[2];
    spurs_mt_lane_cpus(&t, 0x4, 0, 2, c1);
    CHECK(c1[0] == 2 && c1[1] == 2);
}

#ifdef _WIN32
/* ---- scheduler model ---------------------------------------------------- */
enum { NJOBS = 3000, NLANES = 6, MAXDEP = 3, KERNEL_TIMEOUT_MS = 400 };

typedef struct { int ndep, dep[MAXDEP]; volatile LONG posted, claimed, done; LONG seq; } Job;
typedef struct { CONDITION_VARIABLE cv; int busy, job; HANDLE th; } Lane;

static Job s_job[NJOBS];
static Lane s_lane[NLANES];
static SRWLOCK s_lock = SRWLOCK_INIT;
static volatile LONG s_kick, s_seq, s_ndone, s_running, s_maxrun, s_stop, s_errors;
static volatile LONGLONG s_worst_ms;
static LARGE_INTEGER s_freq;

static int job_ready(int j)
{
    if (!s_job[j].posted || s_job[j].claimed) return 0;
    for (int d = 0; d < s_job[j].ndep; d++) if (!s_job[s_job[j].dep[d]].done) return 0;
    return 1;
}
/* caller holds s_lock */
static int pick_job(void)
{
    for (int j = 0; j < NJOBS; j++) if (job_ready(j)) { s_job[j].claimed = 1; return j; }
    return -1;
}

static DWORD WINAPI lane_main(LPVOID p)
{
    Lane* l = (Lane*)p;
    AcquireSRWLockExclusive(&s_lock);
    for (;;) {
        while (!l->busy && !s_stop) SleepConditionVariableSRW(&l->cv, &s_lock, INFINITE, 0);
        if (!l->busy && s_stop) break;
        const int j = l->job;
        ReleaseSRWLockExclusive(&s_lock);
        const LONG r = InterlockedIncrement(&s_running);
        for (LONG m = s_maxrun; r > m && InterlockedCompareExchange(&s_maxrun, r, m) != m; m = s_maxrun) {}
        for (int d = 0; d < s_job[j].ndep; d++)
            if (!s_job[s_job[j].dep[d]].done) InterlockedIncrement(&s_errors);   /* ran before a dependency */
        LARGE_INTEGER t0, t1; QueryPerformanceCounter(&t0);
        do { YieldProcessor(); QueryPerformanceCounter(&t1); }
        while ((t1.QuadPart - t0.QuadPart) * 1000000 / s_freq.QuadPart < 30);   /* ~30 us of "SPU" work */
        InterlockedDecrement(&s_running);
        AcquireSRWLockExclusive(&s_lock);
        s_job[j].seq = InterlockedIncrement(&s_seq);
        InterlockedExchange(&s_job[j].done, 1);
        InterlockedIncrement(&s_ndone);
        l->busy = 0;
        /* self-dispatch, else the kernel looks for work for the other lanes */
        const int nj = pick_job();
        if (nj >= 0) { l->job = nj; l->busy = 1; }
        spurs_mt_kick(&s_kick);   /* completions can make other jobs ready */
    }
    ReleaseSRWLockExclusive(&s_lock);
    return 0;
}

static DWORD WINAPI kernel_main(LPVOID p)
{
    (void)p;
    LONG seen = s_kick - 1;   /* first pass runs at once: work may predate this thread */
    AcquireSRWLockExclusive(&s_lock);
    while (!s_stop) {
        ReleaseSRWLockExclusive(&s_lock);
        LARGE_INTEGER t0, t1; QueryPerformanceCounter(&t0);
        spurs_mt_kick_wait(&s_kick, seen, KERNEL_TIMEOUT_MS);
        QueryPerformanceCounter(&t1);
        AcquireSRWLockExclusive(&s_lock);
        seen = s_kick;
        int handed = 0;
        for (int i = 0; i < NLANES; i++) {
            if (s_lane[i].busy) continue;
            const int j = pick_job();
            if (j < 0) break;
            s_lane[i].job = j; s_lane[i].busy = 1;
            WakeConditionVariable(&s_lane[i].cv);
            handed = 1;
        }
        /* work found after sleeping out the whole fallback timeout = a lost kick */
        const LONGLONG ms = (t1.QuadPart - t0.QuadPart) * 1000 / s_freq.QuadPart;
        if (handed && ms >= KERNEL_TIMEOUT_MS - 5) InterlockedIncrement(&s_errors);
        if (ms > s_worst_ms) s_worst_ms = ms;
    }
    ReleaseSRWLockExclusive(&s_lock);
    return 0;
}

static void test_scheduler(void)
{
    QueryPerformanceFrequency(&s_freq);
    unsigned rng = 12345u;
    for (int j = 0; j < NJOBS; j++) {
        s_job[j].ndep = 0;
        /* a few long chains plus wide independent batches with barriers */
        if (j % 100 == 99) {            /* barrier: depends on the previous three */
            for (int d = 1; d <= MAXDEP; d++) s_job[j].dep[s_job[j].ndep++] = j - d;
        } else if (j > 0) {
            rng = rng * 1103515245u + 12345u;
            const int nd = (int)((rng >> 16) % 3);
            for (int d = 0; d < nd; d++) {
                rng = rng * 1103515245u + 12345u;
                const int back = 1 + (int)((rng >> 16) % 50);
                if (j - back >= 0) s_job[j].dep[s_job[j].ndep++] = j - back;
            }
        }
    }
    for (int i = 0; i < NLANES; i++) s_lane[i].th = CreateThread(NULL, 0, lane_main, &s_lane[i], 0, NULL);
    HANDLE k = CreateThread(NULL, 0, kernel_main, NULL, 0, NULL);

    /* the producer posts in bursts, kicking once per burst (like a ready-count store) */
    LARGE_INTEGER t0, t1; QueryPerformanceCounter(&t0);
    for (int j = 0; j < NJOBS; ) {
        const int burst = 1 + j % 7;
        for (int b = 0; b < burst && j < NJOBS; b++, j++) InterlockedExchange(&s_job[j].posted, 1);
        spurs_mt_kick(&s_kick);
        if (j % 64 == 0) Sleep(0);
    }
    while (s_ndone < NJOBS) {
        QueryPerformanceCounter(&t1);
        if ((t1.QuadPart - t0.QuadPart) / s_freq.QuadPart > 60) break;
        Sleep(1);
    }
    QueryPerformanceCounter(&t1);
    AcquireSRWLockExclusive(&s_lock);
    s_stop = 1;
    for (int i = 0; i < NLANES; i++) WakeAllConditionVariable(&s_lane[i].cv);
    spurs_mt_kick(&s_kick);
    ReleaseSRWLockExclusive(&s_lock);
    WaitForSingleObject(k, INFINITE); CloseHandle(k);
    for (int i = 0; i < NLANES; i++) { WaitForSingleObject(s_lane[i].th, INFINITE); CloseHandle(s_lane[i].th); }

    CHECK(s_ndone == NJOBS);
    CHECK(s_errors == 0);
    for (int j = 0; j < NJOBS; j++) {
        CHECK(s_job[j].done && s_job[j].seq > 0);
        for (int d = 0; d < s_job[j].ndep; d++) CHECK(s_job[s_job[j].dep[d]].seq < s_job[j].seq);
    }
    SYSTEM_INFO si; GetSystemInfo(&si);
    if (si.dwNumberOfProcessors >= 4) CHECK(s_maxrun >= 2);
    printf("scheduler model: %d jobs in %lld ms, up to %ld lanes at once, %ld errors\n", NJOBS,
           (long long)((t1.QuadPart - t0.QuadPart) * 1000 / s_freq.QuadPart), (long)s_maxrun, (long)s_errors);
}
#endif

int main(void)
{
    test_parse();
    test_placement();
#ifdef _WIN32
    test_scheduler();
#endif
    if (g_fail) { fprintf(stderr, "spurs_mt: FAILED\n"); return 1; }
    printf("spurs_mt: ok\n");
    return 0;
}
