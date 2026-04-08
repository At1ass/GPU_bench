#include "demo/passes/particle_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "platform/logger.h"

void ParticlePass::init(const TierResourceView& res) {
    ub_.init(res.core.particle_shader);
}

void ParticlePass::execute(PassContext& ctx, FrameData& fd,
                           const TierResourceView& res,
                           const DemoTierConfig& cfg,
                           const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg;
    (void)scene;

    if (!res.core.particle_shader || res.core.particle_mesh == MeshHandle()) return;

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::Time, fd.time);

    r->setDepthTest(true);
    r->setDepthMask(false);  // don't write depth
    r->setBlending(true);
    r->setCullFace(false);

    ctx.drawMesh(res.core.particle_mesh);

    r->setDepthMask(true);
    r->setBlending(false);
    r->setCullFace(true);
}
