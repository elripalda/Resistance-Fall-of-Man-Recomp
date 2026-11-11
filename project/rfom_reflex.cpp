// Resistance: Fall of Man PC port: NVIDIA Reflex Low Latency through NVAPI (tools/nvapi, MIT).
//
// RFOM_REFLEX   0 / unset: off | 1: Reflex On | 2: Reflex On + Boost
//
// On an NVIDIA GPU (driver with Reflex support) the D3D12 device gets
// NvAPI_D3D_SetSleepMode and the game thread calls NvAPI_D3D_Sleep once per
// frame, right after the port's frame limiter and before the next frame's
// simulation and input sampling. minimumIntervalUs is always 0: Reflex never
// limits the frame rate, the port's limiter (RFOM_FPS) stays in charge.
// Latency markers (one frame id per game frame, strictly increasing):
//   SIMULATION_START / SIMULATION_END    game thread, rfom_frame_wait (stubs.cpp)
//   INPUT_SAMPLE                         first port-0 cellPadGetData of the frame
//   RENDERSUBMIT_START / _END            RSX render thread, first batch of the frame / before present
//   PRESENT_START / PRESENT_END          around IDXGISwapChain::Present
// The render side runs a frame or so behind the game thread: game frame ids
// are queued at SIMULATION_END and taken by the render side when it starts
// recording that frame, so the driver can match the two halves.
// Anywhere NVAPI or Reflex is unavailable (other GPU vendors, old drivers) the
// renderer falls back to its frame-latency-waitable swap chain with one queued
// frame (rsx_d3d12_backend.c).
// With DLSS Frame Generation active (RFOM_DLSS_FG, rsx_dlssg.inc) the same markers and the sleep go through
// NVIDIA Streamline instead of NVAPI (DLSS-G requires Reflex and must see the markers): the renderer sets the
// g_pc_fg_* hooks before it calls g_pc_reflex_init, and Reflex is then always on.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if __has_include("nvapi.h")
#include "nvapi.h"
#define RFOM_HAVE_NVAPI 1
#else
#define RFOM_HAVE_NVAPI 0
#endif

extern "C" {
// rsx_d3d12_backend.c hooks
extern int (*volatile g_pc_reflex_init)(void* device);
extern void (*volatile g_pc_reflex_marker)(int type);
// rsx_dlssg.inc: Streamline Reflex / PCL (set only while DLSS Frame Generation is usable)
extern void (*volatile g_pc_fg_marker)(int type, unsigned long long id);
extern void (*volatile g_pc_fg_sleep)(unsigned long long id);
extern int (*volatile g_pc_fg_latency)(double* total_ms, double* render_ms, double* gpu_ms);
// cellPad.c: port-0 cellPadGetData tick (shared with the key-prompt hot-swap, stubs.cpp)
void rfom_pad_tick_install(void);
}

