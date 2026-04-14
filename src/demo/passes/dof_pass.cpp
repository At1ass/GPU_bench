#include "demo/passes/dof_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_utils.h"

void DoFPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                   const DemoDebugOverrides& dbg) {
    res_ = &res;
    cfg_ = &cfg;
    dbg_ = &dbg;
    ub().init(res.shader(ShaderBank::DofT4));
    setShader(res.shader(ShaderBank::DofT4));
}

bool DoFPass::isEnabled() const {
    return cfg_ && dbg_ && cfg_->enable_dof && !dbg_->skip_dof;
}

void DoFPass::onBind(PassContext& ctx, UniformBlock& ub,
                     const FrameData& fd) {
    const TierResourceView& res = *res_;
    const DemoTierConfig& cfg = *cfg_;

    ctx.bindRTTexture(0, res.t4.hdr.scene_rt);
    ub.set(U::SceneTex, 0);
    ctx.bindTexture(1, res.t4.hdr.depth_tex);
    ub.set(U::DepthTex, 1);

    ctx.bindImage(0, res.t4.dof.tex, false, true); // write-only

    ub.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub.set(U::Near, kDefaultNear);
    ub.set(U::Far, kDefaultFar);
    ub.set(U::FocalDistance, cfg.dof_focal_distance);
    ub.set(U::FocalRange, 5.0f);
    ub.set(U::MaxBlur, 5.0f);
    ub.set(U::DofStrength, cfg.dof_strength);
}

void DoFPass::onWorkgroups(const FrameData& fd,
                           int& gx, int& gy, int& gz) {
    gx = (fd.viewport_w + 15) / 16;
    gy = (fd.viewport_h + 15) / 16;
    gz = 1;
}
