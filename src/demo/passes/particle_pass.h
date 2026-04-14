#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class ParticlePass : public RenderPassBase, public DemoPassMeta {
public:
    const char* name() const override { return "particle"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    int minTier() const override { return 1; }
    bool isEnabled() const override;
    int executionOrder() const override { return 50; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    UniformBlock ub_;
};
