#pragma once
#include "engine/compute_pass.h"
#include "demo/demo_debug.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct TierResourceView;

class DoFPass : public ComputePassBase, public DemoPassMeta {
public:
    const char* name() const override { return "dof"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);

    // ComputePassBase interface
    void onBind(PassContext& ctx, UniformBlock& ub,
                const FrameData& fd) override;
    void onWorkgroups(const FrameData& fd,
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
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 100; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
};
