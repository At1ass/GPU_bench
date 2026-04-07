#pragma once
#include "engine/geometry_pass.h"

class WaterPass : public GeometryPass {
public:
    const char* name() const override { return "water"; }
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
            { ResourceId::ShadowMap, ResourceDecl::READ },
            { ResourceId::SSRResult, ResourceDecl::READ },
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::HDRColor,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 4; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig&, const DemoDebugOverrides&) const override {
        return true;
    }
    int executionOrder() const override { return 60; }

private:
    ShaderProgram* island_shader_ = nullptr;
};
