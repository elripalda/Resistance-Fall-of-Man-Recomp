/* In-swapchain OSD: small pre-rasterised sprites (toasts, the F3 overlay) drawn on top of the
 * present pass, so they are part of the presented image -- visible in any display mode,
 * including exclusive fullscreen, and they don't break independent flip the way a topmost
 * layered window does. Included by rsx_d3d12_backend.c.
 *
 * The UI side (project/rfom_pcfeat.cpp) paints with GDI into a 32-bit DIB and hands the
 * premultiplied BGRA pixels over with rsx_osd_set(); the render thread uploads them only when
 * they changed (a generation counter) and draws one alpha-blended quad per visible slot.
 * Everything (root signature, PSO, textures, upload buffers) is created in blit_init, so the
 * first toast costs no shader compile or allocation. SDR, HDR10 (PQ) and scRGB outputs are
 * handled in the pixel shader (UI white = the HDR paper white). */

#define OSD_SLOTS 3
#define OSD_MAX_W 768
#define OSD_MAX_H 256
#define OSD_PITCH (OSD_MAX_W * 4)   /* a multiple of D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256) */

typedef struct OsdSlot {
    u8* px;                 /* staged pixels, OSD_PITCH per row */
    int w, h, x, y;         /* sprite size; top-left corner in client (swap chain) pixels */
    float alpha;            /* 0 = hidden */
    u32 gen, up_gen;        /* staged / uploaded pixel generation */
    D3D12_RESOURCE_STATES state;
} OsdSlot;

static SRWLOCK s_osd_lock = SRWLOCK_INIT;
static OsdSlot s_osd[OSD_SLOTS];
static volatile LONG s_osd_ready;
static ID3D12Resource* s_osd_tex[OSD_SLOTS];
static ID3D12Resource* s_osd_up[FRAME_COUNT];
static u8* s_osd_up_map[FRAME_COUNT];
static ID3D12DescriptorHeap* s_osd_heap;
static UINT s_osd_inc;
static ID3D12RootSignature* s_osd_rs;
static ID3D12PipelineState* s_osd_pso;

static const char s_osd_hlsl[] =
    "Texture2D t0 : register(t0); SamplerState s0 : register(s0);\n"
    "cbuffer C : register(b0) { float4 r; float4 k; float4 k2; };\n"   /* r = NDC rect; k = (u1, v1, alpha, mode); k2.x = paper nits */
    "struct V { float4 p : SV_Position; float2 uv : TEXCOORD0; };\n"
    "V vs(uint id : SV_VertexID) { V o; float2 t = float2(id & 1, id >> 1);\n"
    "  o.p = float4(lerp(r.xy, r.zw, t), 0, 1); o.uv = t * k.xy; return o; }\n"
    "float4 ps(V i) : SV_Target {\n"
    "  float4 c = t0.Sample(s0, i.uv) * k.z;\n"
    "  if (k.w < 0.5) return c;\n"
    "  float a = c.a; if (a <= 0) return 0;\n"
    "  float3 l = pow(saturate(c.rgb / a), 2.2) * k2.x;\n"
    "  if (k.w > 1.5) return float4(l / 80.0 * a, a);\n"
    "  float3 n = float3(dot(l, float3(0.627404, 0.329283, 0.043313)), dot(l, float3(0.069097, 0.919540, 0.011362)),\n"
    "                    dot(l, float3(0.016391, 0.088013, 0.895595)));\n"
    "  float3 p = pow(saturate(n / 10000.0), 0.1593017578125);\n"
    "  p = pow((0.8359375 + 18.8515625 * p) / (1.0 + 18.6875 * p), 78.84375);\n"
    "  return float4(p * a, a); }\n";

