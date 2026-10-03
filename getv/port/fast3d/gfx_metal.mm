// Native Metal backend for the Fast3D GfxRenderingAPI -- the tvOS/iOS unlock. GL ES is
// deprecated on Apple platforms and our fast3d wants desktop GL 2.1 (#version 120), which
// does not exist on tvOS at all; Metal is the only forward path there. See
// docs/ROADMAP.md "Phase 3: presentation and platforms" and docs/REUSE_AUDIT.md.
//
// PORTED FROM, NOT COPIED: kenix3/libultraship, branch port-maintenance,
// src/fast/backends/gfx_metal.{h,cpp} + gfx_metal_shader.cpp + shaders/metal/default.shader.metal
// (vendored locally at vendor/soh/libultraship -- MIT, Copyright (c) 2022 kenix3). That
// backend targets a C++ virtual-class GfxRenderingAPI with full multi-framebuffer,
// MSAA-variant and CPU-readback support; this one targets our much smaller C
// struct-of-function-pointers (gfx_rendering_api.h, 22 entries, no framebuffer concept at
// all) with a single render target -- the screen -- so the framebuffer/MSAA/readback
// machinery is deliberately not carried over. What IS taken, independently reimplemented
// against plain Objective-C Metal (not metal-cpp, so nothing extra needs vendoring): the
// device/layer/queue setup, per-shader-variant MTLRenderPipelineState caching, and the
// technique of building one MSL source string per N64 colour-combiner and compiling it at
// runtime. The combiner formula itself is transliterated from THIS project's own
// gfx_opengl.c (gfx_opengl_create_and_load_new_shader), not from LUS's shader.metal
// template, because it must match our exact SHADER_*/CC_C2_* bit layout (gfx_cc.h) --
// see the note below about why gfx_cc_get_features() is NOT used here.
//
// KNOWN v1 GAPS, deliberate, staged: no CRT post-processing yet. ImGui rendering and the
// supersample/FXAA path now share the same drawable lifecycle below. GETV_SHOTFRAME capture
// (ge_shot_maybe_metal(), below) is done
// -- same env vars and BMP layout as gfx_opengl.c's ge_shot_maybe(), for headless
// verification without a screenshot-capable window (this port's whole reason to exist on
// tvOS/iOS, where nothing else can photograph the screen).
#ifdef RAPI_METAL

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <SDL2/SDL.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef _LANGUAGE_C
# define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#include "../platform.h"
#include "../configfile.h"
#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "gfx_pc.h"
#include "gfx_metal.h"
#include "../src/ge_diagnostic_capture.h"

#ifdef GE_WITH_IMGUI
#include "imgui.h"
#include "imgui_impl_metal.h"
#include "../src/ge_imgui.h"
#endif

void *gePortMetalLayer = NULL;
void *gePortMetalWindow = NULL;

#define SHADER_PROGRAM_POOL_SIZE 128
#define TEX_CACHE_STEP 512
#define VBO_POOL_COUNT 3
#define VBO_POOL_BYTES (4 * 1024 * 1024)

struct ShaderProgram {
    uint64_t shader_id;
    id<MTLRenderPipelineState> pipeline;
    uint8_t num_inputs;
    bool used_textures[2];
    uint8_t num_floats;
    bool opt_alpha;
    bool used_noise;
};

struct MetalTexture {
    id<MTLTexture> tex;
    id<MTLSamplerState> sampler;
    float size[2];
    bool linear_filter;
    /* Parallax height companion, one per diffuse texture slot -- same reasoning as
     * gfx_opengl.c's identical has_height/height_gltex fields on struct GLTexture: this is
     * per-slot rather than a single shared "current" texture because the uniform refresh
     * (gfx_metal_draw_triangles, every draw) and the override upload
     * (ge_texpack_try_override, gfx_pc.c, only on a texture-cache miss) run at different
     * rates, so a shared flag would go stale the moment a different, already-cached texture
     * got rebound without re-importing. */
    bool has_height;
    id<MTLTexture> height_tex;
};

struct FrameUniforms {
    int32_t frame_count;
};

static struct ShaderProgram shader_program_pool[SHADER_PROGRAM_POOL_SIZE];
static uint16_t shader_program_pool_size;
static struct ShaderProgram *cur_prg = NULL;

static int tex_cache_size = 0;
static int num_textures = 0;
static struct MetalTexture *tex_cache = NULL;
static struct MetalTexture *metal_tex[2];
static int metal_curtex = 0;

static uint32_t frame_count;

/* Parallax height channel's neutral placeholder -- see gfx_opengl.c's identical
 * ge_height_tex for the full reasoning. Bound whenever the currently selected tile-0
 * texture has no real height data of its own (MetalTexture::has_height false), so
 * uTexHeight is always legal to sample even on the frame before any override could
 * possibly have loaded one. 1x1, mid-grey (128,128,128,255): the value the fragment
 * shader's `- 0.5` reads as "no displacement". */
static id<MTLTexture> mtl_height_placeholder;
static id<MTLSamplerState> mtl_height_sampler;

static id<MTLDevice> mtl_device;
static CAMetalLayer *mtl_layer;
static id<MTLCommandQueue> mtl_queue;
static id<MTLCommandBuffer> mtl_cmdbuf;
static id<MTLRenderCommandEncoder> mtl_encoder;
#ifdef GE_WITH_IMGUI
/* The overlay's OWN encoder, separate from the game's mtl_encoder above -- a second render
 * pass on the same drawable texture, loadAction=Load so the game's pixels survive. See the
 * frame-lifecycle note on gfx_metal_end_frame()/gePortMetalFinishFrame(). */
static id<MTLRenderCommandEncoder> mtl_overlay_encoder;
#endif
static id<CAMetalDrawable> mtl_drawable;
static id<MTLTexture> mtl_diag_capture_tex;
static int mtl_diag_readback_armed;
static id<MTLTexture> mtl_depth_tex;
static uint32_t mtl_depth_w, mtl_depth_h;

