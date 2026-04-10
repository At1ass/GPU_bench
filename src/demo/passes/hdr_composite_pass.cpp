#include "demo/passes/hdr_composite_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"

void HDRCompositePass::init(const TierResourceView& res) {
    if (res.t4.hdr.tone_map_shader)
        ub_tone_map_.init(res.t4.hdr.tone_map_shader);
}

void HDRCompositePass::execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                               const DemoTierConfig& cfg, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)scene;

    r->setViewport(0, 0, fd.viewport_w, fd.viewport_h);
    r->setDepthTest(false);
    r->setCullFace(false);
    r->setBlending(false);

    ub_tone_map_.use();

    r->bindRenderTargetTexture(res.t4.hdr.scene_rt, 0);
    ub_tone_map_.set(U::SceneTex, 0);

    // Bloom: prefer compute bloom mip[0], otherwise fragment bloom
    if (cfg.enable_compute_bloom && res.t4.bloom.mips[0] != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res.t4.bloom.mips[0]);
    } else {
        r->bindRenderTargetTexture(res.bloom.bright_rt, 1);
    }
    ub_tone_map_.set(U::BloomTex, 1);
    ub_tone_map_.set(U::BloomStrength, res.bloom.strength);

    // AO: prefer compute GTAO, otherwise fragment SSAO
    if (cfg.enable_gtao && res.t4.gtao.blur_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(2, res.t4.gtao.blur_tex);
        ub_tone_map_.set(U::SsaoTex, 2);
        ub_tone_map_.set(U::HasSsao, 1.0f);
    } else if (fd.has_ssao) {
        r->bindRenderTargetTexture(res.ssao.blur_rt, 2);
        ub_tone_map_.set(U::SsaoTex, 2);
        ub_tone_map_.set(U::HasSsao, 1.0f);
    } else {
        ub_tone_map_.set(U::HasSsao, 0.0f);
    }

    if (fd.has_volumetric_fog) {
        r->bindRenderTargetTexture(res.t4.hdr.fog_rt, 3);
        ub_tone_map_.set(U::FogTex, 3);
        ub_tone_map_.set(U::HasFog, 1.0f);
    } else {
        ub_tone_map_.set(U::HasFog, 0.0f);
    }

    // SSR texture
    if (cfg.enable_ssr && res.t4.ssr.tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res.t4.ssr.tex);
        ub_tone_map_.set(U::SsrTex, 4);
        ub_tone_map_.set(U::HasSsr, 1.0f);
    } else {
        ub_tone_map_.set(U::HasSsr, 0.0f);
    }

    // DoF texture
    if (cfg.enable_dof && res.t4.dof.tex != INVALID_TEXTURE) {
        r->bindTextureUnit(5, res.t4.dof.tex);
        ub_tone_map_.set(U::DofTex, 5);
        ub_tone_map_.set(U::HasDof, 1.0f);
    } else {
        ub_tone_map_.set(U::HasDof, 0.0f);
    }

    ub_tone_map_.set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub_tone_map_.set(U::Time, fd.time);

    // Auto-exposure: read exposure from SSBO
    float exposure = 1.0f;
    if (cfg.enable_auto_exposure && res.t4.exposure.exposure_ssbo != INVALID_BUFFER) {
        ComputeFeatures* cf = r->features<ComputeFeatures>();
        if (cf) {
            cf->readSSBO(res.t4.exposure.exposure_ssbo, &exposure, 0, sizeof(float));
            if (exposure < 0.01f) exposure = 1.0f;
        }
    }
    // Compensate for canonical ACES pre-exposure (0.6 vs previous 0.7)
    ub_tone_map_.set(U::Exposure, exposure * 1.167f);

    // Lens flare: compute sun screen position
    // sun_dir → view space → clip space → screen UV
    float sun_vis = 0.0f;
    float sun_sx = 0.5f, sun_sy = 0.5f;
    if (fd.sun_dir.y > -0.05f) { // sun above horizon
        // Project sun direction as a distant point
        float sw = fd.view.m[0] * fd.sun_dir.x + fd.view.m[4] * fd.sun_dir.y + fd.view.m[8] * fd.sun_dir.z;
        float sh = fd.view.m[1] * fd.sun_dir.x + fd.view.m[5] * fd.sun_dir.y + fd.view.m[9] * fd.sun_dir.z;
        float sz = fd.view.m[2] * fd.sun_dir.x + fd.view.m[6] * fd.sun_dir.y + fd.view.m[10] * fd.sun_dir.z;
        if (sz < 0.0f) { // sun in front of camera (view space -Z is forward)
            // Project to clip space using projection matrix
            float cx = fd.proj.m[0] * sw + fd.proj.m[8] * sz;
            float cy = fd.proj.m[5] * sh + fd.proj.m[9] * sz;
            float cw = fd.proj.m[10] * sz + fd.proj.m[14];
            if (cw < 0.0f) { // valid perspective divide
                sun_sx = (cx / cw) * 0.5f + 0.5f;
                sun_sy = (cy / cw) * 0.5f + 0.5f;
                // Visible if within screen bounds (with margin for off-screen glow)
                if (sun_sx > -0.3f && sun_sx < 1.3f && sun_sy > -0.3f && sun_sy < 1.3f) {
                    sun_vis = 1.0f;
                }
            }
        }
    }
    ub_tone_map_.set(U::SunScreenPos, sun_sx, sun_sy);
    ub_tone_map_.set(U::SunVisible, sun_vis);
    ub_tone_map_.set(U::ChromaticStrength, cfg.chromatic_strength);
    ub_tone_map_.set(U::GrainStrength, cfg.grain_strength);

    ctx.drawMesh(res.bloom.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}