static int osd_init(void)
{
    ID3D12Device* d = s_d3d.device;
    { const char* e = getenv("RFOM_OSD"); if (e && (!_stricmp(e, "window") || *e == '0')) return 0; }
    for (int i = 0; i < OSD_SLOTS; i++) {
        s_osd[i].px = (u8*)calloc(1, (size_t)OSD_PITCH * OSD_MAX_H);
        if (!s_osd[i].px) return -1;
    }
    D3D12_HEAP_PROPERTIES hp = {0}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC td = {0};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = OSD_MAX_W; td.Height = OSD_MAX_H;
    td.DepthOrArraySize = 1; td.MipLevels = 1; td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    for (int i = 0; i < OSD_SLOTS; i++) {
        if (FAILED(d->lpVtbl->CreateCommittedResource(d, &hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST,
                       NULL, &IID_ID3D12Resource, (void**)&s_osd_tex[i]))) return -1;
        s_osd[i].state = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd = {0};
    bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = (UINT64)OSD_PITCH * OSD_MAX_H * OSD_SLOTS;
    bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    for (u32 i = 0; i < FRAME_COUNT; i++) {
        if (FAILED(d->lpVtbl->CreateCommittedResource(d, &hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ,
                       NULL, &IID_ID3D12Resource, (void**)&s_osd_up[i]))) return -1;
        D3D12_RANGE r0 = {0, 0};
        if (FAILED(s_osd_up[i]->lpVtbl->Map(s_osd_up[i], 0, &r0, (void**)&s_osd_up_map[i]))) return -1;
    }
    D3D12_DESCRIPTOR_HEAP_DESC hd = {0};
    hd.NumDescriptors = OSD_SLOTS; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(d->lpVtbl->CreateDescriptorHeap(d, &hd, &IID_ID3D12DescriptorHeap, (void**)&s_osd_heap))) return -1;
    s_osd_inc = d->lpVtbl->GetDescriptorHandleIncrementSize(d, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    { D3D12_CPU_DESCRIPTOR_HANDLE h; s_osd_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(s_osd_heap, &h);
      for (int i = 0; i < OSD_SLOTS; i++) {
          d->lpVtbl->CreateShaderResourceView(d, s_osd_tex[i], NULL, h);
          h.ptr += s_osd_inc;
      } }

    D3D12_DESCRIPTOR_RANGE rg = {0};
    rg.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; rg.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER rp[2] = {0};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[0].Constants.Num32BitValues = 12; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rp[1].DescriptorTable.NumDescriptorRanges = 1; rp[1].DescriptorTable.pDescriptorRanges = &rg;
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sm = {0};
    sm.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sm.AddressU = sm.AddressV = sm.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sm.MaxLOD = D3D12_FLOAT32_MAX; sm.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rd = {0};
    rd.NumParameters = 2; rd.pParameters = rp; rd.NumStaticSamplers = 1; rd.pStaticSamplers = &sm;
    ID3DBlob *sig = NULL, *err = NULL;
    if (FAILED(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err))) {
        if (err) err->lpVtbl->Release(err); return -1; }
    HRESULT hr = d->lpVtbl->CreateRootSignature(d, 0, sig->lpVtbl->GetBufferPointer(sig), sig->lpVtbl->GetBufferSize(sig),
                                                &IID_ID3D12RootSignature, (void**)&s_osd_rs);
    sig->lpVtbl->Release(sig);
    if (FAILED(hr)) return -1;

    ID3DBlob *vsb = NULL, *psb = NULL;
    if (FAILED(D3DCompile(s_osd_hlsl, sizeof s_osd_hlsl - 1, "osd", NULL, NULL, "vs", "vs_5_0", 0, 0, &vsb, &err))) {
        if (err) { printf("[D3D12] OSD VS: %s\n", (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        return -1; }
    if (FAILED(D3DCompile(s_osd_hlsl, sizeof s_osd_hlsl - 1, "osd", NULL, NULL, "ps", "ps_5_0", 0, 0, &psb, &err))) {
        if (err) { printf("[D3D12] OSD PS: %s\n", (const char*)err->lpVtbl->GetBufferPointer(err)); err->lpVtbl->Release(err); }
        vsb->lpVtbl->Release(vsb); return -1; }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {0};
    pd.pRootSignature = s_osd_rs;
    pd.VS.pShaderBytecode = vsb->lpVtbl->GetBufferPointer(vsb); pd.VS.BytecodeLength = vsb->lpVtbl->GetBufferSize(vsb);
    pd.PS.pShaderBytecode = psb->lpVtbl->GetBufferPointer(psb); pd.PS.BytecodeLength = psb->lpVtbl->GetBufferSize(psb);
    D3D12_RENDER_TARGET_BLEND_DESC* bl = &pd.BlendState.RenderTarget[0];
    bl->BlendEnable = TRUE;
    bl->SrcBlend = D3D12_BLEND_ONE; bl->DestBlend = D3D12_BLEND_INV_SRC_ALPHA; bl->BlendOp = D3D12_BLEND_OP_ADD;
    bl->SrcBlendAlpha = D3D12_BLEND_ONE; bl->DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; bl->BlendOpAlpha = D3D12_BLEND_OP_ADD;
    bl->LogicOp = D3D12_LOGIC_OP_NOOP;
    bl->RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = 0xFFFFFFFFu;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.RasterizerState.DepthClipEnable = TRUE;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1; pd.RTVFormats[0] = s_sc_fmt;
    pd.SampleDesc.Count = 1;
    hr = d->lpVtbl->CreateGraphicsPipelineState(d, &pd, &IID_ID3D12PipelineState, (void**)&s_osd_pso);
    vsb->lpVtbl->Release(vsb); psb->lpVtbl->Release(psb);
    if (FAILED(hr)) { printf("[D3D12] OSD PSO failed (0x%08lX)\n", hr); return -1; }
    InterlockedExchange(&s_osd_ready, 1);
    fprintf(stderr, "[osd] in-swapchain notifications ready%c", 10);
    return 0;
}

/* 1 when sprites given to rsx_osd_set are drawn in the swap chain (else the caller keeps its
 * layered windows). */
int rsx_osd_available(void) { return s_osd_ready ? 1 : 0; }

/* Set a sprite: px = premultiplied BGRA rows of `pitch` bytes (NULL keeps the current pixels and
 * only moves / fades it). x, y = top-left in client pixels; alpha 0 hides it. Larger sprites are
 * clipped to OSD_MAX_W x OSD_MAX_H. Thread-safe; cheap when px is NULL. */
void rsx_osd_set(int slot, const void* px, int w, int h, int pitch, int x, int y, float alpha)
{
    if (slot < 0 || slot >= OSD_SLOTS || !s_osd_ready) return;
    OsdSlot* s = &s_osd[slot];
    AcquireSRWLockExclusive(&s_osd_lock);
    if (px && w > 0 && h > 0) {
        if (w > OSD_MAX_W) w = OSD_MAX_W;
        if (h > OSD_MAX_H) h = OSD_MAX_H;
        for (int r = 0; r < h; r++) memcpy(s->px + (size_t)r * OSD_PITCH, (const u8*)px + (size_t)r * pitch, (size_t)w * 4);
        s->w = w; s->h = h; s->gen++;
    }
    s->x = x; s->y = y; s->alpha = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    ReleaseSRWLockExclusive(&s_osd_lock);
}

/* Render thread, inside present_blit with the back buffer bound as the render target. */
static void osd_draw(ID3D12GraphicsCommandList* cl, UINT bi)
{
    if (!s_osd_ready || !s_sc_w || !s_sc_h) return;
    OsdSlot snap[OSD_SLOTS]; int any = 0;
    AcquireSRWLockExclusive(&s_osd_lock);
    for (int i = 0; i < OSD_SLOTS; i++) {
        OsdSlot* s = &s_osd[i];
        if (s->alpha <= 0.0f || s->w <= 0) { snap[i].alpha = 0; continue; }
        if (s->gen != s->up_gen) {
            u8* dst = s_osd_up_map[bi] + (size_t)i * OSD_PITCH * OSD_MAX_H;
            memcpy(dst, s->px, (size_t)OSD_PITCH * s->h);
            D3D12_RESOURCE_BARRIER b = {0};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = s_osd_tex[i];
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            if (s->state != D3D12_RESOURCE_STATE_COPY_DEST) {
                b.Transition.StateBefore = s->state; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                cl->lpVtbl->ResourceBarrier(cl, 1, &b);
            }
            D3D12_TEXTURE_COPY_LOCATION dl = {0}, sl = {0};
            dl.pResource = s_osd_tex[i]; dl.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            sl.pResource = s_osd_up[bi]; sl.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            sl.PlacedFootprint.Offset = (UINT64)i * OSD_PITCH * OSD_MAX_H;
            sl.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            sl.PlacedFootprint.Footprint.Width = (UINT)s->w; sl.PlacedFootprint.Footprint.Height = (UINT)s->h;
            sl.PlacedFootprint.Footprint.Depth = 1; sl.PlacedFootprint.Footprint.RowPitch = OSD_PITCH;
            cl->lpVtbl->CopyTextureRegion(cl, &dl, 0, 0, 0, &sl, NULL);
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            cl->lpVtbl->ResourceBarrier(cl, 1, &b);
            s->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            s->up_gen = s->gen;
        }
        snap[i] = *s; any = 1;
    }
    ReleaseSRWLockExclusive(&s_osd_lock);
    if (!any) return;

    D3D12_VIEWPORT vp = {0, 0, (float)s_sc_w, (float)s_sc_h, 0, 1};
    D3D12_RECT sc = {0, 0, (LONG)s_sc_w, (LONG)s_sc_h};
    cl->lpVtbl->RSSetViewports(cl, 1, &vp);
    cl->lpVtbl->RSSetScissorRects(cl, 1, &sc);
    cl->lpVtbl->SetPipelineState(cl, s_osd_pso);
    cl->lpVtbl->SetGraphicsRootSignature(cl, s_osd_rs);
    cl->lpVtbl->SetDescriptorHeaps(cl, 1, &s_osd_heap);
    cl->lpVtbl->IASetPrimitiveTopology(cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    D3D12_GPU_DESCRIPTOR_HANDLE g0;
    s_osd_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(s_osd_heap, &g0);
    const float sw = (float)s_sc_w, sh = (float)s_sc_h;
    for (int i = 0; i < OSD_SLOTS; i++) {
        const OsdSlot* s = &snap[i];
        if (s->alpha <= 0.0f) continue;
        const float k[12] = {
            (float)s->x / sw * 2.0f - 1.0f, 1.0f - (float)s->y / sh * 2.0f,
            (float)(s->x + s->w) / sw * 2.0f - 1.0f, 1.0f - (float)(s->y + s->h) / sh * 2.0f,
            (float)s->w / OSD_MAX_W, (float)s->h / OSD_MAX_H, s->alpha, (float)g_pc_hdr_active,
            g_pc_hdr_active ? s_hdr_paper : 0.0f, 0, 0, 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE g = g0; g.ptr += (UINT64)i * s_osd_inc;
        cl->lpVtbl->SetGraphicsRoot32BitConstants(cl, 0, 12, k, 0);
        cl->lpVtbl->SetGraphicsRootDescriptorTable(cl, 1, g);
        cl->lpVtbl->DrawInstanced(cl, 4, 1, 0, 0);
    }
}
