#pragma once
#include "engine/geometry_pass.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class ShadowPass : public GeometryPass, public DemoPassMeta {
public:
    const char* name() const override { return "shadow"; }
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
            { ResourceId::ShadowMap, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    int minTier() const override { return 2; }
    bool isEnabled() const override;
    int executionOrder() const override { return 0; }

private:
    const DemoTierConfig* cfg_ = nullptr;
};
