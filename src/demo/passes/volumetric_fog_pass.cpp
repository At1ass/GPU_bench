#include "demo/passes/volumetric_fog_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_utils.h"
#include <cmath>

void VolumetricFogPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                             const DemoDebugOverrides& dbg) {
    res_ = &res;
    cfg_ = &cfg;
    dbg_ = &dbg;
    ub().init(res.shader(ShaderBank::VolumetricFogT4));
    setShader(res.shader(ShaderBank::VolumetricFogT4));
    setOutputRT(res.t4.hdr.fog_rt, res.t4.hdr.fog_w, res.t4.hdr.fog_h);
    setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

bool VolumetricFogPass::isEnabled() const {
    return cfg_ && dbg_ && cfg_->enable_volumetric_fog && !dbg_->skip_vol_fog;
}

void VolumetricFogPass::onExecute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    (void)scene;
    const TierResourceView& res = *res_;
    const DemoTierConfig& cfg = *cfg_;

    // Inputs
    ctx.bindTexture(0, res.t4.hdr.depth_tex);

    // Uniforms
    ub().set(U::DepthTex, 0);

    float fov_rad = kDefaultFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub().set(U::Near, kDefaultNear);
    ub().set(U::Far, kDefaultFar);
    ub().set(U::Aspect, aspect);
    ub().set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub().set(U::SunDir, fd.sun_dir);
    ub().set(U::CamPos, fd.cam_pos);
    ub().set(U::Time, fd.time);
    ub().set(U::FogDensity, cfg.fog_density * 0.5f);
    ub().set(U::FogColor, FOG_COLOR);
    ub().set(U::FogSteps, cfg.fog_steps);

    ub().set(U::ViewInv, computeViewInverse(fd.view));
}
