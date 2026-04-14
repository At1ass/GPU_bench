#pragma once
#include "engine/fullscreen_pass.h"
#include "demo/demo_debug.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct TierResourceView;

class VolumetricFogPass : public FullscreenPass, public DemoPassMeta {
public:
    const char* name() const override { return "volumetric_fog"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);

    // FullscreenPass interface
    void onExecute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::FogResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 100; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
};
