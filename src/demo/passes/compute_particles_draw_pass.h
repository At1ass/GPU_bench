#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/scene/demo_scene.h"

class ComputeParticlesDrawPass : public RenderPassBase {
public:
    const char* name() const override { return "compute_particles_draw"; }
    void init(const TierResourceView& res);
    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ParticleData, ResourceDecl::READ },
            { ResourceId::HDRColor,     ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_compute_particles;
    }
    int executionOrder() const override { return 50; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    UniformBlock ub_;
};
