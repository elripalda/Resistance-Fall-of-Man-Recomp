/* AMD FSR 3 frame generation (frame interpolation, 2x) -- the fallback for GPUs without DLSS Frame Generation.
 * Included by rsx_d3d12_backend.c right after rsx_fsr.inc (it uses its FidelityFX loader helpers). Selection,
 * the HUD split and the on/off rules are shared with DLSS-G (rsx_dlssg.inc, rsx_dlssg_hud.inc):
 *
 *   RFOM_DLSS_FG      2 / 3 / 4: frame generation (FSR 3 makes 2x only) | fsr3: FSR 3 frame generation 2x
 *   RFOM_FG_BACKEND   auto (default): DLSS-G when usable, else FSR 3 | dlss: DLSS-G only | fsr3: FSR 3 only
 *   RFOM_FG_FSR3DBG   1: FSR 3's own reset indicators and tear lines drawn on the output (diagnostic)
 *
 * Needs amd_fidelityfx_loader_dx12.dll + amd_fidelityfx_framegeneration_dx12.dll (AMD FidelityFX SDK 2.3.0,
 * MIT) beside the exe, any temporal anti-aliasing / upscaling mode (FSR 3.1, DLSS or XeSS: their depth,
 * motion vectors, jitter and reset are reused), the game program, and SDR or HDR10.
 *
 * Wiring: the swap chain is FidelityFX's frame-interpolation swap chain (created for the window on the
 * native queue). Interpolation runs in Present on the game queue (no async compute), paced on FidelityFX's
 * presenter thread. The presented frame keeps the HUD and the OSD; the generator is handed the HUD-less
 * image instead (frameGenerationCallback swaps presentColor), and the UI layer (HUD + OSD, premultiplied)
 * is drawn over generated frames only, by our presentCallback -- the same result as DLSS-G's UI
 * recomposition, and real frames stay exactly what the renderer drew. */

/* Official FidelityFX SDK 2.3.0 frame-generation headers (MIT, vendored unmodified in
 * ps3recomp/third_party/FidelityFX-SDK-2.3.0, the same release as the shipped amd_fidelityfx_*_dx12.dll).
 * The api headers are already included by rsx_fsr.inc. The short local names below are pure aliases. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#include "../../third_party/FidelityFX-SDK-2.3.0/framegeneration/include/ffx_framegeneration.h"
#include "../../third_party/FidelityFX-SDK-2.3.0/framegeneration/include/dx12/ffx_api_framegeneration_dx12.h"
#pragma clang diagnostic pop

#define F3_CREATE_FG        FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION
#define F3_CONFIGURE_FG     FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION
#define F3_DISPATCH_FG      FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION
#define F3_PREPARE_V2       FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_V2
#define F3_CREATE_FG_VER    FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_VERSION
#define F3_FG_VERSION       FFX_FRAMEGENERATION_VERSION
#define F3_SC_FOR_HWND      FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_FOR_HWND_DX12
#define F3_SC_REGISTER_UI   FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_REGISTERUIRESOURCE_DX12
#define F3_SC_WAIT_PRESENTS FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_WAIT_FOR_PRESENTS_DX12
#define F3_SC_VER           FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_VERSION_DX12
#define F3_SC_VERSION       FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION
#define F3_ENABLE_HDR       FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE
#define F3_FLAG_DBG_TEAR    FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES
#define F3_FLAG_DBG_RESET   FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS
#define F3_UI_PREMUL        FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA
#define F3_UI_DOUBLE_BUF    FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING

#define f3CreateFg    ffxCreateContextDescFrameGeneration
#define f3CreateVer   ffxCreateContextDescFrameGenerationVersion
#define f3ConfigureFg ffxConfigureDescFrameGeneration
#define f3PrepareV2   ffxDispatchDescFrameGenerationPrepareV2
#define f3ScForHwnd   ffxCreateContextDescFrameGenerationSwapChainForHwndDX12
#define f3ScVer       ffxCreateContextDescFrameGenerationSwapChainVersionDX12
#define f3ScRegUi     ffxConfigureDescFrameGenerationSwapChainRegisterUiResourceDX12
typedef ffxCallbackDescFrameGenerationPresent f3PresentCb;
typedef ffxDispatchDescFrameGeneration        f3DispatchFg;
typedef FfxApiPresentCallbackFunc             f3PresentFn;
typedef FfxApiFrameGenerationDispatchFunc     f3GenerateFn;

/* Pin the x64 ABI this code and amd_fidelityfx_framegeneration_dx12.dll 2.3.0 agree on (values measured with
 * both clang C17 and MSVC C++17 against the official headers). A header bump that moves anything fails here. */
