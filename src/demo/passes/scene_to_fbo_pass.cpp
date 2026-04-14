#include "demo/passes/scene_to_fbo_pass.h"
#include "engine/pass_context.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"
#include "demo/demo_utils.h"

void SceneToFBOPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                          const DemoDebugOverrides& dbg) {
    (void)dbg;
    res_ = &res;
    cfg_ = &cfg;
}

bool SceneToFBOPass::isEnabled() const {
    return cfg_ && cfg_->enable_bloom && !cfg_->enable_hdr;
}

void SceneToFBOPass::execute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    const TierResourceView& res = *res_;

    r->bindRenderTarget(res.scene_rt);
    r->setViewport(0, 0, fd.viewport_w, fd.viewport_h);
    r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
    r->setDepthTest(true);
    r->setCullFace(true);

    if (sky_pass_)
        sky_pass_->execute(ctx, fd, scene);
    r->setBlending(false);
    if (opaque_pass_)
        opaque_pass_->execute(ctx, fd, scene);
    if (grass_pass_)
        grass_pass_->execute(ctx, fd, scene);
    if (fur_pass_)
        fur_pass_->execute(ctx, fd, scene);
    if (torch_pass_)
        torch_pass_->execute(ctx, fd, scene);
    if (particle_pass_)
        particle_pass_->execute(ctx, fd, scene);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}
