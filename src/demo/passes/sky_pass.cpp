#include "demo/passes/sky_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"
#include "platform/logger.h"

void SkyPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                   const DemoDebugOverrides& dbg) {
    (void)cfg; (void)dbg;
    res_ = &res;
    ub_.init(res.shader(ShaderBank::Sky));
}

void SkyPass::execute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    const TierResourceView& res = *res_;
    (void)scene;

    if (!res.shader(ShaderBank::Sky) || res.core.sky_mesh == MeshHandle()) return;

    r->setDepthTest(false);
    r->setCullFace(false);

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::SunDir, fd.sun_dir);
    ub_.set(U::Time, fd.time);

    ctx.drawMesh(res.core.sky_mesh);

    r->setDepthTest(true);
    r->setCullFace(true);
}
