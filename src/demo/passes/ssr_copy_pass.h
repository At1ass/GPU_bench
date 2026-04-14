#pragma once
#include "engine/render_pass.h"
#include "demo/demo_debug.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct TierResourceView;

class SSRCopyPass : public RenderPassBase, public DemoPassMeta {
public:
    const char* name() const override { return "ssr_copy"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        // Note: SSRCopy reads HDRColor but BEFORE WaterPass writes to it.
        // Declaring READ HDRColor would create a cycle with WaterPass
        // (which reads SSRResult). Ordering is by executionOrder (55 < 60).
        static const ResourceDecl d[] = {
            { ResourceId::SSRResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 56; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
};
