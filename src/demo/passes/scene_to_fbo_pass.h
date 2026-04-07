#pragma once
#include "demo/render_pass.h"
#include "demo/demo_scene.h"

// Renders the full scene (sky, opaque, grass, fur, particles) into the bloom scene FBO.
// Sub-passes are invoked via stored pointers set by setSubPasses().
class SceneToFBOPass : public DemoRenderPass {
public:
    SceneToFBOPass()
        : sky_pass_(nullptr), opaque_pass_(nullptr)
        , grass_pass_(nullptr), fur_pass_(nullptr)
        , particle_pass_(nullptr), torch_pass_(nullptr) {}

    const char* name() const override { return "scene_to_fbo"; }

    void setSubPasses(DemoRenderPass* sky, DemoRenderPass* opaque,
                      DemoRenderPass* grass, DemoRenderPass* fur,
                      DemoRenderPass* particle, DemoRenderPass* torch = nullptr) {
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
    DemoRenderPass* sky_pass_;
    DemoRenderPass* opaque_pass_;
    DemoRenderPass* grass_pass_;
    DemoRenderPass* fur_pass_;
    DemoRenderPass* particle_pass_;
    DemoRenderPass* torch_pass_;
};
