#pragma once
#include "engine/geometry_pass.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class OpaquePass : public GeometryPass, public DemoPassMeta {
public:
    const char* name() const override { return "opaque"; }
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
            { ResourceId::HDRColor,  ResourceDecl::WRITE },
            { ResourceId::HDRDepth,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    int minTier() const override { return 1; }
    bool isEnabled() const override { return true; }
    int executionOrder() const override { return 20; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    ShaderProgram* island_shader_ = nullptr;
};
