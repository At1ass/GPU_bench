#pragma once
#include "engine/geometry_pass.h"
#include "demo/scene/demo_scene.h"

class ShadowPass : public GeometryPass {
public:
    const char* name() const override { return "shadow"; }
    void init(const TierResourceView& res);

    // GeometryPass interface
    void setup(const TierResourceView& res) override;
    void sceneSetup(UniformBlock& ub, PassContext& ctx,
                    const FrameData& fd,
                    const TierResourceView& res,
                    const DemoTierConfig& cfg) override;
    bool objectFilter(const SceneObject& obj,
                      const FrameData& fd) override;
    void perObject(UniformBlock& ub, PassContext& ctx,
                   const SceneObject& obj) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ShadowMap, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_shadows;
    }
    int executionOrder() const override { return 0; }
};
