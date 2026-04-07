#pragma once
#include "engine/compute_pass.h"
#include "demo/demo_scene.h"

class ComputeParticlesPass : public ComputePassBase {
public:
    const char* name() const override { return "compute_particles"; }
    void init(const TierResourceView& res);

    // ComputePassBase interface
    void setup(const TierResourceView& res) override;
    void bind(PassContext& ctx, UniformBlock& ub,
              const TierResourceView& res,
              const FrameData& fd,
              const DemoTierConfig& cfg) override;
    void workgroups(const FrameData& fd, const DemoTierConfig& cfg,
                    int& gx, int& gy, int& gz) override;
    unsigned int barrierFlags() const override { return Barrier_SSBO; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ParticleData, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_compute_particles;
    }
    int executionOrder() const override { return 5; }

private:
    ShaderProgram* particle_shader_ = nullptr;
    int particle_count_ = 0;
};
