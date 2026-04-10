#include "demo/passes/ssr_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_utils.h"
#include <cmath>

void SSRPass::init(const TierResourceView& res) {
    ssr_shader_ = res.t4.ssr.shader;
    ub().init(res.t4.ssr.shader);
    setShader(res.t4.ssr.shader);
}

void SSRPass::setup(const TierResourceView& res) {
    (void)res;
}

void SSRPass::bind(PassContext& ctx, UniformBlock& ub,
                   const TierResourceView& res,
                   const FrameData& fd,
                   const DemoTierConfig& cfg) {
    (void)cfg;

    ctx.bindRTTexture(0, res.t4.hdr.scene_rt);
    ub.set(U::SceneTex, 0);
    ctx.bindTexture(1, res.t4.hdr.depth_tex);
    ub.set(U::DepthTex, 1);

    ctx.bindImage(0, res.t4.ssr.tex, false, true); // write-only

    ub.set(U::Proj, fd.proj);
    ssr_shader_->setMat4("u_view", fd.view);

    ub.set(U::ViewInv, computeViewInverse(fd.view));

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub.set(U::Near, kDemoNear);
    ub.set(U::Far, kDemoFar);
    ub.set(U::Aspect, aspect);
    ub.set(U::TanHalfFov, tanf(fov_rad * 0.5f));

    ub.set(U::PuddleCount, 0);
    ssr_shader_->set1i("u_has_mirror", 0);
}

void SSRPass::workgroups(const FrameData& fd, const DemoTierConfig& cfg,
                         int& gx, int& gy, int& gz) {
    (void)cfg;
    gx = (fd.viewport_w + 15) / 16;
    gy = (fd.viewport_h + 15) / 16;
    gz = 1;
}