namespace {

std::atomic<int> s_mode{0};        // 0 off, 1 on, 2 on + boost (only set when NVAPI / Streamline accepted it)
bool s_fg = false;                 // markers and sleep go through Streamline (DLSS Frame Generation)
#if RFOM_HAVE_NVAPI
IUnknown* s_dev = nullptr;
#else
enum { SIMULATION_START, SIMULATION_END, RENDERSUBMIT_START, RENDERSUBMIT_END, PRESENT_START, PRESENT_END, INPUT_SAMPLE };
#endif

// game frame ids: s_sim is the frame being simulated (0 = none yet)
std::atomic<unsigned long long> s_sim{0};
std::atomic<int> s_input_done{0};
// SPSC ring: game thread pushes finished frames, the render side pops them
constexpr unsigned RING = 8;
unsigned long long s_ring[RING];
std::atomic<unsigned> s_head{0}, s_tail{0};
unsigned long long s_render_id = 0, s_render_last = 0;   // render side only
bool s_render_open = false;

std::mutex s_lat_mu;
double s_lat_total_ms = 0, s_lat_render_ms = 0, s_lat_gpu_ms = 0;
ULONGLONG s_lat_at = 0;

void marker(int t, unsigned long long id)
{
    if (!id) return;
    if (s_fg) {
        if (t != INPUT_SAMPLE && g_pc_fg_marker) g_pc_fg_marker(t, id);   // Streamline: no input-sample marker
        return;
    }
#if RFOM_HAVE_NVAPI
    NV_LATENCY_MARKER_PARAMS p = {};
    p.version = NV_LATENCY_MARKER_PARAMS_VER;
    p.frameID = id;
    p.markerType = static_cast<NV_LATENCY_MARKER_TYPE>(t);
    NvAPI_D3D_SetLatencyMarker(s_dev, &p);
#endif
}

int reflex_init(void* device)
{
    const char* e = getenv("RFOM_REFLEX");
    const int want = e ? atoi(e) : 0;
    if (g_pc_fg_marker && g_pc_fg_sleep) {
        // DLSS Frame Generation: Reflex is required and already configured in Streamline by the renderer
        s_fg = true;
        s_mode = want >= 2 ? 2 : 1;
        rfom_pad_tick_install();
        fprintf(stderr, "[reflex] NVIDIA Reflex Low Latency %s through Streamline (required by DLSS Frame Generation;"
                        " the frame limiter stays RFOM_FPS)%c", want >= 2 ? "On + Boost" : "On", 10);
        return 1;
    }
#if RFOM_HAVE_NVAPI
    if (want <= 0 || !device) return 0;
    if (NvAPI_Initialize() != NVAPI_OK) {
        fprintf(stderr, "[reflex] NVAPI unavailable (not an NVIDIA GPU/driver) -- using the waitable swap chain%c", 10);
        return 0;
    }
    s_dev = static_cast<IUnknown*>(device);
    NV_SET_SLEEP_MODE_PARAMS sp = {};
    sp.version = NV_SET_SLEEP_MODE_PARAMS_VER;
    sp.bLowLatencyMode = 1;
    sp.bLowLatencyBoost = want >= 2 ? 1 : 0;
    sp.minimumIntervalUs = 0;          // never a frame limiter
    sp.bUseMarkersToOptimize = 1;
    const NvAPI_Status st = NvAPI_D3D_SetSleepMode(s_dev, &sp);
    if (st != NVAPI_OK) {
        NvAPI_ShortString msg = {};
        NvAPI_GetErrorMessage(st, msg);
        fprintf(stderr, "[reflex] NvAPI_D3D_SetSleepMode failed (%d: %s) -- using the waitable swap chain%c", (int)st, msg, 10);
        s_dev = nullptr;
        return 0;
    }
    s_mode = want >= 2 ? 2 : 1;
    rfom_pad_tick_install();
    fprintf(stderr, "[reflex] NVIDIA Reflex Low Latency %s (NVAPI sleep + latency markers; the frame limiter stays RFOM_FPS)%c",
            want >= 2 ? "On + Boost" : "On", 10);
    return 1;
#else
    (void)device;
    return 0;
#endif
}

// render side (the RSX render thread, or the FIFO thread with RSX_ASYNC=0)
void render_marker(int type)
{
    if (!s_mode) return;
    switch (type) {
    case RENDERSUBMIT_START: {
        if (s_render_open) return;
        // the oldest finished game frame not yet presented; while the frame is
        // still being simulated (its draws stream to the RSX as it runs), that one
        unsigned long long id = 0;
        const unsigned t = s_tail.load(std::memory_order_relaxed);
        if (t != s_head.load(std::memory_order_acquire)) id = s_ring[t % RING];
        else id = s_sim.load(std::memory_order_relaxed);
        if (id <= s_render_last) id = s_render_last + 1;   // strictly increasing
        s_render_id = id;
        s_render_open = true;
        marker(RENDERSUBMIT_START, id);
        break; }
    case RENDERSUBMIT_END:
        if (!s_render_open) render_marker(RENDERSUBMIT_START);
        marker(RENDERSUBMIT_END, s_render_id);
        break;
    case PRESENT_START:
        if (!s_render_open) render_marker(RENDERSUBMIT_START);
        marker(PRESENT_START, s_render_id);
        break;
    case PRESENT_END: {
        if (!s_render_open) return;
        marker(PRESENT_END, s_render_id);
        s_render_last = s_render_id;
        s_render_open = false;
        // drop the presented frame (and anything older) from the queue
        unsigned t = s_tail.load(std::memory_order_relaxed);
        while (t != s_head.load(std::memory_order_acquire) && s_ring[t % RING] <= s_render_last) t++;
        s_tail.store(t, std::memory_order_release);
        break; }
    default: break;
    }
}

#if RFOM_HAVE_NVAPI
void lat_update()
{
    const ULONGLONG now = GetTickCount64();
    {
        std::lock_guard<std::mutex> lk(s_lat_mu);
        if (now - s_lat_at < 500) return;
        s_lat_at = now;
    }
    static NV_LATENCY_RESULT_PARAMS r;   // ~18 KB: not on the stack
    memset(&r, 0, sizeof r);
    r.version = NV_LATENCY_RESULT_PARAMS_VER;
    if (NvAPI_D3D_GetLatency(s_dev, &r) != NVAPI_OK) return;
    double tot = 0, ren = 0, gpu = 0; int n = 0;
    for (const auto& f : r.frameReport) {
        if (!f.frameID || !f.simStartTime || f.gpuRenderEndTime <= f.simStartTime) continue;
        const unsigned long long t0 = f.inputSampleTime && f.inputSampleTime < f.simStartTime + 1000000 ? f.inputSampleTime : f.simStartTime;
        if (f.gpuRenderEndTime <= t0) continue;
        tot += (double)(f.gpuRenderEndTime - t0) / 1000.0;
        if (f.renderSubmitEndTime > f.renderSubmitStartTime) ren += (double)(f.renderSubmitEndTime - f.renderSubmitStartTime) / 1000.0;
        gpu += f.gpuActiveRenderTimeUs / 1000.0;
        n++;
    }
    if (!n) return;
    std::lock_guard<std::mutex> lk(s_lat_mu);
    s_lat_total_ms = tot / n; s_lat_render_ms = ren / n; s_lat_gpu_ms = gpu / n;
}
#endif

struct HookInit
{
    HookInit() { g_pc_reflex_init = reflex_init; g_pc_reflex_marker = render_marker; }
} s_hook_init;

} // namespace

