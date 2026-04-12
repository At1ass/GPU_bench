#include "demo/passes/dof_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/demo_utils.h"

void DoFPass::init(const TierResourceView& res) {
    ub().init(res.t4.dof.shader);
    setShader(res.t4.dof.shader);
}

void DoFPass::setup(const TierResourceView& res) {
    (void)res;
}

void DoFPass::bind(PassContext& ctx, UniformBlock& ub,
                   const TierResourceView& res,
                   const FrameData& fd,
                   const DemoTierConfig& cfg) {
    ctx.bindRTTexture(0, res.t4.hdr.scene_rt);
    ub.set(U::SceneTex, 0);
    ctx.bindTexture(1, res.t4.hdr.depth_tex);
    ub.set(U::DepthTex, 1);

    ctx.bindImage(0, res.t4.dof.tex, false, true); // write-only

    ub.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub.set(U::Near, kDemoNear);
    ub.set(U::Far, kDemoFar);
    ub.set(U::FocalDistance, cfg.dof_focal_distance);
    ub.set(U::FocalRange, 5.0f);
    ub.set(U::MaxBlur, 5.0f);
    ub.set(U::DofStrength, cfg.dof_strength);
}

void DoFPass::workgroups(const FrameData& fd, const DemoTierConfig& cfg,
                         int& gx, int& gy, int& gz) {
    (void)cfg;
    gx = (fd.viewport_w + 15) / 16;
    gy = (fd.viewport_h + 15) / 16;
    gz = 1;
}
