#pragma once
#include "engine/render_pass.h"
#include "demo/demo_scene.h"

// Renders the full scene (sky, opaque, grass, fur, particles) into the bloom scene FBO.
// Sub-passes are invoked via stored pointers set by setSubPasses().
class SceneToFBOPass : public RenderPassBase {
public:
    SceneToFBOPass()
        : sky_pass_(nullptr), opaque_pass_(nullptr)
        , grass_pass_(nullptr), fur_pass_(nullptr)
        , particle_pass_(nullptr), torch_pass_(nullptr) {}

    const char* name() const override { return "scene_to_fbo"; }

    void setSubPasses(RenderPassBase* sky, RenderPassBase* opaque,
                      RenderPassBase* grass, RenderPassBase* fur,
                      RenderPassBase* particle, RenderPassBase* torch = nullptr) {
        sky_pass_ = sky;
        opaque_pass_ = opaque;
        grass_pass_ = grass;
        fur_pass_ = fur;
        particle_pass_ = particle;
        torch_pass_ = torch;
    }

    void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::ShadowMap,   ResourceDecl::READ },
            { ResourceId::SceneColor,  ResourceDecl::WRITE },
            { ResourceId::SceneDepth,  ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 3; }
    DemoTier minTier() const override { return DemoTier::Enhanced; }
    bool isEnabled(const DemoTierConfig& cfg, const DemoDebugOverrides&) const override {
        return cfg.enable_bloom && !cfg.enable_hdr;
    }
    int executionOrder() const override { return 70; }
    QueueType queueType() const override { return QueueType::Graphics; }
    PassRole passRole() const override { return PassRole::SceneContainer; }

private:
    RenderPassBase* sky_pass_;
    RenderPassBase* opaque_pass_;
    RenderPassBase* grass_pass_;
    RenderPassBase* fur_pass_;
    RenderPassBase* particle_pass_;
    RenderPassBase* torch_pass_;
};
