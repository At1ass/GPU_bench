#pragma once
#include "engine/render_pass.h"
#include "engine/pass_context.h"
#include "engine/uniform_block.h"

// Template for compute dispatch passes: input textures/SSBOs -> compute shader -> output.
// Automatically handles: feature check, shader bind, dispatch, barrier.
// Author implements: onBind(), onWorkgroups().
//
// Application-specific resource setup (shaders, textures, RTs) is handled by
// application-level adapter classes, not by engine virtual methods.
//
// Covers: GTAO, bloom compute, SSR, DoF, auto-exposure, compute particles (~7 passes).
//
// Example:
//   class GTAOPass : public ComputePassBase {
//       void onBind(PassContext& ctx, UniformBlock& ub,
//                   const FrameData& fd) override {
//           ctx.bindRTTexture(TexSlot::Primary, scene_rt_);
//           ctx.bindImage(0, gtao_tex_, false, true);
//           ub.set(U::ScreenSize, (float)fd.viewport_w, (float)fd.viewport_h);
//       }
//       void onWorkgroups(const FrameData& fd,
//                         int& gx, int& gy, int& gz) override {
//           gx = (fd.viewport_w + 15) / 16;
//           gy = (fd.viewport_h + 15) / 16;
//           gz = 1;
//       }
//       unsigned int barrierFlags() const override { return Barrier_Image; }
//   };

class ComputePassBase : public RenderPassBase {
public:
    // Bind input/output resources and set uniforms
    virtual void onBind(PassContext& ctx, UniformBlock& ub,
                        const FrameData& fd) = 0;

    // Calculate dispatch workgroup count
    virtual void onWorkgroups(const FrameData& fd,
                              int& gx, int& gy, int& gz) = 0;

    // Override for barrier optimization (default: all barriers).
    // GTAO: Barrier_Image (image store -> texture read)
    // ComputeParticles: Barrier_SSBO (SSBO write -> SSBO read)
    virtual unsigned int barrierFlags() const { return Barrier_All; }

    QueueType queueType() const override { return QueueType::Compute; }

    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader) { shader_ = shader; }
    UniformBlock& ub() { return ub_; }

private:
    UniformBlock ub_;
    ShaderProgram* shader_ = nullptr;
};
