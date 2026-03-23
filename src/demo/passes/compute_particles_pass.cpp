#include "demo/passes/compute_particles_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

void ComputeParticlesPass::init(const TierResourceView& res) {
    ub_.init(res.t4.compute_particle_shader);
}

void ComputeParticlesPass::execute(Renderer* r, FrameData& fd,
                                   const TierResourceView& res,
                                   const DemoTierConfig& cfg,
                                   const SceneData& scene) {
    (void)cfg;
    (void)scene;

    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    ub_.use();
    cf->bindSSBO(res.t4.particle_ssbo, 0);
    ub_.set(U::Time, fd.time);
    ub_.set(U::Dt, 1.0f / 60.0f);
    ub_.set(U::EmitterPos, 0.0f, -0.5f, 0.0f);

    int groups = (res.t4.compute_particle_count + 255) / 256;
    cf->dispatchCompute(groups, 1, 1);
    cf->computeMemoryBarrier();
}
