#pragma once
#include "demo/render_pass.h"
#include "demo/demo_debug.h"

struct DemoTierConfig;

class SSRCopyPass : public DemoRenderPass {
public:
    const char* name() const override { return "ssr_copy"; }
    void init(const TierResourceView&) {}
    void execute(Renderer* r, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor, ResourceDecl::READ },
            { ResourceId::SSRResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides& dbg) const override;
    int executionOrder() const override { return 55; }
};
