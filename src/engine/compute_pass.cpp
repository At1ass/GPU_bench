#include "engine/compute_pass.h"

void ComputePassBase::execute(PassContext& ctx, FrameData& fd,
                              const TierResourceView& res,
                              const DemoTierConfig& cfg,
                              const SceneData& scene) {
    (void)scene;

    if (!ctx.compute()) return;

    ub_.use();
    bind(ctx, ub_, res, fd, cfg);

    int gx = 0, gy = 0, gz = 0;
    workgroups(fd, cfg, gx, gy, gz);
    ctx.dispatch(gx, gy, gz, barrierFlags());
}
