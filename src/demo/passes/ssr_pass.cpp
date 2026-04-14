#include "demo/passes/ssr_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_utils.h"
#include <cmath>

void SSRPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                   const DemoDebugOverrides& dbg) {
    (void)dbg;
    res_ = &res;
    cfg_ = &cfg;
    ssr_shader_ = res.shader(ShaderBank::SsrT4);
    ub().init(res.shader(ShaderBank::SsrT4));
    setShader(res.shader(ShaderBank::SsrT4));
}

bool SSRPass::isEnabled() const {
    return cfg_ && cfg_->enable_ssr;
}

void SSRPass::onBind(PassContext& ctx, UniformBlock& ub,
                     const FrameData& fd) {
    const TierResourceView& res = *res_;

    ctx.bindRTTexture(0, res.t4.hdr.scene_rt);
    ub.set(U::SceneTex, 0);
    ctx.bindTexture(1, res.t4.hdr.depth_tex);
    ub.set(U::DepthTex, 1);

    ctx.bindImage(0, res.t4.ssr.tex, false, true); // write-only

    ub.set(U::Proj, fd.proj);
    ub.set(U::View, fd.view);

    ub.set(U::ViewInv, computeViewInverse(fd.view));

    float fov_rad = kDefaultFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub.set(U::Near, kDefaultNear);
    ub.set(U::Far, kDefaultFar);
    ub.set(U::Aspect, aspect);
    ub.set(U::TanHalfFov, tanf(fov_rad * 0.5f));

    ub.set(U::PuddleCount, 0);
    ub.set(U::HasMirror, 0);
}

void SSRPass::onWorkgroups(const FrameData& fd,
                           int& gx, int& gy, int& gz) {
    gx = (fd.viewport_w + 15) / 16;
    gy = (fd.viewport_h + 15) / 16;
    gz = 1;
}
