#pragma once
#include "engine/compute_pass.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

class SSRPass : public ComputePassBase, public DemoPassMeta {
public:
    const char* name() const override { return "ssr"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);

    // ComputePassBase interface
    void onBind(PassContext& ctx, UniformBlock& ub,
                const FrameData& fd) override;
    void onWorkgroups(const FrameData& fd,
                      int& gx, int& gy, int& gz) override;
    unsigned int barrierFlags() const override { return Barrier_Image; }

    const ResourceDecl* resourceDecls() const override {
        // NOTE: SSR reads HDRColor/HDRDepth via texture binding, but declaring
        // READ HDRColor creates a cycle with WaterPass (which writes HDRColor
        // and reads SSRResult). Ordering enforced by executionOrder (55 < 60).
        static const ResourceDecl d[] = {
            { ResourceId::SSRResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 55; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    ShaderProgram* ssr_shader_ = nullptr;
};
