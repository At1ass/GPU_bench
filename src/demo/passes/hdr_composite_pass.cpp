#include "demo/passes/hdr_composite_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"

void HDRCompositePass::init(const TierResourceView& res) {
    if (res.t4.tone_map_shader)
        ub_tone_map_.init(res.t4.tone_map_shader);
}

void HDRCompositePass::execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                               const DemoTierConfig& cfg, const SceneData& scene) {
    (void)scene;

    r->setViewport(0, 0, fd.viewport_w, fd.viewport_h);
    r->setDepthTest(false);
    r->setCullFace(false);
    r->setBlending(false);

    ub_tone_map_.use();

    r->bindRenderTargetTexture(res.t4.hdr_scene_rt, 0);
    ub_tone_map_.set(U::SceneTex, 0);

    // Bloom: prefer compute bloom mip[0], otherwise fragment bloom
    if (cfg.enable_compute_bloom && res.t4.bloom_mips[0] != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res.t4.bloom_mips[0]);
    } else {
        r->bindRenderTargetTexture(res.bloom.bright_rt, 1);
    }
    ub_tone_map_.set(U::BloomTex, 1);
    ub_tone_map_.set(U::BloomStrength, res.bloom.strength);

    // AO: prefer compute GTAO, otherwise fragment SSAO
    if (cfg.enable_gtao && res.t4.gtao_blur_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(2, res.t4.gtao_blur_tex);
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
        r->bindRenderTargetTexture(res.t4.fog_rt, 3);
        ub_tone_map_.set(U::FogTex, 3);
        ub_tone_map_.set(U::HasFog, 1.0f);
    } else {
        ub_tone_map_.set(U::HasFog, 0.0f);
    }

    // SSR texture
    if (cfg.enable_ssr && res.t4.ssr_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res.t4.ssr_tex);
        ub_tone_map_.set(U::SsrTex, 4);
        ub_tone_map_.set(U::HasSsr, 1.0f);
    } else {
        ub_tone_map_.set(U::HasSsr, 0.0f);
    }

    // DoF texture
    if (cfg.enable_dof && res.t4.dof_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(5, res.t4.dof_tex);
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
    if (cfg.enable_auto_exposure && res.t4.exposure_ssbo != INVALID_BUFFER) {
        ComputeFeatures* cf = r->features<ComputeFeatures>();
        if (cf) {
            cf->readSSBO(res.t4.exposure_ssbo, &exposure, 0, sizeof(float));
            if (exposure < 0.01f) exposure = 1.0f;
        }
    }
    ub_tone_map_.set(U::Exposure, exposure);
    ub_tone_map_.set(U::ChromaticStrength, cfg.chromatic_strength);
    ub_tone_map_.set(U::GrainStrength, cfg.grain_strength);

    r->drawMesh(res.bloom.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}
