/* DLSS Frame Generation (DLSS-G 2x, DLSS 4 Multi Frame Generation 3x / 4x) through NVIDIA Streamline,
 * loaded at run time from project/rfom_dlssg.dll (the bridge to the Streamline SDK, tools/streamline).
 * Included by rsx_d3d12_backend.c after rsx_osd.inc; the draw-loop HUD classifier is rsx_dlssg_hud.inc.
 *
 *   RFOM_DLSS_FG      0 / unset: off | 2 (or 1, on): 2x | 3: 3x | 4: 4x (clamped to what the GPU supports:
 *                     RTX 40 = 2x, RTX 50 = up to 4x). Only in the game program (ResistanceFoMGame.exe), only
 *                     with RSX_FSR=2 (NVIDIA DLSS), only in SDR or HDR10 (not scRGB).
 *   RFOM_FG_HUDSPLIT  1 (default): the HUD is kept out of frame generation (see rsx_dlssg_hud.inc) | 0: off,
 *                     generated frames then interpolate the HUD together with the scene.
 *   RFOM_FG_HUDDBG    1: show the HUD-less image the generator gets | 2: show the separated UI layer over grey.
 *                     Works whenever DLSS runs, also with frame generation off or unsupported.
 *   RFOM_FG_SLLOG     1: verbose Streamline log (streamline logs go to stderr / run.log as "[DLSS-G]").
 *
 * Wiring (manual hooking, see rfom_dlssg.cpp): the renderer keeps its native device and submits to the
 * native queue; only the swap chain is created through Streamline proxies (factory + queue), so the
 * Present goes through DLSS-G. Reflex is required: rfom_reflex.cpp sends its markers and its sleep through
 * the g_pc_fg_* hooks below while frame generation is usable. Frame generation turns itself off on any
 * frame without DLSS inputs (front-end menus, .bik movies, loading screens, DLSS failure), while the
 * window is in the background or minimised, and around swap-chain resizes; it turns back on after two
 * consecutive frames with inputs. The port's frame limiter (RFOM_FPS) caps the rendered frames. */

void (*volatile g_pc_fg_marker)(int type, unsigned long long id) = 0;
void (*volatile g_pc_fg_sleep)(unsigned long long id) = 0;
int (*volatile g_pc_fg_latency)(double* total_ms, double* render_ms, double* gpu_ms) = 0;
int fsr_mode(void);   /* rsx_fsr.inc */

static int  (*p_fg_init)(ID3D12Device*, const wchar_t*, void (*)(const char*), int);
static int  (*p_fg_upgrade)(void**);
static int  (*p_fg_create_queue)(ID3D12Device*, const D3D12_COMMAND_QUEUE_DESC*, void**, ID3D12CommandQueue**, ID3D12CommandQueue**);
static void (*p_fg_marker)(int, unsigned long long);
static void (*p_fg_sleep)(unsigned long long);
static int  (*p_fg_latency)(double*, double*, double*);
static int  (*p_fg_tag)(ID3D12GraphicsCommandList*, ID3D12Resource*, unsigned, ID3D12Resource*, unsigned,
                        unsigned, unsigned, float, float, int);
static int  (*p_fg_present_tags)(ID3D12GraphicsCommandList*, ID3D12Resource*, ID3D12Resource*, unsigned,
                                 unsigned, unsigned, unsigned, unsigned);
static int  (*p_fg_set_options)(int, int, unsigned, unsigned, unsigned);
static int  (*p_fg_state)(unsigned*, unsigned*, int*, unsigned*, unsigned long long*);
static void (*p_fg_shutdown)(void);

static int s_fg_req;                 /* frames to generate asked for (1 = 2x .. 3 = 4x), 0 = off */
static int s_fg_ready;               /* Streamline + DLSS-G usable on this device */
static int s_fg_on;                  /* DLSS-G enabled at the last present */
static int s_fg_gen;                 /* frames generated while on (after the GPU clamp) */
static int s_fg_streak;              /* consecutive presented frames with DLSS inputs */
static int s_fg_split = 1, s_fg_dbg; /* RFOM_FG_HUDSPLIT, RFOM_FG_HUDDBG */
static int s_fg_vs_ok = 1;
static unsigned s_fg_opt_w, s_fg_opt_h, s_fg_last_status = 0xFFFFFFFFu;
static ID3D12CommandQueue* s_fg_sc_queue;   /* Streamline proxy of the queue: the swap chain is made with it */
static void* s_fg_proxy_dev;
static u32 s_fg_presents, s_fg_hud_ok, s_fg_hud_fb;

