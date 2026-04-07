#include "demo/passes/volumetric_fog_pass.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_utils.h"
#include <cmath>

void VolumetricFogPass::init(const TierResourceView& res) {
    res_ = &res;
    ub().init(res.t4.volumetric_fog_shader);
    setShader(res.t4.volumetric_fog_shader);
    setQuad(res.bloom.fullscreen_quad);
    setOutputRT(res.t4.fog_rt, res.t4.fog_w, res.t4.fog_h);
    setClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

void VolumetricFogPass::setup(const TierResourceView& res) {
    (void)res;
}

void VolumetricFogPass::inputs(PassContext& ctx, const TierResourceView& res,
                               const FrameData& fd) {
    (void)fd;
    ctx.bindTexture(0, res.t4.hdr_depth_tex);
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

    // Inverse view matrix (column-major: rotation = transpose, translation = -R^T * t)
    Mat4 vi;
    // Transpose 3x3 rotation
    vi.m[0] = fd.view.m[0]; vi.m[1] = fd.view.m[4]; vi.m[2] = fd.view.m[8];  vi.m[3]  = 0;
    vi.m[4] = fd.view.m[1]; vi.m[5] = fd.view.m[5]; vi.m[6] = fd.view.m[9];  vi.m[7]  = 0;
    vi.m[8] = fd.view.m[2]; vi.m[9] = fd.view.m[6]; vi.m[10] = fd.view.m[10]; vi.m[11] = 0;
    // Translation: -R^T * t  (t is in column 3: m[12], m[13], m[14])
    vi.m[12] = -(vi.m[0]*fd.view.m[12] + vi.m[4]*fd.view.m[13] + vi.m[8]*fd.view.m[14]);
    vi.m[13] = -(vi.m[1]*fd.view.m[12] + vi.m[5]*fd.view.m[13] + vi.m[9]*fd.view.m[14]);
    vi.m[14] = -(vi.m[2]*fd.view.m[12] + vi.m[6]*fd.view.m[13] + vi.m[10]*fd.view.m[14]);
    vi.m[15] = 1;
    ub.set(U::ViewInv, vi);
}