/* What gfx_metal_set_viewport/set_scissor should clamp against for THIS frame's game
 * encoder -- native drawable size on the fast path, the inflated supersampled size on the
 * postfx path (gfx_metal_start_frame sets this each frame; see GETV-SUPERSAMPLE below for
 * why the game's own draws can land in a target larger than the drawable). Reading
 * mtl_drawable.texture directly here (the previous behaviour) is only correct on the fast
 * path -- on the postfx path mtl_drawable is not even acquired yet at this point (see
 * gfx_metal_end_frame's deferred nextDrawable), and even once it is, its size is the wrong
 * (native, not inflated) reference for scissoring the game's own draws. */
static uint32_t mtl_render_target_w, mtl_render_target_h;

/* GETV-SUPERSAMPLE / GETV_MSAA / GETV_FXAA offscreen path. One shared intermediate colour
 * target serves MSAA-resolve, supersample-downsample and FXAA input alike --
 * mirroring how gfx_opengl.c's GE_POSTFX path unifies the same three into one pass, not
 * three separate mechanisms. mtl_pp_depth backs the game's own depth test/write during that
 * pass; nothing ever reads it back afterward, so MTLStorageModeMemoryless is valid for the
 * whole thing on this TBDR GPU, not just for MSAA specifically.
 *
 * mtl_pp_color/mtl_pp_depth are always single-sample: the composite pass (gfx_metal_end_frame)
 * only ever samples mtl_pp_color, and CAMetalLayer's drawable can never itself be multisampled,
 * so something single-sample has to exist as the hand-off point regardless of whether MSAA is
 * active. mtl_pp_color_ms/mtl_pp_depth_ms are the actual multisample render targets the game
 * draws into when GETV_MSAA is active -- both MTLStorageModeMemoryless, since Metal resolves
 * mtl_pp_color_ms into mtl_pp_color automatically via MTLStoreActionMultisampleResolve at
 * endEncoding, and depth is never resolved (nothing reads it back either way). nil when MSAA
 * is off, in which case the game draws straight into mtl_pp_color/mtl_pp_depth exactly as
 * increment 3 already did. */
static id<MTLTexture> mtl_pp_color;
static id<MTLTexture> mtl_pp_depth;
static id<MTLTexture> mtl_pp_color_ms;
static id<MTLTexture> mtl_pp_depth_ms;
static uint32_t mtl_pp_w, mtl_pp_h;
static uint32_t mtl_pp_built_samples;
static id<MTLRenderPipelineState> mtl_pp_pipeline;
static id<MTLSamplerState> mtl_pp_sampler;

static id<MTLBuffer> mtl_vbo_pool[VBO_POOL_COUNT];
static int mtl_vbo_index;
static size_t mtl_vbo_offset;

/* [depth_test][depth_mask][zmode_decal]. Precomputed once: MTLDepthStencilState is
 * immutable after creation, and there are only 8 combinations. */
static id<MTLDepthStencilState> mtl_depth_states[2][2][2];

static bool cur_depth_test = false, cur_depth_mask = true, cur_zmode_decal = false;
static bool cur_zmode_cloud = false;

static bool gfx_metal_z_is_from_0_to_1(void) {
    /* Metal's NDC z range is [0,1], unlike GL's [-1,1] -- this is the one place that
     * difference surfaces in the abstract API; gfx_pc.c builds the projection matrix
     * accordingly. Getting this wrong clips/z-fights everything, silently. */
    return true;
}

// ------------------------------------------------------------------ shader source build
//
// Transliterated from gfx_opengl_create_and_load_new_shader() in gfx_opengl.c, MSL instead
// of GLSL. Deliberately NOT using gfx_cc_get_features() (gfx_cc.c): that helper's
// do_single/do_multiply/do_mix/color_alpha_same are computed only from cycle 1's c[0]/c[1]
// arrays and never from cycle 2's c2[0]/c2[1] -- silently wrong for every two-cycle
// combiner, and two-cycle combiners are nearly everywhere in this game. Decoding inline
// here, matching gfx_opengl.c's own (correct, two-cycle-aware) logic byte for byte, avoids
// depending on that latent bug. Flagged separately; not this file's job to fix gfx_cc.c.

static void m_append_str(char *buf, size_t *len, const char *str) {
    while (*str != '\0') buf[(*len)++] = *str++;
}
static void m_append_line(char *buf, size_t *len, const char *str) {
    while (*str != '\0') buf[(*len)++] = *str++;
    buf[(*len)++] = '\n';
}

static const char *m_shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "float4(0.0, 0.0, 0.0, 0.0)" : "float3(0.0, 0.0, 0.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "in.input1" : "in.input1.xyz";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "in.input2" : "in.input2.xyz";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "in.input3" : "in.input3.xyz";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "in.input4" : "in.input4.xyz";
            case SHADER_TEXEL0:
                return with_alpha ? "texVal0" : "texVal0.xyz";
            case SHADER_TEXEL0A:
                return hint_single_element ? "texVal0.w" :
                    (with_alpha ? "float4(texVal0.w, texVal0.w, texVal0.w, texVal0.w)" : "float3(texVal0.w, texVal0.w, texVal0.w)");
            case SHADER_TEXEL1:
                return with_alpha ? "texVal1" : "texVal1.xyz";
            case SHADER_COMBINED:
                return with_alpha ? "texel" : (inputs_have_alpha ? "texel.xyz" : "texel");
            case SHADER_COMBINEDA:
                if (!inputs_have_alpha)
                    return with_alpha ? "float4(1.0, 1.0, 1.0, 1.0)" : "float3(1.0, 1.0, 1.0)";
                return hint_single_element ? "texel.w" :
                    (with_alpha ? "float4(texel.w, texel.w, texel.w, texel.w)" : "float3(texel.w, texel.w, texel.w)");
        }
    } else {
        switch (item) {
            case SHADER_0: return "0.0";
            case SHADER_INPUT_1: return "in.input1.w";
            case SHADER_INPUT_2: return "in.input2.w";
            case SHADER_INPUT_3: return "in.input3.w";
            case SHADER_INPUT_4: return "in.input4.w";
            case SHADER_TEXEL0: return "texVal0.w";
            case SHADER_TEXEL0A: return "texVal0.w";
            case SHADER_TEXEL1: return "texVal1.w";
            case SHADER_COMBINED:
            case SHADER_COMBINEDA:
                return "texel.w";
        }
    }
    return only_alpha ? "0.0" : (with_alpha ? "float4(0.0, 0.0, 0.0, 0.0)" : "float3(0.0, 0.0, 0.0)");
}

