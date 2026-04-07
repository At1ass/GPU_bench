#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

class SSRPass : public DemoRenderPass {
public:
    const char* name() const override { return "ssr"; }
    void init(const TierResourceView& res);
    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        // NOTE: SSR reads HDRColor/HDRDepth via texture binding, but declaring
        // READ HDRColor creates a cycle with WaterPass (which writes HDRColor
        // and reads SSRResult). Ordering enforced by executionOrder (55 < 60).
        static const ResourceDecl d[] = {
            { ResourceId::SSRResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_ssr;
    }
    int executionOrder() const override { return 55; }
    QueueType queueType() const override { return QueueType::Compute; }

private:
    UniformBlock ub_;
};
