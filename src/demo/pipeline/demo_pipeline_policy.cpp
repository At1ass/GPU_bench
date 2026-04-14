#include "demo/pipeline/demo_pipeline_policy.h"
#include "demo/pipeline/demo_pass_meta.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_utils.h"
#include "engine/pipeline_builder.h"
#include "engine/render_pass.h"
#include "engine/resource_id.h"
#include "platform/logger.h"
#include <vector>

// ============================================================
// DemoPipelinePolicy
// ============================================================

DemoPipelinePolicy::DemoPipelinePolicy(Path path,
                                       const TierResourceView& res,
                                       int viewport_w, int viewport_h)
    : path_(path), res_(res)
    , viewport_w_(viewport_w), viewport_h_(viewport_h)
    , hdr_rt_bound_(false), hdr_rt_unbound_(false)
    , fb_cleared_(false) {
    fog_r_ = FOG_COLOR.x;
    fog_g_ = FOG_COLOR.y;
    fog_b_ = FOG_COLOR.z;
    shadow_size_ = res.shadow.map_size > 0 ? res.shadow.map_size : 1024;
}

bool DemoPipelinePolicy::writesResource(const RenderPassBase& pass, ResourceId rid) const {
    const ResourceDecl* decls = pass.resourceDecls();
    int dc = pass.resourceDeclCount();
    for (int d = 0; d < dc; d++)
        if (decls[d].id == rid && decls[d].access == ResourceDecl::WRITE)
            return true;
    return false;
}

bool DemoPipelinePolicy::readsResource(const RenderPassBase& pass, ResourceId rid) const {
    const ResourceDecl* decls = pass.resourceDecls();
    int dc = pass.resourceDeclCount();
    for (int d = 0; d < dc; d++)
        if (decls[d].id == rid && decls[d].access == ResourceDecl::READ)
            return true;
    return false;
}

PassRTBinding DemoPipelinePolicy::pipelinePrologue() const {
    // No prologue node needed -- RT setup is handled per-pass
    return PassRTBinding();
}

PassRTBinding DemoPipelinePolicy::routePass(const RenderPassBase& pass,
                                            int sorted_index, int total_passes) const {
    (void)sorted_index;
    (void)total_passes;
    switch (path_) {
    case Path::HDR:       return routeHDR(pass);
    case Path::SceneFBO:  return routeSceneFBO(pass);
    case Path::DefaultFB: return routeDefaultFB(pass);
    }
    return PassRTBinding();
}

PassSyncHint DemoPipelinePolicy::syncHint(const RenderPassBase& pass) const {
    PassSyncHint h;
    h.barrier_after = (pass.queueType() == QueueType::Compute);
    // HDR composite needs barrier + image unbind before it
    if (path_ == Path::HDR && pass.passRole() == PassRole::FinalComposite) {
        h.barrier_after = true;
        h.unbind_image_count = 4;
    }
    return h;
}

// ============================================================
// T4 HDR pipeline path
// ============================================================
PassRTBinding DemoPipelinePolicy::routeHDR(const RenderPassBase& pass) const {
    bool writes_shadow = writesResource(pass, ResourceId::ShadowMap);
    bool writes_hdr = writesResource(pass, ResourceId::HDRColor);
    bool reads_hdr = readsResource(pass, ResourceId::HDRColor);
    bool reads_hdr_depth = readsResource(pass, ResourceId::HDRDepth);
    bool reads_hdr_post = (reads_hdr || reads_hdr_depth) && !writes_hdr;

    PassRTBinding rt;

    // Shadow pass: bind shadow RT
    if (writes_shadow) {
        rt.action = PassRTBinding::BindRT;
        rt.rt = res_.shadow.rt;
        rt.clear = true;
        rt.clear_color[0] = rt.clear_color[1] = rt.clear_color[2] = rt.clear_color[3] = 1.0f;
        rt.viewport_w = shadow_size_;
        rt.viewport_h = shadow_size_;
        return rt;
    }

    // Compute passes that don't touch FBOs (e.g., compute_particles)
    if (pass.queueType() == QueueType::Compute && !writes_hdr && !reads_hdr_post) {
        // No RT change needed
        return rt;
    }

    // First HDR writer: bind HDR RT with clear
    if (writes_hdr && !hdr_rt_bound_) {
        hdr_rt_bound_ = true;
        rt.action = PassRTBinding::BindRT;
        rt.rt = res_.t4.hdr.scene_rt;
        rt.clear = true;
        rt.clear_color[0] = fog_r_;
        rt.clear_color[1] = fog_g_;
        rt.clear_color[2] = fog_b_;
        rt.clear_color[3] = 1.0f;
        rt.viewport_w = viewport_w_;
        rt.viewport_h = viewport_h_;
        return rt;
    }

    // Subsequent HDR writers (scene geometry passes)
    if (writes_hdr && hdr_rt_bound_ && !hdr_rt_unbound_) {
        // No RT change -- continue rendering to HDR RT
        return rt;
    }

    // Post-processing reader: unbind HDR RT first time
    if (reads_hdr_post && !hdr_rt_unbound_) {
        hdr_rt_unbound_ = true;
        rt.action = PassRTBinding::Unbind;
        // Note: the pass still executes after unbind
        // The old code added an RT switch *before* the pass, then added the pass.
        // In the engine builder model, this RT binding happens before the pass executes.
        return rt;
    }

    // SceneToFBO manages its own FBO
    if (pass.passRole() == PassRole::SceneContainer) {
        return rt;
    }

    // HDR composite: render to dest
    if (pass.passRole() == PassRole::FinalComposite) {
        rt.action = PassRTBinding::BindDest;
        rt.clear = false;
        rt.viewport_w = viewport_w_;
        rt.viewport_h = viewport_h_;
        return rt;
    }

    // Default: no RT change
    return rt;
}