#define F3_SA(c) _Static_assert(c, #c)
F3_SA(FFX_FRAMEGENERATION_VERSION == ((4u << 22) | (0u << 12) | 1u));
F3_SA(FFX_FRAMEGENERATION_SWAPCHAIN_DX12_VERSION == ((3u << 22) | (1u << 12) | 7u));
F3_SA(sizeof(ffxApiHeader) == 16 && sizeof(struct FfxApiResource) == 48);
F3_SA(offsetof(struct FfxApiResource, description) == 8 && offsetof(struct FfxApiResource, state) == 40);
F3_SA(sizeof(struct ffxCreateBackendDX12Desc) == 24 && offsetof(struct ffxCreateBackendDX12Desc, device) == 16);
F3_SA(sizeof(struct f3CreateFg) == 40 && offsetof(struct f3CreateFg, flags) == 16 && offsetof(struct f3CreateFg, displaySize) == 20
      && offsetof(struct f3CreateFg, maxRenderSize) == 28 && offsetof(struct f3CreateFg, backBufferFormat) == 36);
F3_SA(sizeof(struct f3CreateVer) == 24 && offsetof(struct f3CreateVer, version) == 16);
F3_SA(sizeof(struct f3ScVer) == 24 && offsetof(struct f3ScVer, version) == 16);
F3_SA(sizeof(f3PresentCb) == 192 && offsetof(f3PresentCb, commandList) == 24 && offsetof(f3PresentCb, currentBackBuffer) == 32
      && offsetof(f3PresentCb, currentUI) == 80 && offsetof(f3PresentCb, outputSwapChainBuffer) == 128
      && offsetof(f3PresentCb, isGeneratedFrame) == 176 && offsetof(f3PresentCb, frameID) == 184);
F3_SA(sizeof(f3DispatchFg) == 312 && offsetof(f3DispatchFg, commandList) == 16 && offsetof(f3DispatchFg, presentColor) == 24
      && offsetof(f3DispatchFg, reset) == 268 && offsetof(f3DispatchFg, generationRect) == 284 && offsetof(f3DispatchFg, frameID) == 304);
F3_SA(sizeof(struct f3ConfigureFg) == 144 && offsetof(struct f3ConfigureFg, swapChain) == 16
      && offsetof(struct f3ConfigureFg, presentCallback) == 24 && offsetof(struct f3ConfigureFg, frameGenerationCallback) == 40
      && offsetof(struct f3ConfigureFg, frameGenerationEnabled) == 56 && offsetof(struct f3ConfigureFg, allowAsyncWorkloads) == 57
      && offsetof(struct f3ConfigureFg, HUDLessColor) == 64 && offsetof(struct f3ConfigureFg, flags) == 112
      && offsetof(struct f3ConfigureFg, onlyPresentGenerated) == 116 && offsetof(struct f3ConfigureFg, generationRect) == 120
      && offsetof(struct f3ConfigureFg, frameID) == 136);
F3_SA(sizeof(struct f3PrepareV2) == 232 && offsetof(struct f3PrepareV2, frameID) == 16 && offsetof(struct f3PrepareV2, commandList) == 32
      && offsetof(struct f3PrepareV2, renderSize) == 40 && offsetof(struct f3PrepareV2, jitterOffset) == 48
      && offsetof(struct f3PrepareV2, motionVectorScale) == 56 && offsetof(struct f3PrepareV2, frameTimeDelta) == 64
      && offsetof(struct f3PrepareV2, reset) == 68 && offsetof(struct f3PrepareV2, cameraNear) == 72
      && offsetof(struct f3PrepareV2, viewSpaceToMetersFactor) == 84 && offsetof(struct f3PrepareV2, depth) == 88
      && offsetof(struct f3PrepareV2, motionVectors) == 136 && offsetof(struct f3PrepareV2, cameraForward) == 220);
F3_SA(sizeof(struct f3ScForHwnd) == 64 && offsetof(struct f3ScForHwnd, swapchain) == 16 && offsetof(struct f3ScForHwnd, hwnd) == 24
      && offsetof(struct f3ScForHwnd, desc) == 32 && offsetof(struct f3ScForHwnd, dxgiFactory) == 48
      && offsetof(struct f3ScForHwnd, gameQueue) == 56);
