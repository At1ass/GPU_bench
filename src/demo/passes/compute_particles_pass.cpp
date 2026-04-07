#include "demo/passes/compute_particles_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

// Same as TorchPass: on top of column stumps C, D
static void getTorchPositions(float out[2][3]) {
    static const float stump_x[2] = { -3.0f, 3.0f };
    static const float stump_z = -0.5f;
    static const float stump_half_h = 0.6f;
    static const float fire_offset = 0.15f;
    for (int i = 0; i < 2; i++) {
        float terrain_y = sampleTerrainHeight(stump_x[i], stump_z);
        out[i][0] = stump_x[i];
        out[i][1] = terrain_y + stump_half_h + fire_offset;
        out[i][2] = stump_z;
    }
}

void ComputeParticlesPass::init(const TierResourceView& res) {
    ub_.init(res.t4.compute_particle_shader);
}

void ComputeParticlesPass::execute(PassContext& ctx, FrameData& fd,
                                   const TierResourceView& res,
                                   const DemoTierConfig& cfg,
                                   const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg;
    (void)scene;

    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf) return;

    ub_.use();
    cf->bindSSBO(res.t4.particle_ssbo, 0);
    ub_.set(U::Time, fd.time);
    ub_.set(U::Dt, 1.0f / 60.0f);
    ub_.set(U::EmitterPos, 0.0f, -0.5f, 0.0f);

    float tp[2][3];
    getTorchPositions(tp);
    res.t4.compute_particle_shader->set3f("u_torch_pos_0", tp[0][0], tp[0][1], tp[0][2]);
    res.t4.compute_particle_shader->set3f("u_torch_pos_1", tp[1][0], tp[1][1], tp[1][2]);

    int groups = (res.t4.compute_particle_count + 255) / 256;
    cf->dispatchCompute(groups, 1, 1);
    cf->computeMemoryBarrier();
}
