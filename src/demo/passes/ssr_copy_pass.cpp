#include "demo/passes/ssr_copy_pass.h"
#include "demo/demo_scene.h"
#include "demo/tier_resource_view.h"
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
    if (res.t4.ssr_tex != INVALID_TEXTURE)
        r->copyFramebufferToTexture(res.t4.ssr_tex, fd.viewport_w, fd.viewport_h);
}
