#include "engine/fullscreen_pass.h"
#include "engine/render_state.h"

void FullscreenPass::execute(PassContext& ctx, FrameData& fd,
                             const TierResourceView& res,
                             const DemoTierConfig& cfg,
                             const SceneData& scene) {
    (void)scene;

    if (!pipeline_managed_rt_) {
        if (to_screen_)
            ctx.beginScreen(out_w_, out_h_, has_clear_ ? clear_ : nullptr);
        else
            ctx.beginRT(output_rt_, out_w_, out_h_, has_clear_ ? clear_ : nullptr);
    }

    ctx.applyState(RenderState::fullscreen());
    ub_.use();
    inputs(ctx, res, fd);
    uniforms(ub_, fd, cfg);
    ctx.renderer()->drawMesh(quad_);

    if (!pipeline_managed_rt_)
        ctx.endRT();
}