/* HUD split resources: hl = the display buffer just before the first HUD draw (guest size), ui = the HUD
 * redrawn alone, premultiplied (guest size); hsc / usc = both scaled into swap-chain-sized textures the way
 * the present pass scales the frame (the generator wants them matching the back buffer). */
static ID3D12Resource *s_fg_hl, *s_fg_ui, *s_fg_hsc, *s_fg_usc;
static D3D12_RESOURCE_STATES s_fg_hl_st, s_fg_ui_st, s_fg_hsc_st, s_fg_usc_st;
static u32 s_fg_sc_w, s_fg_sc_h;
static ID3D12DescriptorHeap *s_fg_rtv_heap, *s_fg_srv_heap;   /* rtv: ui, hsc, usc | srv: hl, ui, render_targets[] */
static UINT s_fg_rtv_inc, s_fg_srv_inc;
static ID3D12PipelineState *s_fg_pass_pso, *s_fg_over_pso;
static int s_fg_res_bad;

/* Per-frame HUD-split state (render thread; a frame can span several command-list batches). */
static struct {
    int gate;      /* the split runs this frame */
    int tagged;    /* DLSS inputs were evaluated (and tagged) this frame */
    int open;      /* a HUD run is open on the display buffer */
    int runs;      /* HUD runs started this frame */
    int unsure;    /* something this frame could not be classified: no UI layer this frame */
    int ndraw;     /* HUD draws copied into the UI layer */
    u32 tag;       /* display buffer (disp_tag) the run started on */
} s_fgh;

static void fg_log(const char* m) { fprintf(stderr, "[DLSS-G] %s%c", m, 10); }

/* AMD FSR 3 frame generation, the fallback (rsx_fsr3fg.inc, included after rsx_fsr.inc) */
static int s_f3_want;    /* FSR 3 frame generation usable: make the frame interpolation swap chain */
static int s_f3_ready;   /* the frame interpolation swap chain is in use */
static int  f3_load(void);
static void f3_present(const D3D12_VIEWPORT* vp, int split, int* vs);
static void f3_before_resize(void);
static void f3_shutdown(void);
static void f3_prepare(ID3D12GraphicsCommandList* cl, ID3D12Resource* z, D3D12_RESOURCE_STATES zst, ID3D12Resource* mv,
                       D3D12_RESOURCE_STATES mst, u32 w, u32 h, u32 rw, u32 rh, float jx, float jy, float dt, int reset);
static int  f3_create_swapchain(IDXGIFactory4* factory, DXGI_SWAP_CHAIN_DESC1* desc, IDXGISwapChain1** out);

/* RFOM_DLSS_FG -> frames to generate per rendered frame */
static int fg_env_gen(void)
{
    const char* e = getenv("RFOM_DLSS_FG");
    if (!e || !*e) return 0;
    if (!_stricmp(e, "on") || !_stricmp(e, "1") || !_stricmp(e, "2") || !_stricmp(e, "2x")) return 1;
    if (!_stricmp(e, "fsr3") || !_stricmp(e, "fsr")) return 1;
    if (!strcmp(e, "3") || !_stricmp(e, "3x")) return 2;
    if (!strcmp(e, "4") || !_stricmp(e, "4x")) return 3;
    return 0;
}

static void fg_clear_hooks(void) { g_pc_fg_marker = 0; g_pc_fg_sleep = 0; g_pc_fg_latency = 0; }

static void fg_fail(const char* why)
{
    fprintf(stderr, "[DLSS-G] frame generation disabled: %s%c", why, 10);
    fg_clear_hooks();
    if (s_fg_ready && p_fg_shutdown) p_fg_shutdown();
    s_fg_ready = 0; s_fg_on = 0;
}

