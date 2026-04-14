#include "engine/fullscreen_pass.h"
#include "engine/render_state.h"

void FullscreenPass::execute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    if (!pipeline_managed_rt_) {
        int w = out_w_ > 0 ? out_w_ : fd.viewport_w;
        int h = out_h_ > 0 ? out_h_ : fd.viewport_h;
        if (to_screen_)
            ctx.beginScreen(w, h, has_clear_ ? clear_ : nullptr);
        else
            ctx.beginRT(output_rt_, w, h, has_clear_ ? clear_ : nullptr);
    }

    ctx.applyState(RenderState::fullscreen());
    ub_.use();

    // Delegate to subclass for application-specific rendering logic
    onExecute(ctx, fd, scene);

    if (quad_ != MeshHandle())
        ctx.drawMesh(quad_);
    else
        ctx.drawFullscreen();

    if (!pipeline_managed_rt_)
        ctx.endRT();
}