F3_SA(sizeof(struct f3ScRegUi) == 72 && offsetof(struct f3ScRegUi, uiResource) == 16 && offsetof(struct f3ScRegUi, flags) == 64);
F3_SA(sizeof(struct ffxDispatchDescFrameGenerationSwapChainWaitForPresentsDX12) == 16);
#undef F3_SA

static PfnFfxCreateContext  p_f3Create;
static PfnFfxDestroyContext p_f3Destroy;
static PfnFfxConfigure      p_f3Configure;
static PfnFfxDispatch       p_f3Dispatch;
static ffxContext s_f3_sc_ctx, s_f3_ctx;
static IDXGISwapChain4* s_f3_sc4;           /* the frame-interpolation swap chain (also s_d3d.swap_chain) */
static u32 s_f3_dw, s_f3_dh, s_f3_mw, s_f3_mh, s_f3_hdr, s_f3_ctx_fail;
static u64 s_f3_fid;                         /* frame ID of the last configured present */
static int s_f3_streak, s_f3_on, s_f3_prev_split, s_f3_dbgflags;
static u32 s_f3_presents, s_f3_gen_n;
static ID3D12DescriptorHeap *s_f3_srv_heap, *s_f3_rtv_heap;   /* presenter thread only */
static UINT s_f3_srv_inc, s_f3_rtv_inc, s_f3_slot;
#define F3_RING 16u
static struct { volatile int split, reset; ID3D12Resource* hsc; u32 w, h; } s_f3_ring[F3_RING];

static void f3_log(const char* m) { fprintf(stderr, "[FSR3-FG] %s%c", m, 10); }

static D3D12_RESOURCE_STATES f3_d3d_state(uint32_t s)
{
    switch (s) {
    case FFX_API_RESOURCE_STATE_UNORDERED_ACCESS:   return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case FFX_API_RESOURCE_STATE_COMPUTE_READ:       return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case FFX_API_RESOURCE_STATE_PIXEL_READ:         return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case FFX_API_RESOURCE_STATE_COPY_SRC:           return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case FFX_API_RESOURCE_STATE_COPY_DEST:          return D3D12_RESOURCE_STATE_COPY_DEST;
    case FFX_API_RESOURCE_STATE_GENERIC_READ:       return D3D12_RESOURCE_STATE_GENERIC_READ;
    case FFX_API_RESOURCE_STATE_PRESENT:            return D3D12_RESOURCE_STATE_PRESENT;
    case FFX_API_RESOURCE_STATE_RENDER_TARGET:      return D3D12_RESOURCE_STATE_RENDER_TARGET;
    default:                                        return D3D12_RESOURCE_STATE_COMMON;
    }
}

static void f3_bar(D3D12_RESOURCE_BARRIER* b, ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    memset(b, 0, sizeof *b);
    b->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b->Transition.pResource = r; b->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b->Transition.StateBefore = from; b->Transition.StateAfter = to;
}

/* fg_load (rsx_dlssg.inc), game program only, before the queue / swap chain exist: are the FidelityFX frame
 * generation DLLs there and is there something to interpolate? */
static int f3_load(void)
{
    if (fsr_mode() < 1) {
        f3_log("frame generation disabled: it needs a temporal anti-aliasing / upscaling mode (Anti-aliasing = FSR 3.1, NVIDIA DLSS or Intel XeSS)");
        return 0;
    }
    { const char* h = hdr_env("HDR");
      if (h && (!_stricmp(h, "scrgb") || !strcmp(h, "2"))) { f3_log("frame generation disabled: not available with scRGB HDR output (use HDR10 or SDR)"); return 0; } }
    wchar_t dir[MAX_PATH]; DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { f3_log("frame generation disabled: cannot locate the program folder"); return 0; }
    wchar_t* bs = wcsrchr(dir, L'\\'); if (bs) *bs = 0;
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%ls\\amd_fidelityfx_framegeneration_dx12.dll", dir); path[MAX_PATH - 1] = 0;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) {
        f3_log("frame generation disabled: amd_fidelityfx_framegeneration_dx12.dll is missing (run Setup.bat again, or copy it from the AMD FidelityFX SDK 2.3.0 into the game folder)");
        return 0;
    }
    _snwprintf(path, MAX_PATH, L"%ls\\amd_fidelityfx_loader_dx12.dll", dir); path[MAX_PATH - 1] = 0;
    HMODULE m = LoadLibraryW(path);
    if (!m) { f3_log("frame generation disabled: amd_fidelityfx_loader_dx12.dll is missing"); return 0; }
    p_f3Create = (PfnFfxCreateContext)(void*)GetProcAddress(m, "ffxCreateContext");
    p_f3Destroy = (PfnFfxDestroyContext)(void*)GetProcAddress(m, "ffxDestroyContext");
    p_f3Configure = (PfnFfxConfigure)(void*)GetProcAddress(m, "ffxConfigure");
    p_f3Dispatch = (PfnFfxDispatch)(void*)GetProcAddress(m, "ffxDispatch");
    if (!p_f3Create || !p_f3Destroy || !p_f3Configure || !p_f3Dispatch) {
        f3_log("frame generation disabled: amd_fidelityfx_loader_dx12.dll is too old");
        return 0;
    }
    { const char* e = getenv("RFOM_FG_FSR3DBG"); s_f3_dbgflags = (e && atoi(e)) ? (int)(F3_FLAG_DBG_TEAR | F3_FLAG_DBG_RESET) : 0; }
    s_f3_want = 1;
    return 1;
}

