#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class TessellatedModelPass : public RenderPassBase, public DemoPassMeta {
public:
    const char* name() const override { return "tessellated_model"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    // TessellatedModelPass calls FurPass at the end.
    // Set via this setter so DemoScene can wire it up.
    void setFurPass(RenderPassBase* fur) { fur_pass_ = fur; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ShadowMap, ResourceDecl::READ },
            { ResourceId::HDRColor,  ResourceDecl::WRITE },
            { ResourceId::HDRDepth,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 40; }
    QueueType queueType() const override { return QueueType::Graphics; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    UniformBlock ub_;
    RenderPassBase* fur_pass_ = nullptr;
};