// ============================================================
// T2/T3 scene FBO path
// ============================================================
PassRTBinding DemoPipelinePolicy::routeSceneFBO(const RenderPassBase& pass) const {
    bool writes_shadow = writesResource(pass, ResourceId::ShadowMap);

    PassRTBinding rt;

    if (writes_shadow) {
        rt.action = PassRTBinding::BindRT;
        rt.rt = res_.shadow.rt;
        rt.clear = true;
        rt.clear_color[0] = rt.clear_color[1] = rt.clear_color[2] = rt.clear_color[3] = 1.0f;
        rt.viewport_w = shadow_size_;
        rt.viewport_h = shadow_size_;
        return rt;
    }

    // Composite: render to dest
    if (pass.passRole() == PassRole::FinalComposite) {
        rt.action = PassRTBinding::BindDest;
        rt.clear = false;
        rt.viewport_w = viewport_w_;
        rt.viewport_h = viewport_h_;
        return rt;
    }

    // Everything else (scene_to_fbo, ssao, bloom): no RT change
    return rt;
}

// ============================================================
// T1 default FB path (no FBO)
// ============================================================
PassRTBinding DemoPipelinePolicy::routeDefaultFB(const RenderPassBase& pass) const {
    bool writes_shadow = writesResource(pass, ResourceId::ShadowMap);

    PassRTBinding rt;

    if (writes_shadow) {
        rt.action = PassRTBinding::BindRT;
        rt.rt = res_.shadow.rt;
        rt.clear = true;
        rt.clear_color[0] = rt.clear_color[1] = rt.clear_color[2] = rt.clear_color[3] = 1.0f;
        rt.viewport_w = shadow_size_;
        rt.viewport_h = shadow_size_;
        return rt;
    }

    // Insert default FB clear before first geometry pass
    if (!fb_cleared_) {
        fb_cleared_ = true;
        rt.action = PassRTBinding::Unbind;
        rt.clear = true;
        rt.clear_color[0] = fog_r_;
        rt.clear_color[1] = fog_g_;
        rt.clear_color[2] = fog_b_;
        rt.clear_color[3] = 1.0f;
        rt.viewport_w = viewport_w_;
        rt.viewport_h = viewport_h_;
        return rt;
    }

    // No RT change
    return rt;
}

// ============================================================
// demoBuildPipeline: demo-level entry point
// ============================================================

void demoBuildPipeline(RenderPipeline& pipeline,
                       const std::vector<std::unique_ptr<RenderPassBase>>& passes,
                       const DemoTierConfig& config,
                       const DemoDebugOverrides& debug,
                       const TierResourceView& res,
                       int viewport_w, int viewport_h) {
    (void)debug;  // used indirectly via pass->isEnabled()

    int tier_int = static_cast<int>(config.tier);

    // ----------------------------------------------------------
    // Step 1: filter passes by tier and isEnabled via DemoPassMeta
    // ----------------------------------------------------------
    std::vector<RenderPassBase*> enabled;
    enabled.reserve(passes.size());
    for (size_t i = 0; i < passes.size(); i++) {
        RenderPassBase* p = passes[i].get();
        DemoPassMeta* meta = dynamic_cast<DemoPassMeta*>(p);
        if (meta) {
            if (meta->minTier() > tier_int) continue;
            if (!meta->isEnabled()) continue;
        }
        // Passes without DemoPassMeta are always included
        enabled.push_back(p);
    }

    if (enabled.empty()) return;

    // ----------------------------------------------------------
    // Step 2: detect rendering path
    // ----------------------------------------------------------
    bool has_hdr_writer = false;
    bool has_scene_writer = false;
    bool hdr_rt_available = res.t4.hdr.scene_rt != INVALID_RENDER_TARGET;
    bool scene_rt_available = res.scene_rt != INVALID_RENDER_TARGET;

    for (size_t i = 0; i < enabled.size(); i++) {
        const ResourceDecl* decls = enabled[i]->resourceDecls();
        int dc = enabled[i]->resourceDeclCount();
        for (int d = 0; d < dc; d++) {
            if (decls[d].access == ResourceDecl::WRITE) {
                if (decls[d].id == ResourceId::HDRColor) has_hdr_writer = true;
                if (decls[d].id == ResourceId::SceneColor) has_scene_writer = true;
            }
        }
    }

    has_hdr_writer = has_hdr_writer && hdr_rt_available;
    has_scene_writer = has_scene_writer && scene_rt_available;

    DemoPipelinePolicy::Path path;
    if (has_hdr_writer)
        path = DemoPipelinePolicy::Path::HDR;
    else if (has_scene_writer)
        path = DemoPipelinePolicy::Path::SceneFBO;
    else
        path = DemoPipelinePolicy::Path::DefaultFB;

    // ----------------------------------------------------------
    // Step 3: build pipeline via engine builder + demo policy
    // ----------------------------------------------------------
    DemoPipelinePolicy policy(path, res, viewport_w, viewport_h);
    buildPipeline(pipeline, enabled, policy, viewport_w, viewport_h);

    LOG_DBG("Pipeline: %d nodes (%d enabled) for tier %d, path=%s",
            static_cast<int>(pipeline.passCount()),
            static_cast<int>(pipeline.enabledCount()),
            tier_int,
            has_hdr_writer ? "HDR" : (has_scene_writer ? "SceneFBO" : "DefaultFB"));
}