static void f3_wait_presents(void)
{
    if (!s_f3_sc_ctx) return;
    ffxDispatchDescHeader w; memset(&w, 0, sizeof w);
    w.type = F3_SC_WAIT_PRESENTS;
    p_f3Dispatch(&s_f3_sc_ctx, &w);
}

static void f3_configure(int enabled, int callbacks, const struct FfxApiRect2D* rect);

/* Frame generation context: display size, the largest render size and HDR10 or not. */
static void f3_ctx_drop(void)
{
    if (!s_f3_ctx) return;
    f3_configure(0, 0, NULL);   /* the swap chain must not call into the context any more */
    f3_wait_presents();
    p_f3Destroy(&s_f3_ctx, NULL);
    s_f3_ctx = NULL; s_f3_on = 0; s_f3_streak = 0;
}

static int f3_ctx_make(u32 mw, u32 mh)
{
    struct ffxCreateBackendDX12Desc be; memset(&be, 0, sizeof be);
    be.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12; be.device = s_d3d.device;
    struct f3CreateVer ver; memset(&ver, 0, sizeof ver);
    ver.header.type = F3_CREATE_FG_VER; ver.header.pNext = &be.header; ver.version = F3_FG_VERSION;
    struct f3CreateFg cd; memset(&cd, 0, sizeof cd);
    cd.header.type = F3_CREATE_FG; cd.header.pNext = &ver.header;
    cd.flags = g_pc_hdr_active ? F3_ENABLE_HDR : 0u;
    cd.displaySize.width = s_sc_w; cd.displaySize.height = s_sc_h;
    cd.maxRenderSize.width = mw; cd.maxRenderSize.height = mh;
    cd.backBufferFormat = fsr_ffx_fmt(s_sc_fmt);
    const ffxReturnCode_t rc = p_f3Create(&s_f3_ctx, &cd.header, NULL);
    if (rc != FFX_API_RETURN_OK || !s_f3_ctx) {
        s_f3_ctx = NULL;
        fprintf(stderr, "[FSR3-FG] frame generation disabled: the frame generation context could not be created (error %u; GPU or driver not supported)%c", (unsigned)rc, 10);
        return 0;
    }
    s_f3_dw = s_sc_w; s_f3_dh = s_sc_h; s_f3_mw = mw; s_f3_mh = mh; s_f3_hdr = (u32)g_pc_hdr_active;
    return 1;
}

/* init_d3d12, instead of CreateSwapChainForHwnd: the frame-interpolation swap chain on the native queue, and
 * a first frame generation context as the support check. On any failure everything is undone and the
 * normal swap chain is made (frame generation off). */
