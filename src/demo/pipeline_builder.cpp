#include "demo/pipeline_builder.h"
#include "demo/demo_scene.h"
#include "demo/demo_utils.h"
#include "platform/logger.h"

void ssrCopyCommand(Renderer* r, FrameData& fd,
                    const TierResourceView& res,
                    const DemoTierConfig& cfg,
                    const SceneData& scene) {
    (void)cfg; (void)scene;
    if (res.t4.ssr_tex != INVALID_TEXTURE)
        r->copyFramebufferToTexture(res.t4.ssr_tex, fd.viewport_w, fd.viewport_h);
}

void buildPipeline(DemoPipeline& pipeline,
                   const DemoPassSet& pass,
                   const DemoTierConfig& config,
                   const DemoDebugOverrides& debug,
                   const TierResourceView& res,
                   int viewport_w, int viewport_h) {
    pipeline.clear();

    bool is_t4_hdr = config.enable_hdr
                     && res.t4.hdr_scene_rt != INVALID_RENDER_TARGET
                     && !debug.skip_hdr;
    bool is_t2t3_bloom = config.enable_bloom && !is_t4_hdr;

    if (is_t4_hdr) {
        // ============================================================
        // T4 Ultra HDR pipeline
        // ============================================================

        // Pre-scene compute
        if (config.enable_shadows)
            pipeline.addPass(pass.shadow);
        if (config.enable_compute_particles)
            pipeline.addPass(pass.compute_particles);

        // Scene to HDR FBO
        pipeline.addPassWithRT(pass.sky, res.t4.hdr_scene_rt,
                               true, FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f,
                               viewport_w, viewport_h);
        pipeline.addPass(pass.opaque);
        pipeline.addPass(pass.grass);
        pipeline.addPass(pass.tess_model, config.enable_tessellation);
        pipeline.addPass(pass.fur, !config.enable_tessellation);
        pipeline.addPass(pass.compute_particles_draw, config.enable_compute_particles);
        pipeline.addPass(pass.particle, !config.enable_compute_particles);

        // SSR copy + water
        pipeline.addCommand(&ssrCopyCommand,
                            config.enable_ssr && res.t4.ssr_tex != INVALID_TEXTURE);
        pipeline.addPass(pass.water);

        // Unbind scene FBO
        pipeline.addRTSwitch(INVALID_RENDER_TARGET);

        // Post-processing
        pipeline.addPass(pass.auto_exposure,
                         config.enable_auto_exposure && !debug.skip_auto_exposure);

        bool use_gtao = !debug.skip_gtao && config.enable_gtao
                        && res.t4.gtao_shader != nullptr
                        && res.t4.gtao_tex != INVALID_TEXTURE;
        pipeline.addPass(pass.gtao, use_gtao);
        pipeline.addPass(pass.ssao, config.enable_ssao && !use_gtao);

        pipeline.addPass(pass.vol_fog,
                         config.enable_volumetric_fog && !debug.skip_vol_fog);

        bool use_compute_bloom = !debug.skip_compute_bloom
                                 && config.enable_compute_bloom
                                 && res.t4.bloom_down_compute != nullptr
                                 && res.t4.bloom_mips[0] != INVALID_TEXTURE;
        pipeline.addPass(pass.bloom_compute, use_compute_bloom);
        pipeline.addPass(pass.bloom, config.enable_bloom && !use_compute_bloom);

        pipeline.addPass(pass.dof,
                         config.enable_dof && res.t4.dof_shader != nullptr
                         && !debug.skip_dof);

        // Compute barrier + final composite
        pipeline.addBarrier(4);
        pipeline.addPassToDest(pass.hdr_composite, false, 0.0f, 0.0f, 0.0f, 0.0f,
                               viewport_w, viewport_h);

    } else if (is_t2t3_bloom) {
        // ============================================================
        // T2/T3: shadow → scene FBO → SSAO → bloom → composite
        // ============================================================
        if (config.enable_shadows)
            pipeline.addPass(pass.shadow);
        pipeline.addPass(pass.scene_to_fbo);
        if (config.enable_ssao)
            pipeline.addPass(pass.ssao);
        pipeline.addPass(pass.bloom);
        pipeline.addPass(pass.composite);

    } else {
        // ============================================================
        // T1 Basic: shadow → clear → sky → opaque → grass → fur → particles
        // ============================================================
        if (config.enable_shadows)
            pipeline.addPass(pass.shadow);
        pipeline.addDefaultFBWithClear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f,
                                       viewport_w, viewport_h);
        pipeline.addPass(pass.sky);
        pipeline.addPass(pass.opaque);
        pipeline.addPass(pass.grass);
        pipeline.addPass(pass.fur);
        pipeline.addPass(pass.particle);
    }

    LOG_DBG("Pipeline: %d nodes (%d enabled) for tier %d",
            static_cast<int>(pipeline.passCount()),
            static_cast<int>(pipeline.enabledCount()),
            static_cast<int>(config.tier));
}
