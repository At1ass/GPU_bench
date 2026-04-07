#pragma once
#include "engine/compute_pass.h"
#include "demo/demo_scene.h"

class SSRPass : public ComputePassBase {
public:
    const char* name() const override { return "ssr"; }
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
        // NOTE: SSR reads HDRColor/HDRDepth via texture binding, but declaring
        // READ HDRColor creates a cycle with WaterPass (which writes HDRColor
        // and reads SSRResult). Ordering enforced by executionOrder (55 < 60).
        static const ResourceDecl d[] = {
            { ResourceId::SSRResult, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_ssr;
    }
    int executionOrder() const override { return 55; }

private:
    ShaderProgram* ssr_shader_ = nullptr;
};
