#include "engine/compute_pass.h"

void ComputePassBase::execute(PassContext& ctx, FrameData& fd,
                              const SceneData& scene) {
    (void)scene;

    if (!ctx.compute()) return;

    ub_.use();
    onBind(ctx, ub_, fd);

    int gx = 0, gy = 0, gz = 0;
    onWorkgroups(fd, gx, gy, gz);
    ctx.dispatch(gx, gy, gz, barrierFlags());
}
