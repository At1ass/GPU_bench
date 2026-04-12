#include "demo/passes/ssr_copy_pass.h"
#include "engine/pass_context.h"
#include "demo/scene/demo_scene.h"
#include "demo/tier/tier_resource_view.h"
#include "renderer/features.h"
#include "platform/logger.h"

bool SSRCopyPass::isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides& dbg) const {
    (void)dbg;
    return cfg.enable_ssr;
}

void SSRCopyPass::execute(PassContext& ctx, FrameData& fd,
                          const TierResourceView& res,
                          const DemoTierConfig& cfg,
                          const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg; (void)scene;

    GL4Features* g4 = r->features<GL4Features>();
    if (!g4) return;

    // Copy HDR RT color (RGBA16F) -> ssr_color_snapshot (RGBA16F)
    // Use glCopyImageSubData for reliable texture-to-texture copy (format must match).
    if (res.t4.ssr.color_snapshot != INVALID_TEXTURE &&
        res.t4.hdr.color_tex != INVALID_TEXTURE) {
        g4->copyImageSubData(res.t4.hdr.color_tex, res.t4.ssr.color_snapshot,
                             fd.viewport_w, fd.viewport_h);
    }

    // Copy HDR RT depth (DEPTH_COMPONENT24) -> ssr_depth_snapshot (DEPTH_COMPONENT24)
    if (res.t4.ssr.depth_snapshot != INVALID_TEXTURE &&
        res.t4.hdr.depth_tex != INVALID_TEXTURE) {
        g4->copyImageSubData(res.t4.hdr.depth_tex, res.t4.ssr.depth_snapshot,
                             fd.viewport_w, fd.viewport_h);
    }

    // Memory barrier: ensure copies are visible to subsequent texture reads
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (cf) {
        cf->computeMemoryBarrier();
    }
}
