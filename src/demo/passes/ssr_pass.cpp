#include "demo/passes/ssr_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>

void SSRPass::init(const TierResourceView& res) {
    ub_.init(res.t4.ssr_shader);
}

void SSRPass::execute(Renderer* r, FrameData& fd,
                      const TierResourceView& res,
                      const DemoTierConfig& cfg,
                      const SceneData& scene) {
    (void)scene;

    if (!res.t4.ssr_shader || res.t4.ssr_tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_.use();

    r->bindRenderTargetTexture(res.t4.hdr_scene_rt, 0);
    ub_.set(U::SceneTex, 0);
    r->bindTextureUnit(1, res.t4.hdr_depth_tex);
    ub_.set(U::DepthTex, 1);

    g4->bindImageTexture(res.t4.ssr_tex, 0, false, true); // write-only

    ub_.set(U::Proj, fd.proj);
    res.t4.ssr_shader->setMat4("u_view", fd.view);

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

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub_.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub_.set(U::Near, kDemoNear);
    ub_.set(U::Far, kDemoFar);
    ub_.set(U::Aspect, aspect);
    ub_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));

    ub_.set(U::PuddleCount, 0);
    res.t4.ssr_shader->set1i("u_has_mirror", 0);

    int gx = (fd.viewport_w + 15) / 16;
    int gy = (fd.viewport_h + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}