/* Streamline + DLSS-G (game program, frame generation requested). */
static int fg_load_dlss(ID3D12Device* dev)
{
    wchar_t dir[MAX_PATH]; DWORD n = GetModuleFileNameW(NULL, dir, MAX_PATH);
    if (!n || n >= MAX_PATH) { fg_log("frame generation disabled: cannot locate the program folder"); return 0; }
    wchar_t* bs = wcsrchr(dir, L'\\');
    if (bs) *bs = 0;
    if (fsr_mode() != 2) { fg_log("frame generation disabled: it needs NVIDIA DLSS (Anti-aliasing = NVIDIA DLSS, RSX_FSR=2)"); return 0; }
    { const char* h = hdr_env("HDR");
      if (h && (!_stricmp(h, "scrgb") || !strcmp(h, "2"))) { fg_log("frame generation disabled: not available with scRGB HDR output (use HDR10 or SDR)"); return 0; } }
    wchar_t path[MAX_PATH];
    _snwprintf(path, MAX_PATH, L"%ls\\rfom_dlssg.dll", dir); path[MAX_PATH - 1] = 0;
    HMODULE m = LoadLibraryW(path);
    if (!m) { fg_log("frame generation disabled: rfom_dlssg.dll is missing (build with the Streamline SDK in tools\\streamline)"); return 0; }
#define FG_FN(v, name) *(FARPROC*)&v = GetProcAddress(m, name)
    FG_FN(p_fg_init, "rfom_dlssg_init"); FG_FN(p_fg_upgrade, "rfom_dlssg_upgrade");
    FG_FN(p_fg_create_queue, "rfom_dlssg_create_queue"); FG_FN(p_fg_marker, "rfom_dlssg_marker");
    FG_FN(p_fg_sleep, "rfom_dlssg_sleep"); FG_FN(p_fg_latency, "rfom_dlssg_latency"); FG_FN(p_fg_tag, "rfom_dlssg_tag");
    FG_FN(p_fg_present_tags, "rfom_dlssg_present_tags"); FG_FN(p_fg_set_options, "rfom_dlssg_set_options");
    FG_FN(p_fg_state, "rfom_dlssg_state"); FG_FN(p_fg_shutdown, "rfom_dlssg_shutdown");
#undef FG_FN
    if (!p_fg_init || !p_fg_upgrade || !p_fg_create_queue || !p_fg_marker || !p_fg_sleep || !p_fg_latency || !p_fg_tag ||
        !p_fg_present_tags || !p_fg_set_options || !p_fg_state || !p_fg_shutdown) {
        fg_log("frame generation disabled: rfom_dlssg.dll is from another build");
        return 0;
    }
    const char* rx = getenv("RFOM_REFLEX");
    const int boost = rx && atoi(rx) >= 2;
    if (!p_fg_init(dev, dir, fg_log, boost ? 2 : 1)) {
        p_fg_shutdown();
        fg_log("frame generation disabled (see the reason above)");
        return 0;
    }
    s_fg_ready = 1;
    fprintf(stderr, "[DLSS-G] Streamline ready: DLSS Frame Generation %dx requested, HUD split %s%s%c", s_fg_req + 1,
            s_fg_split ? "on" : "off (RFOM_FG_HUDSPLIT=0)", s_fg_dbg ? ", debug view on (RFOM_FG_HUDDBG)" : "", 10);
    return 1;
}

/* init_d3d12, right after the device exists (before the queue): pick the frame generation backend.
 * DLSS-G: returns 1 (then fg_create_queue / fg_upgrade_factory must be used). FSR 3: s_f3_want is set and
 * the swap chain is made by f3_create_swapchain. */
static int fg_load(ID3D12Device* dev)
{
    { const char* e = getenv("RFOM_FG_HUDSPLIT"); s_fg_split = e ? atoi(e) != 0 : 1;
      e = getenv("RFOM_FG_HUDDBG"); s_fg_dbg = e ? atoi(e) : 0; if (s_fg_dbg < 0 || s_fg_dbg > 2) s_fg_dbg = 0; }
    s_fg_req = fg_env_gen();
    const char* e = getenv("RFOM_DLSS_FG");
    if (!s_fg_req) {
        if (e && *e && strcmp(e, "0") && _stricmp(e, "off")) fprintf(stderr, "[DLSS-G] RFOM_DLSS_FG=%s not understood (0, 2, 3, 4 or fsr3) -- off%c", e, 10);
        return 0;
    }
    int be = 0;   /* 0 auto, 1 dlss, 2 fsr3 */
    { const char* b = getenv("RFOM_FG_BACKEND");
      if (b && *b) {
          if (!_stricmp(b, "dlss")) be = 1;
          else if (!_stricmp(b, "fsr3") || !_stricmp(b, "fsr")) be = 2;
          else if (_stricmp(b, "auto")) fprintf(stderr, "[FG] RFOM_FG_BACKEND=%s not understood (auto, dlss or fsr3) -- auto%c", b, 10);
      } }
    if (!_stricmp(e, "fsr3") || !_stricmp(e, "fsr")) {
        if (be == 1) fg_log("RFOM_DLSS_FG=fsr3 overrides RFOM_FG_BACKEND=dlss");
        be = 2;
    }
    { wchar_t exe[MAX_PATH]; DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
      const wchar_t* bs = (n && n < MAX_PATH) ? wcsrchr(exe, L'\\') : NULL;
      if (!bs || _wcsnicmp(bs + 1, L"ResistanceFoMGame", 17) != 0) {
          fprintf(stderr, "[FG] frame generation is off in the front end (it runs in the game program only)%c", 10);
          return 0;
      } }
    if (be != 2) {
        if (fg_load_dlss(dev)) return 1;
        if (be == 1) return 0;
        fprintf(stderr, "[FG] DLSS Frame Generation unavailable -- trying AMD FSR 3 frame generation (2x)%c", 10);
    }
    if (s_fg_req > 1) {
        fprintf(stderr, "[FSR3-FG] %dx requested: FSR 3 generates one frame per rendered frame -- using 2x%c", s_fg_req + 1, 10);
        s_fg_req = 1;
    }
    f3_load();
    return 0;
}

