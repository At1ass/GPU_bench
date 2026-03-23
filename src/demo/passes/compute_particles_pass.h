#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

class ComputeParticlesPass : public DemoRenderPass {
public:
    const char* name() const override { return "compute_particles"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

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
    QueueType queueType() const override { return QueueType::Compute; }

private:
    UniformBlock ub_;
};
