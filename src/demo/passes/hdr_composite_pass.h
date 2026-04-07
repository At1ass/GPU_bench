#pragma once
#include "demo/render_pass.h"
#include "demo/uniform_block.h"
#include "demo/demo_scene.h"

// T4 HDR composite: ACES tone mapping + bloom + AO + volumetric fog + SSR + DoF.
// Reads from many textures and applies final post-processing.
class HDRCompositePass : public DemoRenderPass {
public:
    HDRCompositePass() : prev_exposure_(1.0f) {}

    const char* name() const override { return "hdr_composite"; }
    void init(const TierResourceView& res);
    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::HDRColor,     ResourceDecl::READ },
            { ResourceId::BloomResult,  ResourceDecl::READ },
            { ResourceId::AOResult,     ResourceDecl::READ },
            { ResourceId::FogResult,    ResourceDecl::READ },
            { ResourceId::SSRResult,    ResourceDecl::READ },
            { ResourceId::DoFResult,    ResourceDecl::READ },
            { ResourceId::ExposureData, ResourceDecl::READ }
        };
        return d;
    }
    int resourceDeclCount() const override { return 7; }
    DemoTier minTier() const override { return DemoTier::Ultra; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_hdr;
    }
    int executionOrder() const override { return 200; }
    QueueType queueType() const override { return QueueType::Graphics; }
    PassRole passRole() const override { return PassRole::FinalComposite; }

private:
    UniformBlock ub_tone_map_;
    float prev_exposure_;
};
