/* DLSS Frame Generation: the HUD split (rsx_dlssg.inc has the rest). Included by rsx_d3d12_backend.c after
 * rsx_fsr.inc; hooked into render_frame_impl's draw loop.
 *
 * The renderer knows every draw, so instead of guessing the HUD from pixels (what injectors have to do) it
 * classifies the draws that reach the display buffer after the frame's DLSS inputs were evaluated:
 *   a HUD draw goes to the display buffer (no offscreen target, no MRT), has depth test (or an ALWAYS test)
 *   and depth write off and no stencil, writes RGB, samples no render target (post passes -- bloom, the
 *   upscaled scene, the screen copy -- do; HUD sprites and fonts sample ordinary textures) and does not
 *   read the display buffer, and blends either as alpha (SRC_ALPHA / ONE, INV_SRC_ALPHA) or additively
 *   (SRC_ALPHA / ONE, ONE) with an ADD equation.
 * A run of such draws is the HUD. Before its first draw the display buffer is copied (the HUD-less frame);
 * every HUD draw is drawn a second time into a cleared UI layer with the alpha channel blended so the layer
 * ends up premultiplied (final = ui.rgb + (1 - ui.a) * hudless), which is what DLSS-G recomposes over the
 * generated frames. The real frame on screen is never changed.
 * Any other draw to the display buffer closes the run; a later HUD draw starts a new one (the frame so far,
 * earlier HUD included, becomes the HUD-less image). The frame gets no UI layer at all (the whole frame is
 * generated, HUD included, as without the split) when it is unsure: more than four runs, the run changes
 * display buffer, a HUD pipeline variant is not available, the display buffer is replaced at frame end
 * (composite / guest framebuffer paths), or no run was open at the end of the frame. */

/* render_frame_impl, start of a guest frame: forget the previous frame's runs (the gate stays). */
static void fg_hud_frame_start(void)
{
    const int g = s_fgh.gate;
    memset(&s_fgh, 0, sizeof s_fgh);
    s_fgh.gate = g;
}

/* display-buffer colour clear in the draw loop, and the guest frame's first clear */
static void fg_hud_clear(void) { s_fgh.open = 0; }

static int fg_hud_blend(u32 k, u32* alpha_bits)
{
    if (!(k & 1u)) return 0;
    const u32 sc = (k >> 1) & 31u, dc = (k >> 6) & 31u, oc = (k >> 21) & 7u;
    if (oc != (u32)D3D12_BLEND_OP_ADD) return 0;
    if ((sc == (u32)D3D12_BLEND_SRC_ALPHA || sc == (u32)D3D12_BLEND_ONE) && dc == (u32)D3D12_BLEND_INV_SRC_ALPHA) {
        *alpha_bits = ((u32)D3D12_BLEND_ONE << 11) | ((u32)D3D12_BLEND_INV_SRC_ALPHA << 16) | ((u32)D3D12_BLEND_OP_ADD << 24);
        return 1;
    }
    if ((sc == (u32)D3D12_BLEND_SRC_ALPHA || sc == (u32)D3D12_BLEND_ONE) && dc == (u32)D3D12_BLEND_ONE) {
        *alpha_bits = ((u32)D3D12_BLEND_ZERO << 11) | ((u32)D3D12_BLEND_ONE << 16) | ((u32)D3D12_BLEND_OP_ADD << 24);
        return 1;
    }
    return 0;
}

static int fg_hud_is(const D3D12DrawRecord* dr, const int* cur_m, u32* key)
{
    if (!dr->fp_addr || dr->rt_mrt[0] || cur_m[0] >= 0 || dr_num_rts(dr) != 1) return 0;
    const u32 c = dr->cull;
    if (!(c & 0x100u) || (c & 0x400u)) return 0;                         /* depth state unknown, or written */
    if ((c & 0x200u) && ((c >> 12) & 7u) != 7u) return 0;                /* a real depth test */
    if (c >> 24) return 0;                                               /* stencil */
    if ((dr->cmask & 7u) != 7u) return 0;
    if (dr->samp_disp) return 0;
    for (int u = 0; u < RSX_UNITS; u++) if (dr->tex_rt[u] >= 0) return 0;
    u32 ab = 0;
    if (!fg_hud_blend(dr->blend_key, &ab)) return 0;
    *key = (dr->blend_key & ~((31u << 11) | (31u << 16) | (7u << 24))) | ab;
    return 1;
}