static void m_append_formula_row(char *buf, size_t *len, const uint8_t row[4], bool with_alpha, bool only_alpha, bool opt_alpha) {
    const bool do_single   = row[2] == 0;
    const bool do_multiply = row[1] == 0 && row[3] == 0;
    const bool do_mix      = row[1] == row[3];
    if (do_single) {
        m_append_str(buf, len, m_shader_item_to_str(row[3], with_alpha, only_alpha, opt_alpha, false));
    } else if (do_multiply) {
        m_append_str(buf, len, m_shader_item_to_str(row[0], with_alpha, only_alpha, opt_alpha, false));
        m_append_str(buf, len, " * ");
        m_append_str(buf, len, m_shader_item_to_str(row[2], with_alpha, only_alpha, opt_alpha, true));
    } else if (do_mix) {
        m_append_str(buf, len, "mix(");
        m_append_str(buf, len, m_shader_item_to_str(row[1], with_alpha, only_alpha, opt_alpha, false));
        m_append_str(buf, len, ", ");
        m_append_str(buf, len, m_shader_item_to_str(row[0], with_alpha, only_alpha, opt_alpha, false));
        m_append_str(buf, len, ", ");
        m_append_str(buf, len, m_shader_item_to_str(row[2], with_alpha, only_alpha, opt_alpha, true));
        m_append_str(buf, len, ")");
    } else {
        m_append_str(buf, len, "(");
        m_append_str(buf, len, m_shader_item_to_str(row[0], with_alpha, only_alpha, opt_alpha, false));
        m_append_str(buf, len, " - ");
        m_append_str(buf, len, m_shader_item_to_str(row[1], with_alpha, only_alpha, opt_alpha, false));
        m_append_str(buf, len, ") * ");
        m_append_str(buf, len, m_shader_item_to_str(row[2], with_alpha, only_alpha, opt_alpha, true));
        m_append_str(buf, len, " + ");
        m_append_str(buf, len, m_shader_item_to_str(row[3], with_alpha, only_alpha, opt_alpha, false));
    }
}

static void m_append_cycle(char *buf, size_t *len, uint8_t c[2][4], bool opt_alpha, bool halves_same) {
    if (!halves_same && opt_alpha) {
        m_append_str(buf, len, "float4(");
        m_append_formula_row(buf, len, c[0], false, false, true);
        m_append_str(buf, len, ", ");
        m_append_formula_row(buf, len, c[1], true, true, true);
        m_append_str(buf, len, ")");
    } else {
        m_append_formula_row(buf, len, c[0], opt_alpha, false, opt_alpha);
    }
}

/* Defined below (needs mtl_device, only valid after gfx_metal_init()); forward-declared here
 * because gfx_metal_build_pipeline needs it for rasterSampleCount and shader/pipeline building
 * lives ahead of the mipmap/aniso/MSAA sampler-state helpers in this file's layout. */
static uint32_t ge_metal_msaa_samples(void);

