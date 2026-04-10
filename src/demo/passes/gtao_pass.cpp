#include "demo/passes/gtao_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>

void GTAOPass::init(const TierResourceView& res) {
    ub_gtao_.init(res.t4.gtao.shader);
    ub_blur_.init(res.t4.gtao.blur_shader);
}

void GTAOPass::execute(PassContext& ctx, FrameData& fd,
                       const TierResourceView& res,
                       const DemoTierConfig& cfg,
                       const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)scene;

    // --- GTAO compute pass ---
    if (!res.t4.gtao.shader || res.t4.gtao.tex == INVALID_TEXTURE) return;

    GL4Features* g4 = r->features<GL4Features>();
    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!g4 || !cf) return;

    ub_gtao_.use();

    r->bindTextureUnit(0, res.t4.hdr.depth_tex);
    ub_gtao_.set(U::DepthTex, 0);

    g4->bindImageTexture(res.t4.gtao.tex, 0, false, true); // write-only

    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub_gtao_.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub_gtao_.set(U::Near, kDemoNear);
    ub_gtao_.set(U::Far, kDemoFar);
    ub_gtao_.set(U::Aspect, aspect);
    ub_gtao_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub_gtao_.set(U::AoRadius, cfg.ssao_radius);
    ub_gtao_.set(U::AoIntensity, cfg.ssao_intensity);

    int gx = (fd.viewport_w + 15) / 16;
    int gy = (fd.viewport_h + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();

    // --- GTAO bilateral blur pass ---
    if (!res.t4.gtao.blur_shader || res.t4.gtao.blur_tex == INVALID_TEXTURE) return;

    ub_blur_.use();

    g4->bindImageTexture(res.t4.gtao.tex, 0, true, false);      // read-only input
    g4->bindImageTexture(res.t4.gtao.blur_tex, 1, false, true);  // write-only output

    r->bindTextureUnit(0, res.t4.hdr.depth_tex);
    ub_blur_.set(U::DepthTex, 0);
    ub_blur_.set(U::ScreenSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub_blur_.set(U::Near, kDemoNear);
    ub_blur_.set(U::Far, kDemoFar);

    gx = (fd.viewport_w + 15) / 16;
    gy = (fd.viewport_h + 15) / 16;
    cf->dispatchCompute(gx, gy, 1);
    g4->imageMemoryBarrier();
}