/* Every draw the loop issues, just before it is issued, with its state bound. */
static void fg_hud_draw(const D3D12DrawRecord* dr, int cur_rt, const int* cur_m, ID3D12PipelineState* bound_pso,
                        D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, u32 fi)
{
    if (!s_fgh.gate || !s_fgh.tagged || s_fgh.unsure || cur_rt >= 0) return;
    u32 key = 0;
    if (!fg_hud_is(dr, cur_m, &key)) { s_fgh.open = 0; return; }
    if (s_fgh.open && dr->disp_tag && dr->disp_tag != s_fgh.tag) { s_fgh.unsure = 1; return; }
    fp_set_unnorm(dr);
    ID3D12PipelineState* up = vp_get_fp_pso(dr_vs_key(dr), dr->fp_addr, key, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                                            dr->fp_exp32, dr->cmask | 8u, dr->cull, dr_cube_mask(dr));
    if (!up) { s_fgh.unsure = 1; return; }
    ID3D12GraphicsCommandList* cl = s_d3d.cmd_list;
    if (!s_fgh.open) {
        if (++s_fgh.runs > 4) { s_fgh.unsure = 1; return; }
        s_fgh.open = 1; s_fgh.tag = dr->disp_tag;
        D3D12_RESOURCE_STATES ds = D3D12_RESOURCE_STATE_RENDER_TARGET;
        fg_tr(cl, s_d3d.render_targets[fi], &ds, D3D12_RESOURCE_STATE_COPY_SOURCE);
        fg_tr(cl, s_fg_hl, &s_fg_hl_st, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->lpVtbl->CopyResource(cl, s_fg_hl, s_d3d.render_targets[fi]);
        fg_tr(cl, s_d3d.render_targets[fi], &ds, D3D12_RESOURCE_STATE_RENDER_TARGET);
        fg_tr(cl, s_fg_ui, &s_fg_ui_st, D3D12_RESOURCE_STATE_RENDER_TARGET);
        static const float zero[4] = {0, 0, 0, 0};
        cl->lpVtbl->ClearRenderTargetView(cl, fg_rtv(0), zero, 0, NULL);
    }
    const D3D12_CPU_DESCRIPTOR_HANDLE ur = fg_rtv(0);
    cl->lpVtbl->SetPipelineState(cl, up);
    cl->lpVtbl->OMSetRenderTargets(cl, 1, &ur, FALSE, dsv.ptr ? &dsv : NULL);
    if (dr->ib_count && s_vp_ib)
        cl->lpVtbl->DrawIndexedInstanced(cl, dr->ib_count, 1, dr->ib_first, dr->gf ? 0 : (INT)(dr->vb_byte_offset / 256), 0);
    else
        cl->lpVtbl->DrawInstanced(cl, dr->vertex_count, 1, dr->vb_byte_offset / 256, 0);
    cl->lpVtbl->SetPipelineState(cl, bound_pso);
    cl->lpVtbl->OMSetRenderTargets(cl, 1, &rtv, FALSE, dsv.ptr ? &dsv : NULL);
    s_fgh.ndraw++;
}

/* Frame end, before the guest-framebuffer / composite paths may replace the display buffer. */
static void fg_hud_frame_end(void)
{
    static int gfb = -1;
    if (gfb < 0) gfb = getenv("GCM_GUEST_FB") != NULL;
    if (s_fgh.open && (s_composite_src || gfb)) s_fgh.unsure = 1;
}

/* fg_present: DLSS gave up for FSR (no inputs for DLSS-G any more) -- say so once. */
static void fg_late_check(void)
{
    static int said = 0;
    if (!said && s_fg_ready && s_dlss_bad) {
        said = 1;
        fg_log("DLSS Super Resolution failed and the port switched to FSR -- frame generation stays off");
    }
}
