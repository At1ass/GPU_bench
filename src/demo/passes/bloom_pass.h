#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

// Fragment bloom pass: extract bright pixels + horizontal blur + vertical blur (ping-pong).
class BloomPass : public RenderPassBase, public DemoPassMeta {
public:
    const char* name() const override { return "bloom"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor,  ResourceDecl::READ },
            { ResourceId::BloomResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    int minTier() const override { return 2; }
    bool isEnabled() const override;
    int executionOrder() const override { return 100; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    UniformBlock ub_bloom_extract_;
    UniformBlock ub_bloom_blur_;
};