static int f3_create_swapchain(IDXGIFactory4* factory, DXGI_SWAP_CHAIN_DESC1* desc, IDXGISwapChain1** out)
{
    if (!s_f3_want) return 0;
    s_f3_want = 0;
    IDXGISwapChain4* sc4 = NULL;
    struct f3ScVer ver; memset(&ver, 0, sizeof ver);
    ver.header.type = F3_SC_VER; ver.version = F3_SC_VERSION;
    struct f3ScForHwnd cd; memset(&cd, 0, sizeof cd);
    cd.header.type = F3_SC_FOR_HWND; cd.header.pNext = &ver.header;
    cd.swapchain = &sc4; cd.hwnd = s_d3d.hwnd; cd.desc = desc; cd.fullscreenDesc = NULL;
    cd.dxgiFactory = (IDXGIFactory*)factory; cd.gameQueue = s_d3d.cmd_queue;
    ffxReturnCode_t rc = p_f3Create(&s_f3_sc_ctx, &cd.header, NULL);
    if (rc != FFX_API_RETURN_OK || !sc4) {
        if (s_f3_sc_ctx) p_f3Destroy(&s_f3_sc_ctx, NULL);
        s_f3_sc_ctx = NULL;
        fprintf(stderr, "[FSR3-FG] frame generation disabled: the frame interpolation swap chain could not be created (error %u)%c", (unsigned)rc, 10);
        return 0;
    }
    s_f3_sc4 = sc4;
    if (!f3_ctx_make(SC(s_d3d.width), SC(s_d3d.height))) {
        s_f3_sc4 = NULL;
        p_f3Destroy(&s_f3_sc_ctx, NULL); s_f3_sc_ctx = NULL;
        sc4->lpVtbl->Release(sc4);
        return 0;
    }
    if (FAILED(sc4->lpVtbl->QueryInterface(sc4, &IID_IDXGISwapChain1, (void**)out)) || !*out) {
        f3_ctx_drop();
        s_f3_sc4 = NULL;
        p_f3Destroy(&s_f3_sc_ctx, NULL); s_f3_sc_ctx = NULL;
        sc4->lpVtbl->Release(sc4);
        f3_log("frame generation disabled: the frame interpolation swap chain has no IDXGISwapChain1");
        return 0;
    }
    sc4->lpVtbl->Release(sc4);   /* the swap chain context and *out keep it */
    s_f3_ready = 1;
    fprintf(stderr, "[FSR3-FG] AMD FSR 3 frame generation ready: 2x, HUD split %s%s%c",
            s_fg_split ? "on" : "off (RFOM_FG_HUDSPLIT=0)", s_fg_dbg ? ", debug view on (RFOM_FG_HUDDBG)" : "", 10);
    return 1;
}

/* FidelityFX's frame generation dispatch, from Present (render thread, game queue): the generator gets the
 * HUD-less image of this frame when the split ran, and our reset on top of FidelityFX's own. */
static ffxReturnCode_t f3_generate_cb(f3DispatchFg* d, void* user)
{
    (void)user;
    const u32 i = (u32)(d->frameID % F3_RING);
    if (s_f3_ring[i].split && s_f3_ring[i].hsc)
        d->presentColor = fsr_res(s_f3_ring[i].hsc, s_sc_fmt, s_f3_ring[i].w, s_f3_ring[i].h,
                                  FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_PIXEL_READ);
    if (s_f3_ring[i].reset) d->reset = true;
    return p_f3Dispatch(&s_f3_ctx, &d->header);
}

/* FidelityFX's present callback, once per presented frame (presenter thread while generation runs): the
 * frame as is; the UI layer over generated frames when the split ran for that frame. */
