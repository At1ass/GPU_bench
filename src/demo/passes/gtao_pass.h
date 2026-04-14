#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/demo_debug.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct TierResourceView;

class GTAOPass : public RenderPassBase, public DemoPassMeta {
public:
    const char* name() const override { return "gtao"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRDepth, ResourceDecl::READ },
            { ResourceId::AOResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 100; }
    QueueType queueType() const override { return QueueType::Compute; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
    UniformBlock ub_gtao_;
    UniformBlock ub_blur_;
};
