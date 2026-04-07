#pragma once
#include "demo/render_pass.h"
#include "engine/pass_context.h"
#include "demo/uniform_block.h"

// Template for "fullscreen quad" passes: input textures -> shader -> output RT.
// Automatically handles: RT binding, viewport, fullscreen state, draw quad.
// Author implements: setup(), inputs(), optionally uniforms().
//
// Covers: bloom extract, bloom blur, SSAO, SSAO blur, composite,
//         HDR composite, volumetric fog (~7 passes).
//
// Example:
//   class BloomExtract : public FullscreenPass {
//       void setup(const TierResourceView& res) override {
//           setShader(res.bloom.extract_shader);
//           setQuad(res.bloom.fullscreen_quad);
//           setOutputRT(res.bloom.bright_rt, bw, bh);
//       }
//       void inputs(PassContext& ctx, const TierResourceView& res,
//                   const FrameData& fd) override {
//           ctx.bindRTTexture(TexSlot::Primary, bloom_source_rt);
//       }
//       void uniforms(UniformBlock& ub, const FrameData& fd,
//                     const DemoTierConfig& cfg) override {
//           ub.set(U::Threshold, 0.8f);
//       }
//   };

class FullscreenPass : public DemoRenderPass {
public:
    // Author implements:
    virtual void setup(const TierResourceView& res) = 0;
    virtual void inputs(PassContext& ctx,
                        const TierResourceView& res,
                        const FrameData& fd) = 0;
    virtual void uniforms(UniformBlock& ub,
                          const FrameData& fd,
                          const DemoTierConfig& cfg) {}

    // DemoRenderPass::execute — final implementation
    void execute(PassContext& ctx, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader) { shader_ = shader; }
    void setQuad(MeshHandle quad) { quad_ = quad; }
    void setOutputRT(RenderTargetHandle rt, int w, int h) {
        output_rt_ = rt; out_w_ = w; out_h_ = h; to_screen_ = false;
    }
    void setOutputScreen(int w, int h) {
        out_w_ = w; out_h_ = h; to_screen_ = true;
    }
    // Skip RT binding in execute() — for passes where pipeline manages RT
    // (e.g., FinalComposite passes where pipeline does addPassToDest)
    void setPipelineManagedRT() { pipeline_managed_rt_ = true; }
    void setClearColor(float r, float g, float b, float a) {
        has_clear_ = true;
        clear_[0] = r; clear_[1] = g; clear_[2] = b; clear_[3] = a;
    }
    UniformBlock& ub() { return ub_; }

private:
    UniformBlock ub_;
    ShaderProgram* shader_ = nullptr;
    MeshHandle quad_;
    RenderTargetHandle output_rt_;
    int out_w_ = 0, out_h_ = 0;
    bool has_clear_ = false;
    float clear_[4] = {};
    bool to_screen_ = false;
    bool pipeline_managed_rt_ = false;
};
