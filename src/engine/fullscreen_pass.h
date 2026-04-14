#pragma once
#include "engine/render_pass.h"
#include "engine/pass_context.h"
#include "engine/uniform_block.h"

// Engine-level fullscreen pass template.
// Handles: RT binding, viewport, fullscreen state, draw quad/triangle.
// Subclasses implement onExecute() with their rendering logic.
//
// For application-specific typed passes (with resource views, configs),
// use an application-level adapter (e.g., DemoFullscreenPass in demo/).

class FullscreenPass : public RenderPassBase {
public:
    // Subclass implements rendering logic
    virtual void onExecute(PassContext& ctx, FrameData& fd, const SceneData& scene) = 0;

    // RenderPassBase::execute — framework implementation
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader) { shader_ = shader; }
    void setQuad(MeshHandle quad) { quad_ = quad; }
    void setOutputRT(RenderTargetHandle rt, int w, int h) {
        output_rt_ = rt; out_w_ = w; out_h_ = h; to_screen_ = false;
    }
    void setOutputScreen(int w, int h) {
        out_w_ = w; out_h_ = h; to_screen_ = true;
    }
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
