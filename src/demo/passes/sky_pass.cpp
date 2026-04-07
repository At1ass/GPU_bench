#include "demo/passes/sky_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "platform/logger.h"

void SkyPass::init(const TierResourceView& res) {
    ub_.init(res.core.sky_shader);
}

void SkyPass::execute(PassContext& ctx, FrameData& fd,
                      const TierResourceView& res,
                      const DemoTierConfig& cfg,
                      const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg;
    (void)scene;

    if (!res.core.sky_shader || res.core.sky_mesh == MeshHandle()) return;

    r->setDepthTest(false);
    r->setCullFace(false);

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::SunDir, fd.sun_dir);
    ub_.set(U::Time, fd.time);

    r->drawMesh(res.core.sky_mesh);

    r->setDepthTest(true);
    r->setCullFace(true);
}
