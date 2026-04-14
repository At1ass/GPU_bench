#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

// SSAO pass: compute ambient occlusion from scene depth + blur.
// Combines renderSSAOPass() and renderSSAOBlur() into a single execute().
class SSAOPass : public RenderPassBase, public DemoPassMeta {
public:
    const char* name() const override { return "ssao"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneDepth, ResourceDecl::READ },
            { ResourceId::AOResult,   ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    int minTier() const override { return 2; }
    bool isEnabled() const override;
    int executionOrder() const override { return 100; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    void renderSSAO(PassContext& ctx, Renderer* r, FrameData& fd);
    void renderSSAOBlur(PassContext& ctx, Renderer* r, FrameData& fd);

    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    UniformBlock ub_ssao_;
    UniformBlock ub_ssao_blur_;
};