extern "C" {

// 0 off, 1 On, 2 On + Boost (NVAPI or Streamline active)
int rfom_reflex_mode(void) { return s_mode; }

// Game thread, entry of rfom_frame_wait: the frame's simulation and command submission are done.
void rfom_reflex_sim_end(void)
{
    if (!s_mode) return;
    const unsigned long long id = s_sim.load(std::memory_order_relaxed);
    if (!id) return;
    marker(SIMULATION_END, id);
    unsigned h = s_head.load(std::memory_order_relaxed);
    if (h - s_tail.load(std::memory_order_acquire) >= RING) return;   // the render side is far behind: skip
    s_ring[h % RING] = id;
    s_head.store(h + 1, std::memory_order_release);
}

// Game thread, after the port's frame limiter: Reflex sleep, then the next frame starts.
void rfom_reflex_sim_start(void)
{
    if (!s_mode) return;
    const unsigned long long id = s_sim.load(std::memory_order_relaxed) + 1;
    if (s_fg) { if (g_pc_fg_sleep) g_pc_fg_sleep(id); }
#if RFOM_HAVE_NVAPI
    else NvAPI_D3D_Sleep(s_dev);
#endif
    s_sim.store(id, std::memory_order_relaxed);
    s_input_done.store(0, std::memory_order_relaxed);
    marker(SIMULATION_START, id);
}

// Game thread, cellPadGetData (port 0): the first input read of the frame.
void rfom_reflex_input(void)
{
    if (!s_mode || s_input_done.exchange(1, std::memory_order_relaxed)) return;
    marker(INPUT_SAMPLE, s_sim.load(std::memory_order_relaxed));
}

// F3 overlay: average latency (input/sim start -> GPU done), render submit and GPU active time
// over the driver's last 64 frames. Returns 0 when Reflex is not active.
int rfom_reflex_stats(double* total_ms, double* render_ms, double* gpu_ms)
{
    if (s_fg) return s_mode && g_pc_fg_latency ? g_pc_fg_latency(total_ms, render_ms, gpu_ms) : 0;
#if RFOM_HAVE_NVAPI
    if (!s_mode) return 0;
    lat_update();
    std::lock_guard<std::mutex> lk(s_lat_mu);
    if (s_lat_total_ms <= 0) return 0;
    *total_ms = s_lat_total_ms; *render_ms = s_lat_render_ms; *gpu_ms = s_lat_gpu_ms;
    return 1;
#else
    (void)total_ms; (void)render_ms; (void)gpu_ms;
    return 0;
#endif
}

} // extern "C"
