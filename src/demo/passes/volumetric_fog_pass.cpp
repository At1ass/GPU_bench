#include "demo/passes/volumetric_fog_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>

void VolumetricFogPass::init(const TierResourceView& res) {
    ub_.init(res.t4.volumetric_fog_shader);
}

void VolumetricFogPass::execute(Renderer* r, FrameData& fd,
                                const TierResourceView& res,
                                const DemoTierConfig& cfg,
                                const SceneData& scene) {
    (void)scene;

    r->bindRenderTarget(res.t4.fog_rt);
    r->setViewport(0, 0, fd.viewport_w / 2, fd.viewport_h / 2);
    r->clear(0.0f, 0.0f, 0.0f, 0.0f);
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_.use();

    r->bindTextureUnit(0, res.t4.hdr_depth_tex);
    ub_.set(U::DepthTex, 0);

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub_.set(U::Near, kDemoNear);
    ub_.set(U::Far, kDemoFar);
    ub_.set(U::Aspect, aspect);
    ub_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub_.set(U::SunDir, fd.sun_dir);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::Time, fd.time);
    ub_.set(U::FogDensity, cfg.fog_density * 0.5f);
    ub_.set(U::FogColor, FOG_COLOR);
    ub_.set(U::FogSteps, cfg.fog_steps);

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
    ub_.set(U::ViewInv, vi);

    r->drawMesh(res.bloom.fullscreen_quad);
    r->bindRenderTarget(INVALID_RENDER_TARGET);
}
