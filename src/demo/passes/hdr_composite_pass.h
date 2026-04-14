#pragma once
#include "engine/render_pass.h"
#include "engine/uniform_block.h"
#include "demo/pipeline/demo_pass_meta.h"

struct DemoTierConfig;
struct DemoDebugOverrides;
struct TierResourceView;

// T4 HDR composite: ACES tone mapping + bloom + AO + volumetric fog + SSR + DoF.
// Reads from many textures and applies final post-processing.
class HDRCompositePass : public RenderPassBase, public DemoPassMeta {
public:
    HDRCompositePass() = default;

    const char* name() const override { return "hdr_composite"; }
    void init(const TierResourceView& res, const DemoTierConfig& cfg,
              const DemoDebugOverrides& dbg);
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

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
    int minTier() const override { return 4; }
    bool isEnabled() const override;
    int executionOrder() const override { return 200; }
    QueueType queueType() const override { return QueueType::Graphics; }
    PassRole passRole() const override { return PassRole::FinalComposite; }

private:
    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    UniformBlock ub_tone_map_;
};
