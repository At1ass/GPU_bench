#pragma once
#include "demo/render_pass.h"

// Renders the full scene (sky, opaque, grass, fur, particles) into the bloom scene FBO.
// Sub-passes are invoked via stored pointers set by setSubPasses().
class SceneToFBOPass : public DemoRenderPass {
public:
    SceneToFBOPass()
        : sky_pass_(nullptr), opaque_pass_(nullptr)
        , grass_pass_(nullptr), fur_pass_(nullptr)
        , particle_pass_(nullptr) {}

    const char* name() const override { return "scene_to_fbo"; }

    void setSubPasses(DemoRenderPass* sky, DemoRenderPass* opaque,
                      DemoRenderPass* grass, DemoRenderPass* fur,
                      DemoRenderPass* particle) {
        sky_pass_ = sky;
        opaque_pass_ = opaque;
        grass_pass_ = grass;
        fur_pass_ = fur;
        particle_pass_ = particle;
    }

    void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                 const DemoTierConfig& cfg, const SceneData& scene) override;

private:
    DemoRenderPass* sky_pass_;
    DemoRenderPass* opaque_pass_;
    DemoRenderPass* grass_pass_;
    DemoRenderPass* fur_pass_;
    DemoRenderPass* particle_pass_;
};
