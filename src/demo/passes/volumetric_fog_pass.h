#pragma once
#include "engine/fullscreen_pass.h"
#include "demo/demo_scene.h"
#include "demo/demo_debug.h"

class VolumetricFogPass : public FullscreenPass {
public:
    const char* name() const override { return "volumetric_fog"; }
    void init(const TierResourceView& res);

    // FullscreenPass interface
    void setup(const TierResourceView& res) override;
    void inputs(PassContext& ctx, const TierResourceView& res,
                const FrameData& fd) override;
    void uniforms(UniformBlock& ub, const FrameData& fd,
                  const DemoTierConfig& cfg) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::FogResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 2; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides& dbg) const override {
        return cfg.enable_volumetric_fog && !dbg.skip_vol_fog;
    }
    int executionOrder() const override { return 100; }

private:
    const TierResourceView* res_ = nullptr;
};
