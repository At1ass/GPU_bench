#include "demo/passes/ssr_copy_pass.h"
#include "demo/demo_scene.h"
#include "demo/tier_resource_view.h"
#include "renderer/features.h"
#include "platform/logger.h"

bool SSRCopyPass::isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides& dbg) const {
    (void)dbg;
    return cfg.enable_ssr;
}

void SSRCopyPass::execute(Renderer* r, FrameData& fd,
                          const TierResourceView& res,
                          const DemoTierConfig& cfg,
                          const SceneData& scene) {
    (void)cfg; (void)scene;

    GL4Features* g4 = r->features<GL4Features>();
    if (!g4) return;

    // Copy HDR RT color (RGBA16F) -> ssr_color_snapshot (RGBA16F)
    // Use glCopyImageSubData for reliable texture-to-texture copy (format must match).
    if (res.t4.ssr_color_snapshot != INVALID_TEXTURE &&
        res.t4.hdr_color_tex != INVALID_TEXTURE) {
        g4->copyImageSubData(res.t4.hdr_color_tex, res.t4.ssr_color_snapshot,
                             fd.viewport_w, fd.viewport_h);
    }

    // Copy HDR RT depth (DEPTH_COMPONENT24) -> ssr_depth_snapshot (DEPTH_COMPONENT24)
    if (res.t4.ssr_depth_snapshot != INVALID_TEXTURE &&
        res.t4.hdr_depth_tex != INVALID_TEXTURE) {
        g4->copyImageSubData(res.t4.hdr_depth_tex, res.t4.ssr_depth_snapshot,
                             fd.viewport_w, fd.viewport_h);
    }

    // Memory barrier: ensure copies are visible to subsequent texture reads
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (cf) {
        cf->computeMemoryBarrier();
    }
}
