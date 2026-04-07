#pragma once
#include "engine/compute_pass.h"
#include "demo/demo_scene.h"
#include "demo/demo_debug.h"

class DoFPass : public ComputePassBase {
public:
    const char* name() const override { return "dof"; }
    void init(const TierResourceView& res);

    // ComputePassBase interface
    void setup(const TierResourceView& res) override;
    void bind(PassContext& ctx, UniformBlock& ub,
              const TierResourceView& res,
              const FrameData& fd,
              const DemoTierConfig& cfg) override;
    void workgroups(const FrameData& fd, const DemoTierConfig& cfg,
                    int& gx, int& gy, int& gz) override;
    unsigned int barrierFlags() const override { return Barrier_Image; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor,  ResourceDecl::READ },
            { ResourceId::HDRDepth,  ResourceDecl::READ },
            { ResourceId::DoFResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides& dbg) const override {
        return cfg.enable_dof && !dbg.skip_dof;
    }
    int executionOrder() const override { return 100; }
};
