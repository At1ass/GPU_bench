#pragma once
#include "engine/compute_pass.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class ComputeParticlesPass : public ComputePassBase, public DemoPassMeta {
public:
    const char* name() const override { return "compute_particles"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);

    // ComputePassBase interface
    void onBind(PassContext& ctx, UniformBlock& ub,
                const FrameData& fd) override;
    void onWorkgroups(const FrameData& fd,
                      int& gx, int& gy, int& gz) override;
    unsigned int barrierFlags() const override { return Barrier_SSBO; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ParticleData, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 5; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    ShaderProgram* particle_shader_ = nullptr;
    int particle_count_ = 0;
};
