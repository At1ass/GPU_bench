#pragma once
#include "engine/geometry_pass.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class WaterPass : public GeometryPass, public DemoPassMeta {
public:
    const char* name() const override { return "water"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);

    // GeometryPass interface
    void onSceneSetup(UniformBlock& ub, PassContext& ctx,
                      const FrameData& fd) override;
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
    int minTier() const override { return 4; }
    bool isEnabled() const override { return true; }
    int executionOrder() const override { return 60; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    ShaderProgram* island_shader_ = nullptr;
};
