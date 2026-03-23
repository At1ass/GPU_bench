#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"
#include "demo/demo_debug.h"

class DoFPass : public DemoRenderPass {
public:
    const char* name() const override { return "dof"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor,  ResourceDecl::READ },
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::DoFResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides& dbg) const override {
        return cfg.enable_dof && !dbg.skip_dof;
    }
    int executionOrder() const override { return 100; }
    QueueType queueType() const override { return QueueType::Compute; }

private:
    UniformBlock ub_;
};