static ffxReturnCode_t f3_present_cb(f3PresentCb* p, void* user)
{
    (void)user;
    ID3D12GraphicsCommandList* cl = (ID3D12GraphicsCommandList*)p->commandList;
    ID3D12Resource* src = (ID3D12Resource*)p->currentBackBuffer.resource;
    ID3D12Resource* dst = (ID3D12Resource*)p->outputSwapChainBuffer.resource;
    ID3D12Resource* ui = (ID3D12Resource*)p->currentUI.resource;
    if (!cl || !src || !dst) return FFX_API_RETURN_ERROR_PARAMETER;
    const D3D12_RESOURCE_STATES ss = f3_d3d_state(p->currentBackBuffer.state), ds = f3_d3d_state(p->outputSwapChainBuffer.state);
    D3D12_RESOURCE_BARRIER b[2];
    f3_bar(&b[0], src, ss, D3D12_RESOURCE_STATE_COPY_SOURCE);
    f3_bar(&b[1], dst, ds, D3D12_RESOURCE_STATE_COPY_DEST);
    cl->lpVtbl->ResourceBarrier(cl, ss == D3D12_RESOURCE_STATE_COPY_SOURCE ? 1 : 2, ss == D3D12_RESOURCE_STATE_COPY_SOURCE ? &b[1] : b);
    cl->lpVtbl->CopyResource(cl, dst, src);
    const u32 i = (u32)(p->frameID % F3_RING);
    D3D12_RESOURCE_STATES dnow = D3D12_RESOURCE_STATE_COPY_DEST;
    if (p->isGeneratedFrame && s_f3_ring[i].split && ui && s_fg_over_pso && s_f3_srv_heap) {
        const D3D12_RESOURCE_STATES us = f3_d3d_state(p->currentUI.state);
        int nb = 0;
        f3_bar(&b[nb++], dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        if (!(us & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) f3_bar(&b[nb++], ui, us, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cl->lpVtbl->ResourceBarrier(cl, (UINT)nb, b);
        dnow = D3D12_RESOURCE_STATE_RENDER_TARGET;
        const UINT k = s_f3_slot++ % F3_RING;
        D3D12_CPU_DESCRIPTOR_HANDLE sh, rh; D3D12_GPU_DESCRIPTOR_HANDLE sg;
        s_f3_srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(s_f3_srv_heap, &sh);
        s_f3_srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(s_f3_srv_heap, &sg);
        s_f3_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(s_f3_rtv_heap, &rh);
        sh.ptr += (SIZE_T)k * s_f3_srv_inc; sg.ptr += (UINT64)k * s_f3_srv_inc; rh.ptr += (SIZE_T)k * s_f3_rtv_inc;
        D3D12_SHADER_RESOURCE_VIEW_DESC sv; memset(&sv, 0, sizeof sv);
        D3D12_RESOURCE_DESC ud; ui->lpVtbl->GetDesc(ui, &ud);
        sv.Format = ud.Format; sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sv.Texture2D.MipLevels = 1;
        s_d3d.device->lpVtbl->CreateShaderResourceView(s_d3d.device, ui, &sv, sh);
        s_d3d.device->lpVtbl->CreateRenderTargetView(s_d3d.device, dst, NULL, rh);
        D3D12_RESOURCE_DESC od; dst->lpVtbl->GetDesc(dst, &od);
        const D3D12_VIEWPORT vp = { 0.0f, 0.0f, (float)od.Width, (float)od.Height, 0.0f, 1.0f };
        const D3D12_RECT sc = { 0, 0, (LONG)od.Width, (LONG)od.Height };
        cl->lpVtbl->OMSetRenderTargets(cl, 1, &rh, FALSE, NULL);
        cl->lpVtbl->RSSetViewports(cl, 1, &vp);
        cl->lpVtbl->RSSetScissorRects(cl, 1, &sc);
        cl->lpVtbl->SetGraphicsRootSignature(cl, s_blit_rs);
        cl->lpVtbl->SetDescriptorHeaps(cl, 1, &s_f3_srv_heap);
        cl->lpVtbl->SetPipelineState(cl, s_fg_over_pso);
        cl->lpVtbl->IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cl->lpVtbl->SetGraphicsRootDescriptorTable(cl, 1, sg);
        cl->lpVtbl->DrawInstanced(cl, 3, 1, 0, 0);
        if (!(us & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) {
            f3_bar(&b[0], ui, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, us);
            cl->lpVtbl->ResourceBarrier(cl, 1, b);
        }
    }
    {
        int nb = 0;
        if (ss != D3D12_RESOURCE_STATE_COPY_SOURCE) f3_bar(&b[nb++], src, D3D12_RESOURCE_STATE_COPY_SOURCE, ss);
        if (ds != dnow) f3_bar(&b[nb++], dst, dnow, ds);
        if (nb) cl->lpVtbl->ResourceBarrier(cl, (UINT)nb, b);
    }
    return FFX_API_RETURN_OK;
}

static void f3_configure(int enabled, int callbacks, const struct FfxApiRect2D* rect)
{
    struct f3ConfigureFg c; memset(&c, 0, sizeof c);
    c.header.type = F3_CONFIGURE_FG;
    c.swapChain = s_f3_sc4;
    if (callbacks) { c.presentCallback = f3_present_cb; c.frameGenerationCallback = f3_generate_cb; }
    c.frameGenerationEnabled = enabled != 0;
    c.allowAsyncWorkloads = false;
    c.flags = (uint32_t)s_f3_dbgflags;
    if (rect) c.generationRect = *rect;
    else { c.generationRect.width = (int32_t)s_sc_w; c.generationRect.height = (int32_t)s_sc_h; }
    c.frameID = s_f3_fid;
    const ffxReturnCode_t rc = p_f3Configure(&s_f3_ctx, &c.header);
    if (rc != FFX_API_RETURN_OK) { static int n = 0; if (n < 3 && ++n) fprintf(stderr, "[FSR3-FG] configure failed (%u)%c", (unsigned)rc, 10); }
}

/* rsx_fsr.inc, after the anti-aliasing / upscaling pass of a frame (any of FSR 3.1, DLSS, XeSS), on the
 * frame's command list: the same depth and motion vectors (top-left rw x rh of w x h, UV units), jitter and
 * reset go to FidelityFX's frame generation for the coming present. */
static void f3_prepare(ID3D12GraphicsCommandList* cl, ID3D12Resource* z, D3D12_RESOURCE_STATES zst, ID3D12Resource* mv,
                       D3D12_RESOURCE_STATES mst, u32 w, u32 h, u32 rw, u32 rh, float jx, float jy, float dt, int reset)
{
    if (!s_f3_ready) return;
    const D3D12_RESOURCE_STATES rd = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (zst != rd || mst != rd) { static int n = 0; if (!n++) f3_log("depth / motion vectors in an unexpected state -- frame skipped"); return; }
    if (s_f3_ctx && (s_f3_dw != s_sc_w || s_f3_dh != s_sc_h || w > s_f3_mw || h > s_f3_mh || s_f3_hdr != (u32)g_pc_hdr_active)) {
        wait_for_gpu();
        f3_ctx_drop();
    }
    if (!s_f3_ctx) {
        if (s_f3_ctx_fail) return;
        if (!f3_ctx_make(w > SC(s_d3d.width) ? w : SC(s_d3d.width), h > SC(s_d3d.height) ? h : SC(s_d3d.height))) {
            s_f3_ctx_fail = 1;
            return;
        }
    }
    struct f3PrepareV2 pd; memset(&pd, 0, sizeof pd);
    pd.header.type = F3_PREPARE_V2;
    pd.frameID = s_f3_fid + 1u;
    pd.commandList = cl;
    pd.renderSize.width = rw; pd.renderSize.height = rh;
    pd.jitterOffset.x = jx; pd.jitterOffset.y = jy;
    pd.motionVectorScale.x = (float)rw; pd.motionVectorScale.y = (float)rh;
    pd.frameTimeDelta = dt;
    pd.reset = reset != 0;
    pd.cameraNear = 0.1f; pd.cameraFar = 1000.0f; pd.cameraFovAngleVertical = 1.0f; pd.viewSpaceToMetersFactor = 1.0f;
    pd.depth = fsr_res(z, DXGI_FORMAT_R32_FLOAT, w, h, FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    pd.motionVectors = fsr_res(mv, DXGI_FORMAT_R16G16_FLOAT, w, h, FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    const ffxReturnCode_t rc = p_f3Dispatch(&s_f3_ctx, &pd.header);
    if (rc != FFX_API_RETURN_OK) { static int n = 0; if (n < 3 && ++n) fprintf(stderr, "[FSR3-FG] prepare failed (%u)%c", (unsigned)rc, 10); return; }
    s_fgh.tagged = 1;
    s_f3_ring[(u32)((s_f3_fid + 1u) % F3_RING)].reset = reset != 0;
}

static int f3_heaps(void)
{
    if (s_f3_srv_heap) return 1;
    D3D12_DESCRIPTOR_HEAP_DESC hd; memset(&hd, 0, sizeof hd);
    hd.NumDescriptors = F3_RING; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(s_d3d.device->lpVtbl->CreateDescriptorHeap(s_d3d.device, &hd, &IID_ID3D12DescriptorHeap, (void**)&s_f3_rtv_heap))) { s_f3_rtv_heap = NULL; return 0; }
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ID3D12DescriptorHeap* sh = NULL;
    if (FAILED(s_d3d.device->lpVtbl->CreateDescriptorHeap(s_d3d.device, &hd, &IID_ID3D12DescriptorHeap, (void**)&sh))) return 0;
    s_f3_rtv_inc = s_d3d.device->lpVtbl->GetDescriptorHandleIncrementSize(s_d3d.device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    s_f3_srv_inc = s_d3d.device->lpVtbl->GetDescriptorHandleIncrementSize(s_d3d.device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    s_f3_srv_heap = sh;
    return 1;
}

/* fg_present, after the HUD-less / UI images were built (split: hsc / usc hold this frame's pair): the UI
 * layer goes to the swap chain, generation on or off for this present, then the configure for its frame ID. */
static void f3_present(const D3D12_VIEWPORT* vp, int split, int* vs)
{
    if (!s_f3_ready) return;
    /* Same rule as the DLSS-G path: the frame-interpolation swap chain paces generated + real frames itself, so
     * a sync interval > 1 would halve the output rate; clamp to 1 (0 = no vsync / VRR stays as requested). */
    if (s_f3_ctx && vs && *vs > 1) {
        static int n = 0; if (!n++) f3_log("sync interval > 1 is not supported with frame generation -- using 1");
        *vs = 1;
    }
    const u64 fid = ++s_f3_fid;
    const u32 i = (u32)(fid % F3_RING);
    const int ui_on = fg_ui_on();
    const int want = s_fgh.tagged && s_f3_ctx && s_app_active && !IsIconic(s_d3d.hwnd) && (!ui_on || split);
    s_f3_streak = want ? s_f3_streak + 1 : 0;
    const int on = s_f3_streak >= 2;
    const int sp = on && split && f3_heaps();
    if (!s_fgh.tagged) s_f3_ring[i].reset = 1;
    if (sp != s_f3_prev_split || (on && !s_f3_on)) s_f3_ring[i].reset = 1;
    s_f3_ring[i].split = sp; s_f3_ring[i].hsc = sp ? s_fg_hsc : NULL; s_f3_ring[i].w = s_sc_w; s_f3_ring[i].h = s_sc_h;
    s_f3_prev_split = sp;
    if (on != s_f3_on) {
        static int n = 0;
        if (n < 6) { n++; if (on) fprintf(stderr, "[FSR3-FG] frame generation on: 2x, %ux%u%s%c", s_sc_w, s_sc_h, sp ? ", HUD kept out of generated frames" : "", 10);
                     else f3_log("frame generation off (no anti-aliasing inputs this frame, window in the background, or resize)"); }
    }
    s_f3_on = on;
    if (!s_f3_ctx) { s_f3_ring[(u32)((fid + 1u) % F3_RING)].reset = 0; return; }
    struct f3ScRegUi ru; memset(&ru, 0, sizeof ru);
    ru.header.type = F3_SC_REGISTER_UI;
    if (sp && s_fg_usc) {
        ru.uiResource = fsr_res(s_fg_usc, s_sc_fmt, s_sc_w, s_sc_h, FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_PIXEL_READ);
        ru.flags = F3_UI_PREMUL | F3_UI_DOUBLE_BUF;
    }
    p_f3Configure(&s_f3_sc_ctx, &ru.header);
    struct FfxApiRect2D r;
    r.left = (int32_t)(vp->TopLeftX + 0.5f); r.top = (int32_t)(vp->TopLeftY + 0.5f);
    r.width = (int32_t)(vp->Width + 0.5f); r.height = (int32_t)(vp->Height + 0.5f);
    if (r.left + r.width > (int32_t)s_sc_w) r.width = (int32_t)s_sc_w - r.left;
    if (r.top + r.height > (int32_t)s_sc_h) r.height = (int32_t)s_sc_h - r.top;
    f3_configure(on, 1, &r);
    s_f3_ring[(u32)((fid + 1u) % F3_RING)].reset = 0;   /* f3_prepare sets it for the next frame */
    if (on) s_f3_gen_n++;
    if ((++s_f3_presents % 6000u) == 0u)
        fprintf(stderr, "[FSR3-FG] %u of the last 6000 presents had a generated frame; HUD split %u of %u frames%c",
                s_f3_gen_n, s_fg_hud_ok, s_fg_hud_ok + s_fg_hud_fb, 10), s_f3_gen_n = 0;
}

/* sc_resize (GPU idle, before ResizeBuffers) and shutdown. */
static void f3_before_resize(void)
{
    if (!s_f3_ready) return;
    f3_ctx_drop();
    f3_wait_presents();
    s_f3_ctx_fail = 0;
}

static void f3_shutdown(void)
{
    if (!s_f3_ready) return;
    f3_ctx_drop();
    { struct f3ScRegUi ru; memset(&ru, 0, sizeof ru); ru.header.type = F3_SC_REGISTER_UI; p_f3Configure(&s_f3_sc_ctx, &ru.header); }
    f3_wait_presents();
    p_f3Destroy(&s_f3_sc_ctx, NULL);
    s_f3_sc_ctx = NULL; s_f3_sc4 = NULL;
    s_f3_ready = 0;
}
