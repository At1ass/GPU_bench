#include "demo/passes/volumetric_fog_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/demo_utils.h"
#include <cmath>

void VolumetricFogPass::init(const TierResourceView& res) {
    res_ = &res;
    ub().init(res.t4.hdr.volumetric_fog_shader);
    setShader(res.t4.hdr.volumetric_fog_shader);
    setQuad(res.bloom.fullscreen_quad);
    setOutputRT(res.t4.hdr.fog_rt, res.t4.hdr.fog_w, res.t4.hdr.fog_h);
    setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void VolumetricFogPass::setup(const TierResourceView& res) {
    (void)res;
}

void VolumetricFogPass::inputs(PassContext& ctx, const TierResourceView& res,
                               const FrameData& fd) {
    (void)fd;
    ctx.bindTexture(0, res.t4.hdr.depth_tex);
}

void VolumetricFogPass::uniforms(UniformBlock& ub, const FrameData& fd,
                                 const DemoTierConfig& cfg) {
    ub.set(U::DepthTex, 0);

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub.set(U::Near, kDemoNear);
    ub.set(U::Far, kDemoFar);
    ub.set(U::Aspect, aspect);
    ub.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub.set(U::SunDir, fd.sun_dir);
    ub.set(U::CamPos, fd.cam_pos);
    ub.set(U::Time, fd.time);
    ub.set(U::FogDensity, cfg.fog_density * 0.5f);
    ub.set(U::FogColor, FOG_COLOR);
    ub.set(U::FogSteps, cfg.fog_steps);

    ub.set(U::ViewInv, computeViewInverse(fd.view));
}
