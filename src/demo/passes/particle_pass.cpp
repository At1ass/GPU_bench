#include "demo/passes/particle_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"
#include "platform/logger.h"

void ParticlePass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                        const DemoDebugOverrides& dbg) {
    (void)dbg;
    res_ = &res;
    cfg_ = &cfg;
    ub_.init(res.shader(ShaderBank::Particle));
}

bool ParticlePass::isEnabled() const {
    return !cfg_ || !cfg_->enable_compute_particles;
}

void ParticlePass::execute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    const TierResourceView& res = *res_;
    (void)scene;

    if (!res.shader(ShaderBank::Particle) || res.core.particle_mesh == MeshHandle()) return;

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
