#include "demo/passes/compute_particles_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/demo_utils.h"

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
    particle_shader_ = res.t4.compute_particle_shader;
    particle_count_ = res.t4.compute_particle_count;
    ub().init(res.t4.compute_particle_shader);
    setShader(res.t4.compute_particle_shader);
}

void ComputeParticlesPass::setup(const TierResourceView& res) {
    (void)res;
}

void ComputeParticlesPass::bind(PassContext& ctx, UniformBlock& ub,
                                const TierResourceView& res,
                                const FrameData& fd,
                                const DemoTierConfig& cfg) {
    (void)cfg;

    ctx.bindSSBO(res.t4.particle_ssbo, 0);
    ub.set(U::Time, fd.time);
    ub.set(U::Dt, 1.0f / 60.0f);
    ub.set(U::EmitterPos, 0.0f, -0.5f, 0.0f);

    float tp[2][3];
    getTorchPositions(tp);
    particle_shader_->set3f("u_torch_pos_0", tp[0][0], tp[0][1], tp[0][2]);
    particle_shader_->set3f("u_torch_pos_1", tp[1][0], tp[1][1], tp[1][2]);
}

void ComputeParticlesPass::workgroups(const FrameData& fd, const DemoTierConfig& cfg,
                                      int& gx, int& gy, int& gz) {
    (void)fd;
    (void)cfg;
    gx = (particle_count_ + 255) / 256;
    gy = 1;
    gz = 1;
}