/* The renderer's queue, created through Streamline's device proxy. */
static int fg_create_queue(const D3D12_COMMAND_QUEUE_DESC* qd)
{
    ID3D12CommandQueue *sc = NULL, *nat = NULL;
    if (!p_fg_create_queue(s_d3d.device, qd, &s_fg_proxy_dev, &sc, &nat) || !sc || !nat) {
        fg_fail("the command queue could not be created through Streamline");
        return 0;
    }
    s_fg_sc_queue = sc;
    s_d3d.cmd_queue = nat;
    return 1;
}

/* Reflex through Streamline (DLSS-G requires it): set the hooks rfom_reflex.cpp routes through. */
static int fg_reflex_init(void)
{
    g_pc_fg_marker = p_fg_marker; g_pc_fg_sleep = p_fg_sleep; g_pc_fg_latency = p_fg_latency;
    int (*ri)(void*) = g_pc_reflex_init;
    if (!ri || !ri((void*)s_d3d.device)) { fg_fail("NVIDIA Reflex could not be routed through Streamline"); return 0; }
    g_pc_reflex_active = 2;
    return 1;
}

static void fg_tr(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES* st, D3D12_RESOURCE_STATES to)
{
    if (!r || *st == to) return;
    D3D12_RESOURCE_BARRIER b = {0};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r; b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = *st; b.Transition.StateAfter = to;
    cl->lpVtbl->ResourceBarrier(cl, 1, &b);
    *st = to;
}

static ID3D12Resource* fg_tex(u32 w, u32 h, DXGI_FORMAT f, int rt, D3D12_RESOURCE_STATES st)
{
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d = {0};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; d.Width = w; d.Height = h; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.Format = f; d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags = rt ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE;
    ID3D12Resource* r = NULL;
    if (FAILED(s_d3d.device->lpVtbl->CreateCommittedResource(s_d3d.device, &hp, D3D12_HEAP_FLAG_NONE, &d, st, NULL,
                                                              &IID_ID3D12Resource, (void**)&r))) return NULL;
    return r;
}

static D3D12_CPU_DESCRIPTOR_HANDLE fg_rtv(int i)
{
    D3D12_CPU_DESCRIPTOR_HANDLE h; s_fg_rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(s_fg_rtv_heap, &h);
    h.ptr += (SIZE_T)i * s_fg_rtv_inc; return h;
}
static D3D12_GPU_DESCRIPTOR_HANDLE fg_srv(int i)
{
    D3D12_GPU_DESCRIPTOR_HANDLE h; s_fg_srv_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(s_fg_srv_heap, &h);
    h.ptr += (UINT64)i * s_fg_srv_inc; return h;
}

static const char s_fg_pass_hlsl[] =
    "Texture2D t0 : register(t0); SamplerState s0 : register(s0);\n"
    "struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
    "V vs(uint id : SV_VertexID) { V o; float2 uv = float2((id << 1) & 2, id & 2);\n"
    "  o.uv = uv; o.p = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); return o; }\n"
    "float4 ps(V i) : SV_Target { return t0.Sample(s0, i.uv); }\n";