static id<MTLRenderPipelineState> gfx_metal_build_pipeline(uint64_t shader_id, struct ShaderProgram *prg) {
    uint8_t c[2][4], c2[2][4];
    for (int i = 0; i < 4; i++) {
        c[0][i] = (shader_id >> (i * 3)) & 7;
        c[1][i] = (shader_id >> (12 + i * 3)) & 7;
        c2[0][i] = (shader_id >> (CC_C2_RGB_SHIFT + i * CC_C2_SLOT_BITS)) & CC_C2_SLOT_MASK;
        c2‹­¦ëm®éÜj×¢¸ Šv¥jšk£¦j×­¢G§r‹§·oxÓˆÜš]J‹KMŠNÂˆ›Üˆ
HHHNÈHHÈKKJHÂˆÛÛœİ[œÚYÛ™YÚ\ˆ
œ›İÈH
È
Ú^™Wİ
^H
ˆÈ
ˆÂˆ[Âˆ›Üˆ
HÈÎÈ
ÊÊHÂˆÜš]J›İÈ
È
ˆKËŠNÂˆBˆYˆ
Y
HÜš]J™\›ËK
Ú^™Wİ
\YŠNÂˆBˆYˆ
˜ÛÜÙJŠHOH
HÂˆœš[Šİ\œ‹–ÙÙ]—VÜÚİH˜ÛÜÙH˜Z[Y›Üˆ	É\ÉÎˆ	\×ˆ‹]İ™\œ›ÜŠ\œ››ÊJNÂˆ™›\Ú
İ\œŠNÂˆ™]\›ˆÂˆBˆ™]\›ˆNÂŸB‚œİ]XÈ[ÙWÜÚİØØ\\™Wİ^\™WÛY][
ÛÛœİÚ\ˆ
œ]YU^\™Oˆ^ˆ[
›İ]İË[
›İ]Ú
BÂˆ[ÎÂˆ[Âˆ[œÚYÛ™YÚ\ˆ
œÂˆ[ÚÎÂ‚ˆYˆ
]^]OH•S
HÈ™]\›ˆÈBˆÈH
[
]^ÚYÂˆH
[
]^šZYÚÂˆYˆ
ÈHH
HÈ™]\›ˆÈB‚ˆH
[œÚYÛ™YÚ\ˆ
Š[X[ØÊ
Ú^™Wİ
]È
ˆ
ˆ
NÂˆYˆ
\
HÈ™]\›ˆÈBˆİ^Ù]]\Îœˆ]\Ô\”›İÎŠ”ÕR[YÙ\ŠJÈ
ˆ
Bˆœ›ÛT™YÚ[Û“U™YÚ[Û“XZÙL‘

”ÕR[YÙ\Š]Ë
”ÕR[YÙ\ŠZ
BˆZ\X\]™[ŒNÂˆÚÈHÙWÜÚİİÜš]WØ›\ÛY][
]Ë
NÂˆœ™YJ
NÂˆYˆ
ÚÊHÂˆYˆ
İ]İÊH
›İ]İÈHÎÂˆYˆ
İ]Ú
H
›İ]ÚHÂˆBˆ™]\›ˆÚÎÂŸB‚œİ]XÈ›ÚYÙWÜÚİÛX^X™WÛY][
YUÛÛ[X[™Y™™\ˆÛYY‹YU^\™Oˆ^
HÂˆİ]XÈ[ÚİÙœ˜[YHHLÂˆİ]XÈ[œÚYÛ™YÛ™È››ÎÂˆİ]XÈÛÛœİÚ\ˆ
œÚİÜ]Âˆİ]XÈÚ\ˆÚİÜ]ØY–ÌLNÂˆÛÛœİÚ\ˆ
™XY×Ü]Âˆ[ØÚY[YÂˆYˆ
ÚİÙœ˜[YHOHLŠHÂˆÛÛœİÚ\ˆ
™HHÙ][Š‘ÑU—ÔÒÕ”SQHŠNÂˆÚİÙœ˜[YHH
H	‰ˆ
™JHÈ]ÚJJHˆLNÂˆÚİÜ]HÙ][Š‘ÑU—ÔÒÕUŠNÂˆYˆ
\ÚİÜ]JœÚİÜ]
HÂˆÊˆH˜\™H™[]]™H˜[YHÛ›HÛÜšÜÈÚ\™HH›ØÙ\ÜÉÜÈÕÑ\ÈÜš]X›HKHYBˆ
ˆÛˆ\ÚİÜ˜[ÙHÛˆ“ÔËÚSÔËÚ\™HH\[™H]Ù[ˆ\È™XY[Û›H[™ˆ
ˆ›Ü[Š
H˜Z[ÈÚ]TT“H
›İ[™H\™Ø^Nˆ\È˜Z[YÛÛ\][Bˆ
ˆÚ[[H™Y›Ü™HHXYÛ›ÜİXÈH™]È[™\È™[İÈ^\İY
KˆTTˆ\ÈBˆ
ˆİ[™\™ÔÒV[ˆ˜\ˆ]™\H›ØÙ\ÜÈÛˆH]›Ü›H[™XYH\ÈÙ]Âˆ
ˆ]\	ÜÈİÛˆØ[™›ŞYÛÛZ[™\ˆKH›È\‹Z[œİ[URQÈ\ØÛİ™\ˆÜ‚ˆ
ˆİY\ÜÈ]œ›ÛHİ]ÚYHH›ØÙ\ÜËˆ
‹ÂˆÛÛœİÚ\ˆ
\HÙ][Š•TTˆŠNÂˆYˆ
\	‰ˆ
\
HÂˆÛœš[ŠÚİÜ]ØY‹Ú^™[ÙˆÚİÜ]ØY‹‰\ËÙÙ]—ÜÚİ˜›\‹\
NÂˆÚİÜ]HÚİÜ]ØYÂˆH[ÙHÂˆÚİÜ]H™Ù]—ÜÚİ˜›\ÂˆBˆBˆBˆ››ÊÊÎÂˆÊˆÚX\Ûˆ]™\Hİ\ˆœ˜[YNˆHÛİ[\ˆX›İ™H\ÈÈ[ˆ[˜ÛÛ™][Û˜[HÈÛ›İÂˆ
ˆÚXÚœ˜[YH\È\Ë]HÔHİ[™[İÈ\ÈHÛ™H[™ÈÑU—ÔÒÕ”SQH\Âˆ
ˆİ\ÜÙYÈÛÜİÛ›HÛˆHÚ[™ÛHœ˜[YHXİX[H™Z[™ÈØ\\™Yˆ
‹ÂˆØÚY[YHÚİÙœ˜[YHˆ	‰ˆ
Û™ÊY››ÈOH
Û™Ê\ÚİÙœ˜[YNÂˆXY×Ü]HÙTÜXYÛ›ÜİXÔØÜ™Y[œÚİ]

NÂˆYˆ
\ØÚY[Y	‰ˆXY×Ü]OH•S
H™]\›ÂˆYˆ
]^	‰ˆØÚY[Y
H™]\›Â‚ˆÊˆ]ØÛYYˆØ\È[™XYHÛÛ[Z]YHHØ[\ˆ
™\Ù[˜]ØX›NˆØÚY[\Âˆ
ˆ™\Ù[][Ûˆ›ÜˆÚ[ˆHÔHš[š\Ú\Ë]Ù\È›İ]Ù[ˆ›ØÚÊHKHØZ]›Üˆ]ˆ
ˆÔHÛÜšÈÈ[™™Y›Ü™HÙ]]\ËÜˆ\È™XYÈÚ]]™\ˆØ\È[ˆH^\™H™Y›Ü™Bˆ
ˆ\Èœ˜[YIÜÈ˜]ÜË›İ\Èœ˜[YKˆ
‹ÂˆØÛYYˆØZ][[ÛÛ\]YNÂ‚ˆYˆ
ØÚY[Y
HÂˆ[ÈHHÂˆYˆ
ÙWÜÚİØØ\\™Wİ^\™WÛY][
ÚİÜ]^	Ë	š
JHÂˆœš[Šİ\œ‹–ÙÙ]—VÜÚİHœ˜[YH	[HOˆ	\È
	Y	Y
Wˆ‹››ËÚİÜ]Ë
NÂˆ™›\Ú
İ\œŠNÂˆBˆBˆYˆ
XY×Ü]OH•S
HÂˆ[ÈHHÂˆ[ÚÈHÙWÜÚİØØ\\™Wİ^\™WÛY][
XY×Ü]]ÙXY×ØØ\\™Wİ^	Ë	š
NÂˆÙTÜXYÛ›ÜİXÔØÜ™Y[œÚİÛÛ\]JÚËË
NÂˆYˆ
ÚÊHÂˆœš[Šİ\œ‹–ÙÙ]—VÙXY×HØÜ™Y[œÚİœ˜[YH	[HOˆ	\È
	Y	Y
Wˆ‹ˆ››ËXY×Ü]Ë
NÂˆ™›\Ú
İ\œŠNÂˆBˆBŸB‚‹Êˆ[™ÈHĞSQIÜÈ™[™\ˆ[˜ÛÙ\ˆÛ›HKHÙ\È“Õ™\Ù[ÜˆÛÛ[Z]ˆ]Ü]
[™\Âˆ
ˆÚÛHš[H™Z[™È™XXÚX›Hœ›ÛHİ]ÚYHÙÜË˜ÉÜÈÙ™[™\š[™ĞTHX›H][
H^\İÂˆ
ˆ›ÜˆÛ™H™X\ÛÛˆ[QİZKˆÙÜË˜ÈØ[ÈÙÜ˜\KO™[™Ùœ˜[YJ
H[™[‚ˆ
ˆÙİØ\KOœİØ\ØY™™\œ×Ø™YÚ[Š
H
ÙÜÙ‹˜ÊKÚXÚ\ÈÚ\™HH][˜Ú\‹Ù]‹[İ™\›^Bˆ
ˆ˜]ÜÈ[™Ú\™HÓ	ÜÈÑÑÓÔİØ\Ú[™İÊ
H]™\ÈKHK™KˆHİ™\›^H˜]ÜÈS•ÈHĞSQBˆ
ˆ”SQKY\ˆHØ[YH]™Y›Ü™H]™XXÚ\ÈHØÜ™Y[‹ˆY][\È›È\]Z]˜[[Ùˆ™˜]Âˆ
ˆ[Ü™H[È[ˆ[™XYK\™\Ù[Y˜]ØX›Hˆ™\Ù[˜]ØX›JØÛÛ[Z]\È\›Z[˜[ˆÛÈ[™Ùœ˜[YBˆ
ˆ\™HÛ›HÛÜÙ\Èİ]HØ[YIÜÈİÛˆ[˜ÛÙ\‹HÛÛ[X[™Y™™\ˆ[™˜]ØX›Hİ^H[]™Kˆ
ˆ[™ÙTÜY][š[š\Úœ˜[YJ
H™[İÈKHØ[Yœ›ÛHÙÜÙ‹˜È[ˆÓ	ÜÈÑÑÓÔİØ\Ú[™İÂˆ
ˆÛİK™KˆQ•TˆHİ™\›^IÜÈİÛˆ[˜ÛÙ\ˆ
ÙTÜY][[YİZP™YÚ[”\ÜËÑ[™\ÜÊH\È[ˆKBˆ
ˆ\ÈÚ]XİX[H™\Ù[È[™ÛÛ[Z]ËˆÚ]İ][QİZHZ[[ˆ\Èİ[[œÈ^XİHBˆ
ˆØ[YHØ^NÈÙTÜY][š[š\Úœ˜[YJ
H\È[˜ÛÛ™][Û˜[›İÑWÕÒUÒSQÕRKYØ]Y™XØ]\ÙBˆ
ˆÛÛY][™È\ÈÈ™\Ù[Hœ˜[YHZ]\ˆØ^Kˆ
‹Âœİ]XÈ›ÚYÙÛY][Ù[™Ùœ˜[YJ›ÚY
HÂˆYˆ
[]Ù[˜ÛÙ\ŠH™]\›ÂˆÛ]Ù[˜ÛÙ\ˆ[™[˜ÛÙ[™×NÂˆ]Ù[˜ÛÙ\ˆHš[Â‚ˆYˆ
ÙWÛY][ÜÜİØXİ]™J
JHÂˆÊˆHÛÛ\ÜÚ]H\ÜÎˆXÜ]Z\™HH˜]ØX›H›İÈ
Y™\œ™Yœ›ÛHİ\Ùœ˜[YKÙYH]Âˆ
ˆİÛˆÛÛ[Y[
KØ[\H]ÜØÛÛÜˆKHHØ[YIÜÈ\İYš[š\ÚYÙ™œØÜ™Y[ˆœ˜[YHKBˆ
ˆ[™İÛœØ[\H][ÈH™X[İ]]ˆ]Ù˜]ØX›H\ÈÈ™HÙ]HH[YBˆ
ˆ\È[˜İ[Ûˆ™]\›œÎˆÙTÜY][[YİZP™YÚ[”\ÜÊ
H
ÙÜÙ‹˜Ë[œÈ™]ÙY[ˆ\™Bˆ
ˆ[™ÙTÜY][š[š\Úœ˜[YJ
JHİX\™ÈÛˆYˆ
[]ØÛYYˆ[]Ù˜]ØX›JH™]\›Øˆ
ˆ[™]ÈİÛˆØYXİ[ÛSØY\[™ÈÛˆ\È\ÜÈ]š[™È[™XYHÜš][ˆ™X[ˆ
ˆ^[È›Üˆ]È™\Ù\™Kˆ
‹Âˆ]]Ü™[X\Ù\ÛÛÂˆ]Ù˜]ØX›HHÛ]Û^Y\ˆ™^˜]ØX›WNÂˆYˆ
]Ù˜]ØX›JHÂˆÙÛY][Ù[œİ\™WÜÜİÜ\[[™J
NÂ‚ˆU™[™\”\ÜÑ\ØÜš\Üˆ
œ\ÜÈHÓU™[™\”\ÜÑ\ØÜš\Üˆ™[™\”\ÜÑ\ØÜš\Ü—NÂˆ\ÜË˜ÛÛÜ]XÚY[ÖÌK^\™HH]Ù˜]ØX›K^\™NÂˆÊˆ[Hİ™\Üš][ˆHH[\ØÜ™Y[ˆšX[™ÛH™[İÈKH[›ZÙHH[QİZBˆ
ˆİ™\›^H\ÜËÚXÚ[X™\˜][H\Ù\ÈØYÈ™\Ù\™HTÈ\ÜÉÜÂˆ
ˆİ]]\È\ÜÈ\È›İ[™ÈÈ™\Ù\™Hœ›ÛH™Y›Ü™H]ˆ
‹Âˆ\ÜË˜ÛÛÜ]XÚY[ÖÌK›ØYXİ[ÛˆHUØYXİ[Û‘ÛØ\™NÂˆ\ÜË˜ÛÛÜ]XÚY[ÖÌKœİÜ™PXİ[ÛˆHUİÜ™PXİ[Û”İÜ™NÂ‚ˆYU™[™\ÛÛ[X[™[˜ÛÙ\ˆ[˜ÈHÛ]ØÛYYˆ™[™\ÛÛ[X[™[˜ÛÙ\•Ú]\ØÜš\Üœ\Ü×NÂˆÜ[˜ÈÙ]™[™\”\[[™Tİ]N›]ÜÜ\[[™WNÂˆÜ[˜ÈÙ]œ˜YÛY[^\™N›]ÜØÛÛÜˆ][™^ŒNÂˆÜ[˜ÈÙ]œ˜YÛY[Ø[\\”İ]N›]ÜÜØ[\\ˆ][™^ŒNÂˆYˆ
ÙWÛY][ÙXWÙ[˜X›Y

JHÂˆÊˆ]ÜØÛÛÜ‰ÜÈİÛˆÚ^™K›İH˜]ØX›IÜÈKHH–PH\ÜÈØ[\\Âˆ
ˆ]ÜØÛÛÜ‹[™^[ÜXÚ[™È\ÈÈX]ÚH^\™H™Z[™Âˆ
ˆ™ZYÚ›İ\‹\Ø[\Y›İH
ÜÜÚX›HY™™\™[ÜİYİÛœØ[\JBˆ
ˆİ]]Ú^™KˆX]Ú\ÈÙÛY][Ù[œİ\™WÜÜİÜ\[[™IÜÈœ˜YÛY[ˆ
ˆÚXÚXÛ\™\È\ÈY™™\ˆÛ›HÚ[ˆ–PH\ÈÛÛ\[Y[‹ˆ
‹Âˆ›Ø]™\ÖÌ—HHÈ
›Ø]
[]ÜİË
›Ø]
[]ÜÚNÂˆÜ[˜ÈÙ]œ˜YÛY[]\Îœ™\È[™İœÚ^™[Ùˆ™\È][™^ŒNÂˆBˆÜ[˜È˜]Ôš[Z]]™\Î“Uš[Z]]™U\UšX[™ÛH™\^İ\Œ™\^Ûİ[Œ×NÂˆÜ[˜È[™[˜ÛÙ[™×NÂˆBˆBˆ]Ü™[™\—İ\™Ù]İÈH
Z[Ì—İ
[]Û^Y\‹™˜]ØX›TÚ^™KÚYÂˆ]Ü™[™\—İ\™Ù]ÚH
Z[Ì—İ
[]Û^Y\‹™˜]ØX›TÚ^™KšZYÚÂˆB‚ˆÊˆ™\Ù\™HHØ[YK[Û›H˜]ØX›H™Y›Ü™HÙÜÙ‹˜ÈÚ]™\È[QİZH]ÈØY\™\Ù\š[™È\ÜË‚ˆ
ˆH›]\ÈÜ™\™Y[ˆHØ[YHÛÛ[X[™Y™™\ˆY\ˆHØ[YKÜÜİÛÜšÈ[™™Y›Ü™HBˆ
ˆİ™\›^KÛÈHš]˜]H^\™H\ÈH^Xİœ˜[YHŒÈØ\È™\ÜÙYÛˆÚ]İ]XYÈRKˆ
‹ÂˆYˆ
]ÙXY×Ü™XY˜XÚ×Ø\›YY	‰ˆÙTÜXYÛ›ÜİXÔØÜ™Y[œÚİ]

HOH•S
HÂˆYˆ
]Ù˜]ØX›HOHš[	‰ˆ]ØÛYYˆOHš[
HÂˆ”ÕR[YÙ\ˆÈH]Ù˜]ØX›K^\™KÚYÂˆ”ÕR[YÙ\ˆH]Ù˜]ØX›K^\™KšZYÚÂˆU^\™Q\ØÜš\Üˆ
™\ØÈBˆÓU^\™Q\ØÜš\Üˆ^\™L‘\ØÜš\Ü•Ú]^[›Ü›X]“U^[›Ü›X]‘ÔN[›Ü›BˆÚYÂˆZYÚšˆZ\X\Y““×NÂˆ\ØËœİÜ˜YÙS[ÙHHUİÜ˜YÙS[ÙTÚ\™YÂˆ]ÙXY×ØØ\\™Wİ^HÛ]Ù]šXÙH™]Õ^\™UÚ]\ØÜš\Ü™\Ø×NÂˆYˆ
]ÙXY×ØØ\\™Wİ^OHš[
HÂˆYU›]ÛÛ[X[™[˜ÛÙ\ˆ›]HÛ]ØÛYYˆ›]ÛÛ[X[™[˜ÛÙ\—NÂˆØ›]ÛÜQœ›ÛU^\™N›]Ù˜]ØX›K^\™BˆÛİ\˜ÙTÛXÙNŒˆÛİ\˜ÙS]™[ŒˆÛİ\˜ÙSÜšYÚ[“UÜšYÚ[“XZÙJ
BˆÛİ\˜ÙTÚ^™N“UÚ^™SXZÙJËJBˆÕ^\™N›]ÙXY×ØØ\\™Wİ^ˆ\İ[˜][Û”ÛXÙNŒˆ\İ[˜][Û“]™[Œˆ\İ[˜][Û“ÜšYÚ[“UÜšYÚ[“XZÙJ
WNÂˆØ›][™[˜ÛÙ[™×NÂˆH[ÙHÂˆÙTÜXYÛ›ÜİXÔØÜ™Y[œÚİÛÛ\]J
NÂˆBˆH[ÙHÂˆÙTÜXYÛ›ÜİXÔØÜ™Y[œÚİÛÛ\]J
NÂˆBˆBŸB‚›ÚYÙTÜY][š[š\Úœ˜[YJ›ÚY
HÂˆ[™\İÜ™WÙœ˜[YXY™™\—ÛÛ›HH]ÙXY×Ü™XY˜XÚ×Ø\›YYÂˆYˆ
[]ØÛYYŠHÂˆYˆ
™\İÜ™WÙœ˜[YXY™™\—ÛÛ›JHÂˆÙTÜXYÛ›ÜİXÔØÜ™Y[œÚİÛÛ\]J
NÂˆ]ÙXY×ØØ\\™Wİ^Hš[Âˆ]ÙXY×Ü™XY˜XÚ×Ø\›YYHÂˆÈÛÛœİÚ\ˆ
™HHÙ][Š‘ÑU—ÔÒÕ”SQHŠNÂˆYˆ
JH	‰ˆ
™JJH]Û^Y\‹™œ˜[YXY™™\“Û›HHQTÎÈBˆBˆ™]\›ÂˆBˆYˆ
]Ù˜]ØX›JHÛ]ØÛYYˆ™\Ù[˜]ØX›N›]Ù˜]ØX›WNÂˆÛ]ØÛYYˆÛÛ[Z]NÂˆÙWÜÚİÛX^X™WÛY][
]ØÛYY‹]Ù˜]ØX›K^\™JNÂˆ]ÙXY×ØØ\\™Wİ^Hš[Âˆ]ÙXY×Ü™XY˜XÚ×Ø\›YYHÂˆYˆ
™\İÜ™WÙœ˜[YXY™™\—ÛÛ›JHÂˆÛÛœİÚ\ˆ
™HHÙ][Š‘ÑU—ÔÒÕ”SQHŠNÂˆYˆ
JH	‰ˆ
™JJH]Û^Y\‹™œ˜[YXY™™\“Û›HHQTÎÂˆBˆ]ØÛYYˆHš[Âˆ]Ù˜]ØX›HHš[Âˆ]İ˜›×Ú[™^H
]İ˜›×Ú[™^
ÈJH	H“×ÔÓÓĞÓÕS•ÂŸB‚ˆÚY™YˆÑWÕÒUÒSQÕRBš[ÙTÜY][[YİZR[š]
›ÚY
HÂˆYˆ
R[QİZWÒ[\Y][Ò[š]
]Ù]šXÙJJH™]\›ˆÂˆÊˆZ[U‘T–H]šXÙHØš™XİKH\\İ[˜Ú[İ]H[™H›Û]\È^\™HKH“ÕËšXBˆ
ˆÜ™X]Q]šXÙSØš™XİÊ
K›İ^š[H[œÚYHHš\œİ[QİZWÒ[\Y][Ó™]Ñœ˜[YJ
HØ[ˆÛÂˆ
ˆYÜË›İ[™Ûˆ\È^XİÛÙH]
ÙWÛ][˜Ú\—ÛY][›[H]›İš\œİÈÙYH]È]XÚˆ
ˆÛ™Ù\ˆÛÛ[Y[›ÜˆH[İÜJN‚ˆ
ˆKˆ[QİZN“™]Ñœ˜[YJ
H\ÜÙ\ÈË’SË‘›ÛËO’\ĞZ[

H™Y›Ü™H[H™[™\™\ˆ˜XÚÙ[™Ø[ˆ
ˆ[œÈ][ÛÈH]\È]\İ^\İ™Y›Ü™HHÛÜ	ÜÈš\œİ™]Ñœ˜[YJ
HKH›İˆ
ˆY\™[H™Y›Ü™HHš\œİ˜]ËÚXÚ\ÈÚ\™HÙTÜY][[YİZP™YÚ[”\ÜÊ
H[œË‚ˆ
ˆ‹ˆ[QİZWÒ[\Y][Ó™]Ñœ˜[YJ
H^š[HØ[ÈÜ™X]Q]šXÙSØš™XİÊ
H]Ù[ˆHš\œİ[YBˆ
ˆ
Yˆ
\İ[˜Ú[İ]HOHš[
X
K[™Ü™X]Q]šXÙSØš™XİÊ
H[˜ÛÛ™][Û˜[Bˆ
ˆ‘P•RSÈH›Û^\™H]™[ˆYˆÛ™H[™XYH^\İËˆØ[[™È\İˆ
ˆÜ™X]Q›ÛÕ^\™J
H\™H
Hš\œİ[˜ÛÛ\]Hš^
HY\İ[˜Ú[İ]Hš[ˆ
ˆÛÈ]^H™XZ[İ[š\™YÛˆHš\œİ™YÚ[”\ÜÊ
HKHY\ˆ[QİZN”™[™\Š
Bˆ
ˆY[™XYH™XÛÜ™Y˜]ÈÛÛ[X[™È™Y™\™[˜Ú[™ÈH’T”Õ^\™KÚXÚTÈ[‚ˆ
ˆX[ØØ]Yİ]œ›ÛH[™\ˆ[HH[ÛY[HÙXÛÛ™Û™H™\XÙY][ˆBˆ
ˆİ›Û™È›Ü\KˆÜ™X]Q]šXÙSØš™XİÊ
HÙ]È\İ[˜Ú[İ]HÛËÛÈH^Bˆ
ˆœ˜[˜Ú™]™\ˆš\™\Ëˆ
‹Âˆ™]\›ˆ[QİZWÒ[\Y][ĞÜ™X]Q]šXÙSØš™XİÊ]Ù]šXÙJHÈHˆÂŸB‚‹ÊˆHÙXÛÛ™™[™\ˆ\ÜÈÛˆHĞSQH˜]ØX›H^\™HHØ[YH\İ™]È[ËØYXİ[ÛBˆ
ˆØYÛÈÜÙH^[È\™HÙ\˜]\ˆ[ˆÛX\™Yˆ[QİZWÒ[\Y][Ó™]Ñœ˜[YHÛ›Hİ\Ú\Âˆ
ˆ\È\ÜÉÜÈ^[›Ü›X]ÜØ[\HÛİ[›Üˆ\[[™K\İ]HX]Ú[™È[™^š[HÜ™X]\Âˆ
ˆ]šXÙHØš™XİÈ
›Û^\™JHÛˆš\œİØ[KH]Ù\È›İİXÚ[QİZRSËÛ^[İ]İ]KÛÂˆ
ˆØ[[™È]\™H˜]\ˆ[ˆ]HÛÛ™[[Û˜[İ\[Ù‹Yœ˜[YHÚ[
Ú\™HÙH]™H›Âˆ
ˆ\ÜÈY]KHÙİØ\KOœİ\Ùœ˜[YJ
H[œÈ™Y›Ü™HÙÜ˜\KOœİ\Ùœ˜[YJ
KÙYHÙÜË˜ÊH\Âˆ
ˆØY™Kˆ›È\]XÚY[ˆ[QİZHÙ\Û‰İ\İÜˆÜš]H\ˆ
‹Â›ÚYÙTÜY][[YİZP™YÚ[”\ÜÊ›ÚY
HÂˆYˆ
[]ØÛYYˆ[]Ù˜]ØX›JH™]\›Âˆ]]Ü™[X\Ù\ÛÛÂˆU™[™\”\ÜÑ\ØÜš\Üˆ
œ\ÜÈHÓU™[™\”\ÜÑ\ØÜš\Üˆ™[™\”\ÜÑ\ØÜš\Ü—NÂˆ\ÜË˜ÛÛÜ]XÚY[ÖÌK^\™HH]Ù˜]ØX›K^\™NÂˆ\ÜË˜ÛÛÜ]XÚY[ÖÌK›ØYXİ[ÛˆHUØYXİ[Û“ØYÂˆ\ÜË˜ÛÛÜ]XÚY[ÖÌKœİÜ™PXİ[ÛˆHUİÜ™PXİ[Û”İÜ™NÂˆ[QİZWÒ[\Y][Ó™]Ñœ˜[YJ\ÜÊNÂˆ]Ûİ™\›^WÙ[˜ÛÙ\ˆHÛ]ØÛYYˆ™[™\ÛÛ[X[™[˜ÛÙ\•Ú]\ØÜš\Üœ\Ü×NÂˆÊˆØ[YH™X\ÛÛš[™È\ÈHØ[YH[˜ÛÙ\‰ÜÈY[XØ[Ø[X›İ™HKHX]Ú[™È\İ™X[Bˆ
ˆX[˜\Ú\ÚXÚÙ]È\ÈÛˆ]™\H[˜ÛÙ\ˆ]Ü™X]\Ë›İ\İÛ™Kˆ
‹ÂˆÛ]Ûİ™\›^WÙ[˜ÛÙ\ˆÙ]\Û\[ÙN“U\Û\[ÙPÛ[\NÂˆBŸB‚š[ÙTÜY][[YİZT™[™\‘˜]Ñ]J›ÚY
™˜]×Ù]JHÂˆYˆ
[]Ûİ™\›^WÙ[˜ÛÙ\ŠH™]\›ˆÂˆ[QİZWÒ[\Y][Ô™[™\‘˜]Ñ]J
[Q˜]Ñ]H
ŠY˜]×Ù]K]ØÛYY‹]Ûİ™\›^WÙ[˜ÛÙ\ŠNÂˆ™]\›ˆNÂŸB‚›ÚYÙTÜY][[YİZQ[™\ÜÊ›ÚY
HÂˆYˆ
[]Ûİ™\›^WÙ[˜ÛÙ\ŠH™]\›ÂˆÛ]Ûİ™\›^WÙ[˜ÛÙ\ˆ[™[˜ÛÙ[™×NÂˆ]Ûİ™\›^WÙ[˜ÛÙ\ˆHš[ÂŸB‚›ÚYÙTÜY][[YİZTÚ]İÛŠ›ÚY
HÂˆ[QİZWÒ[\Y][ÔÚ]İÛŠ
NÂŸBˆÙ[™YˆÊˆÑWÕÒUÒSQÕRH
‹Â‚œİ]XÈ›ÚYÙÛY][Ùš[š\ÚÜ™[™\Š›ÚY
HÂŸB‚œİ]XÈ›ÚYÙÛY][ÜÚ]İÛŠ›ÚY
HÂŸB‚œİXİÙ™[™\š[™ĞTHÙÛY][Ø\HHÂˆÙÛY][Ş—Ú\×Ùœ›ÛWÌİ×ÌKˆÙÛY][İ[›ØYÜÚY\‹ˆÙÛY][ÛØYÜÚY\‹ˆÙÛY][ØÜ™X]WØ[™ÛØYÛ™]×ÜÚY\‹ˆÙÛY][ÛÛÚİ\ÜÚY\‹ˆÙÛY][ÜÚY\—ÙÙ]Ú[™›ËˆÙÛY][Û™]×İ^\™KˆÙÛY][ÜÙ[Xİİ^\™KˆÙÛY][İ\ØYİ^\™KˆÙÛY][İ\ØYÚZYÚİ^\™KˆÙÛY][ØÛX\—ÚZYÚİ^\™KˆÙÛY][ÜÙ]ÜØ[\\—Ü\˜[Y]\œËˆÙÛY][ÜÙ]Ù\İ\İˆÙÛY][ÜÙ]Ù\ÛX\ÚËˆÙÛY][ÜÙ]Ş›[ÙWÙXØ[ˆÙÛY][ÜÙ]Ş›[ÙWØÛİYˆÙÛY][ÜÙ]İšY]ÜÜˆÙÛY][ÜÙ]ÜØÚ\ÜÛÜ‹ˆÙÛY][ÜÙ]İ\ÙWØ[KˆÙÛY][Ù˜]×İšX[™Û\ËˆÙÛY][Ú[š]ˆÙÛY][ÛÛ—Ü™\Ú^™KˆÙÛY][Üİ\Ùœ˜[YKˆÙÛY][Ù[™Ùœ˜[YKˆÙÛY][Ùš[š\ÚÜ™[™\‹ˆÙÛY][ÜÚ]İÛ‚ŸNÂ‚ˆÙ[™YˆËÈTWÓQUS