/* Guest-sized textures, heaps and pipelines (once; needs blit_init's root signature). */
static int fg_res_init(void)
{
    if (s_fg_rtv_heap) return 1;
    if (s_fg_res_bad || !s_blit_rs) return 0;
    s_fg_res_bad = 1;
    ID3D12Device* d = s_d3d.device;
    const u32 gw = SC(s_d3d.width), gh = SC(s_d3d.height);
    s_fg_hl = fg_tex(gw, gh, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    s_fg_ui = fg_tex(gw, gh, DXGI_FORMAT_R8G8B8A8_UNORM, 1, D3D12_RESOURCE_STATE_RENDER_TARGET);
    s_fg_hl_st = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; s_fg_ui_st = D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (!s_fg_hl || !s_fg_ui) { fg_log("HUD split unavailable (out of video memory)"); return 0; }
    D3D12_DESCRIPTOR_HEAP_DESC hd = {0};
    hd.NumDescriptors = 3; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(d->lpVtbl->CreateDescriptorHeap(d, &hd, &IID_ID3D12DescriptorHeap, (void**)&s_fg_rtv_heap))) { s_fg_rtv_heap = NULL; return 0; }
    s_fg_rtv_inc = d->lpVtbl->GetDescriptorHandleIncrementSize(d, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    hd.NumDescriptors = 2 + FRAME_COUNT; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(d->lpVtbl->CreateDescriptorHeap(d, &hd, &IID_ID3D12DescriptorHeap, (void**)&s_fg_srv_heap))) { s_fg_srv_heap = NULL; return 0; }
    s_fg_srv_inc = d->lpVtbl->GetDescriptorHandleIncrementSize(d, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    d->lpVtbl->CreateRenderTargetView(d, s_fg_ui, NULL, fg_rtv(0));
    { D3D12_CPU_DESCRIPTOR_HANDLE h; s_fg_srv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(s_fg_srv_heap, &h);
      ID3D12Resource* src[2 + FRAME_COUNT];
      src[0] = s_fg_hl; src[1] = s_fg_ui;
      for (u32 i = 0; i < FRAME_COUNT; i++) src[2 + i] = s_d3d.render_targets[i];
      for (u32 i = 0; i < 2 + FRAME_COUNT; i++) {
          D3D12_SHADER_RESOURCE_VIEW_DESC sv = {0};
          sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
          sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sv.Texture2D.MipLevels = 1;
          d->lpVtbl->CreateShaderResourceView(d, src[i], &sv, h);
          h.ptr += s_fg_srv_inc;
      } }
    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    if (FAILED(D3DCompile(s_fg_pass_hlsl, sizeof s_fg_pass_hlsl - 1, "fg_pass", NULL, NULL, "vs", "vs_5_0", 0, 0, &vsb, &err)) ||
        FAILED(D3DCompile(s_fg_pass_hlsl, sizeof s_fg_pass_hlsl - 1, "fg_pass", NULL, NULL, "ps", "ps_5_0", 0, 0, &psb, &err))) {
        if (err) err->lpVtbl->Release(err);
        if (vsb) vsb->lpVtbl->Release(vsb);
        fg_log("HUD split unavailable (shader compile failed)");
        return 0;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {0};
    pd.pRootSignature = s_blit_rs;
    pd.VS.pShaderBytecode = vsb->lpVtbl->GetBufferPointer(vsb); pd.VS.BytecodeLength = vsb->lpVtbl->GetBufferSize(vsb);
    pd.PS.pShaderBytecode = psb->lpVtbl->GetBufferPointer(psb); pd.PS.BytecodeLength = psb->lpVtbl->GetBufferSize(psb);
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = 0xFFFFFFFFu;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1; pd.RTVFormats[0] = s_sc_fmt; pd.SampleDesc.Count = 1;
    HRESULT hr = d->lpVtbl->CreateGraphicsPipelineState(d, &pd, &IID_ID3D12PipelineState, (void**)&s_fg_pass_pso);
    D3D12_RENDER_TARGET_BLEND_DESC* b = &pd.BlendState.RenderTarget[0];
    b->BlendEnable = TRUE; b->BlendOp = b->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b->SrcBlend = b->SrcBlendAlpha = D3D12_BLEND_ONE; b->DestBlend = b->DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    if (SUCCEEDED(hr)) hr = d->lpVtbl->CreateGraphicsPipelineState(d, &pd, &IID_ID3D12PipelineState, (void**)&s_fg_over_pso);
    vsb->lpVtbl->Release(vsb); psb->lpVtbl->Release(psb);
    if (FAILED(hr)) { fg_log("HUD split unavailable (pipeline creation failed)"); return 0; }
    s_fg_res_bad = 0;
    return 1;
}

/* Swap-chain-sized HUD-less / UI textures; recreated when the window size changes. */
static int fg_sc_res(void)
{
    if (s_fg_hsc && s_fg_sc_w == s_sc_w && s_fg_sc_h == s_sc_h) return 1;
    if (s_fg_hsc) { s_fg_hsc->lpVtbl->Release(s_fg_hsc); s_fg_hsc = NULL; }
    if (s_fg_usc) { s_fg_usc->lpVtbl->Release(s_fg_usc); s_fg_usc = NULL; }
    s_fg_hsc = fg_tex(s_sc_w, s_sc_h, s_sc_fmt, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    s_fg_usc = fg_tex(s_sc_w, s_sc_h, s_sc_fmt, 1, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    s_fg_hsc_st = s_fg_usc_st = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (!s_fg_hsc || !s_fg_usc) {
        if (s_fg_hsc) { s_fg_hsc->lpVtbl->Release(s_fg_hsc); s_fg_hsc = NULL; }
        if (s_fg_usc) { s_fg_usc->lpVtbl->Release(s_fg_usc); s_fg_usc = NULL; }
        return 0;
    }
    s_d3d.device->lpVtbl->CreateRenderTargetView(s_d3d.device, s_fg_hsc, NULL, fg_rtv(1));
    s_d3d.device->lpVtbl->CreateRenderTargetView(s_d3d.device, s_fg_usc, NULL, fg_rtv(2));
    s_fg_sc_w = s_sc_w; s_fg_sc_h = s_sc_h;
    return 1;
}

/* rsx_fsr.inc, right after the DLSS Super Resolution evaluation on the frame's command list: the same
 * depth and motion vectors (render resolution, UV units, camera + object motion) go to DLSS-G. */
static void fg_tag_scene(ID3D12GraphicsCommandList* cl, ID3D12Resource* z, unsigned zst, ID3D12Resource* mv, unsigned mst,
                         unsigned rw, unsigned rh, float jx, float jy, int reset)
{
    if (!s_fg_ready && !s_fg_dbg) return;
    s_fgh.tagged = 1;
    if (s_fg_ready) p_fg_tag(cl, z, zst, mv, mst, rw, rh, jx, jy, reset || !s_fg_on);
}

/* the UI layer is handed over (and recomposed by DLSS-G) in SDR with the split on */
static int fg_ui_on(void) { return s_fg_split && !g_pc_hdr_active && s_fg_rtv_heap != NULL; }

static void fg_set(int gen)
{
    if (p_fg_set_options(gen, fg_ui_on(), s_sc_w, s_sc_h, (unsigned)s_sc_fmt)) {
        s_fg_on = gen > 0; s_fg_opt_w = s_sc_w; s_fg_opt_h = s_sc_h;
    } else s_fg_on = 0;
}

/* sc_resize, after the GPU is idle and before ResizeBuffers: generation off (it comes back two frames
 * later with the new size), swap-chain-sized textures dropped. */
static void fg_before_resize(void)
{
    if (s_fg_ready && s_fg_on) fg_set(0);
    f3_before_resize();
    s_fg_streak = 0;
    if (s_fg_hsc) { s_fg_hsc->lpVtbl->Release(s_fg_hsc); s_fg_hsc = NULL; }
    if (s_fg_usc) { s_fg_usc->lpVtbl->Release(s_fg_usc); s_fg_usc = NULL; }
}

static void fg_shutdown(void)
{
    f3_shutdown();
    if (!s_fg_ready) return;
    fg_clear_hooks();
    p_fg_shutdown();
    s_fg_ready = 0; s_fg_on = 0;
}

static void fg_hud_reset(void)
{
    const int gate = (s_fg_ready || s_f3_ready || s_fg_dbg) && s_fg_split && !g_pc_hdr_active && fg_res_init();
    memset(&s_fgh, 0, sizeof s_fgh);
    s_fgh.gate = gate;
}

static void fg_late_check(void);   /* rsx_dlssg_hud.inc */

/* present_blit, after the frame and the OSD are in the back buffer (bound as the render target, RT state
 * unless the screenshot copy left it in COPY_SOURCE: shot == 1): build the HUD-less and UI images, tag
 * them, decide whether generation runs for this present, and apply the debug view. */
static void fg_present(ID3D12GraphicsCommandList* cl, UINT bi, u32 fi, D3D12_CPU_DESCRIPTOR_HANDLE sc_rtv,
                       const D3D12_VIEWPORT* vp, const float* k, int shot, int* vs)
{
    if (!s_fg_ready && !s_f3_ready && !s_fg_dbg) return;
    fg_late_check();
    const int split = s_fgh.gate && s_fgh.tagged && fg_sc_res();
    const int valid = split && s_fgh.open && !s_fgh.unsure && s_fgh.ndraw > 0;
    if (split) {
        if (valid) s_fg_hud_ok++; else s_fg_hud_fb++;
        fg_tr(cl, s_fg_ui, &s_fg_ui_st, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        fg_tr(cl, s_fg_hl, &s_fg_hl_st, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        fg_tr(cl, s_fg_hsc, &s_fg_hsc_st, D3D12_RESOURCE_STATE_RENDER_TARGET);
        fg_tr(cl, s_fg_usc, &s_fg_usc_st, D3D12_RESOURCE_STATE_RENDER_TARGET);
        static const float black[4] = {0, 0, 0, 1}, zero[4] = {0, 0, 0, 0};
        const D3D12_RECT sc = {0, 0, (LONG)s_sc_w, (LONG)s_sc_h};
        const D3D12_CPU_DESCRIPTOR_HANDLE hr = fg_rtv(1), ur = fg_rtv(2);
        cl->lpVtbl->ClearRenderTargetView(cl, hr, black, 0, NULL);
        cl->lpVtbl->ClearRenderTargetView(cl, ur, zero, 0, NULL);
        cl->lpVtbl->RSSetViewports(cl, 1, vp);
        cl->lpVtbl->RSSetScissorRects(cl, 1, &sc);
        cl->lpVtbl->SetGraphicsRootSignature(cl, s_blit_rs);
        cl->lpVtbl->SetDescriptorHeaps(cl, 1, &s_fg_srv_heap);
        cl->lpVtbl->IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cl->lpVtbl->SetGraphicsRoot32BitConstants(cl, 0, 8, k, 0);
        /* HUD-less: the same present pass (scale, sharpen, colour-blind filter) as the real frame */
        cl->lpVtbl->OMSetRenderTargets(cl, 1, &hr, FALSE, NULL);
        cl->lpVtbl->SetPipelineState(cl, s_blit_pso);
        cl->lpVtbl->SetGraphicsRootDescriptorTable(cl, 1, fg_srv(valid ? 0 : 2 + (int)fi));
        cl->lpVtbl->DrawInstanced(cl, 3, 1, 0, 0);
        /* UI: the separated HUD (premultiplied), then the port's own OSD on top of it */
        cl->lpVtbl->OMSetRenderTargets(cl, 1, &ur, FALSE, NULL);
        if (valid) {
            cl->lpVtbl->SetPipelineState(cl, s_fg_pass_pso);
            cl->lpVtbl->SetGraphicsRootDescriptorTable(cl, 1, fg_srv(1));
            cl->lpVtbl->DrawInstanced(cl, 3, 1, 0, 0);
        }
        osd_draw(cl, bi);
        fg_tr(cl, s_fg_hsc, &s_fg_hsc_st, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        fg_tr(cl, s_fg_usc, &s_fg_usc_st, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (s_fg_dbg && shot != 1) {   /* debug view replaces what is presented (never the screenshot) */
            static const float grey[4] = {0.5f, 0.5f, 0.5f, 1};
            cl->lpVtbl->OMSetRenderTargets(cl, 1, &sc_rtv, FALSE, NULL);
            cl->lpVtbl->ClearRenderTargetView(cl, sc_rtv, s_fg_dbg == 2 ? grey : black, 0, NULL);
            cl->lpVtbl->RSSetViewports(cl, 1, vp);
            cl->lpVtbl->RSSetScissorRects(cl, 1, &sc);
            cl->lpVtbl->SetGraphicsRootSignature(cl, s_blit_rs);
            cl->lpVtbl->SetDescriptorHeaps(cl, 1, &s_fg_srv_heap);
            cl->lpVtbl->IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cl->lpVtbl->SetGraphicsRoot32BitConstants(cl, 0, 8, k, 0);
            if (s_fg_dbg == 1) {
                cl->lpVtbl->SetPipelineState(cl, s_blit_pso);
                cl->lpVtbl->SetGraphicsRootDescriptorTable(cl, 1, fg_srv(valid ? 0 : 2 + (int)fi));
                cl->lpVtbl->DrawInstanced(cl, 3, 1, 0, 0);
            } else if (valid) {
                cl->lpVtbl->SetPipelineState(cl, s_fg_over_pso);
                cl->lpVtbl->SetGraphicsRootDescriptorTable(cl, 1, fg_srv(1));
                cl->lpVtbl->DrawInstanced(cl, 3, 1, 0, 0);
            }
        }
    }
    if (s_fg_ready) {
        const unsigned l = (unsigned)(vp->TopLeftX + 0.5f), t = (unsigned)(vp->TopLeftY + 0.5f);
        unsigned w = (unsigned)(vp->Width + 0.5f), h = (unsigned)(vp->Height + 0.5f);
        if (l + w > s_sc_w) w = s_sc_w - l;
        if (t + h > s_sc_h) h = s_sc_h - t;
        const int ui_on = fg_ui_on();
        if (s_fgh.tagged)
            p_fg_present_tags(cl, split ? s_fg_hsc : NULL, split ? s_fg_usc : NULL,
                              (unsigned)D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, l, t, w, h);
        /* generation on for this present: DLSS inputs this frame and the one before, window in front */
        const int want = s_fgh.tagged && s_app_active && !IsIconic(s_d3d.hwnd) && (!ui_on || split);
        s_fg_streak = want ? s_fg_streak + 1 : 0;
        if (s_fg_streak >= 2 && (!s_fg_on || s_fg_opt_w != s_sc_w || s_fg_opt_h != s_sc_h)) {
            if (!s_fg_gen) {
                unsigned st = 0, mx = 0, pr = 0; int vok = 1; unsigned long long vram = 0;
                s_fg_gen = s_fg_req;
                if (p_fg_state(&st, &mx, &vok, &pr, &vram)) {
                    if (mx < 1) mx = 1;
                    if ((unsigned)s_fg_gen > mx) {
                        fprintf(stderr, "[DLSS-G] %dx frame generation is not supported on this GPU -- using %ux%c", s_fg_req + 1, mx + 1, 10);
                        s_fg_gen = (int)mx;
                    }
                    s_fg_vs_ok = vok;
                }
            }
            fg_set(s_fg_gen);
            if (s_fg_on) { static int n = 0; if (n < 3) { n++; fprintf(stderr, "[DLSS-G] frame generation on: %dx, %ux%u%s%c",
                                s_fg_gen + 1, s_sc_w, s_sc_h, ui_on ? ", HUD kept out of generated frames" : "", 10); } }
        } else if (!want && s_fg_on) fg_set(0);
        if (s_fg_on && *vs) {
            if (!s_fg_vs_ok) { static int n = 0; if (!n++) fg_log("vsync is not supported with frame generation on this system -- presenting without it");
                               *vs = 0; }
            else if (*vs > 1) { static int n = 0; if (!n++) fg_log("sync interval > 1 is not supported with frame generation -- using 1");
                                *vs = 1; }
        }
        if (s_fg_on && ((++s_fg_presents % 600u) == 0 || s_fg_presents == 120u)) {
            unsigned st = 0, mx = 0, pr = 0; int vok = 1; unsigned long long vram = 0;
            if (p_fg_state(&st, &mx, &vok, &pr, &vram)) {
                if (st != s_fg_last_status || (s_fg_presents % 6000u) == 0) {
                    fprintf(stderr, "[DLSS-G] status 0x%X%s%s%s%s%s, %u frames presented per rendered frame, %llu MB; HUD split %u of %u frames%c",
                            st, st & 1 ? " (resolution too low)" : "", st & 2 ? " (Reflex not detected)" : "",
                            st & 4 ? " (HDR format not supported)" : "", st & 8 ? " (camera constants invalid)" : "",
                            st & 16 ? " (GetCurrentBackBufferIndex not called)" : "", pr, vram >> 20,
                            s_fg_hud_ok, s_fg_hud_ok + s_fg_hud_fb, 10);
                    s_fg_last_status = st;
                }
            }
        }
    } else if (s_f3_ready) f3_present(vp, split, vs);
    else if (s_fg_dbg && (++s_fg_presents % 600u) == 0)
        fprintf(stderr, "[DLSS-G] HUD split (debug view): HUD separated in %u of %u frames%c", s_fg_hud_ok, s_fg_hud_ok + s_fg_hud_fb, 10);
    fg_hud_reset();
}
