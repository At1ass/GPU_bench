#include "demo/passes/ssao_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "geometry/math_types.h"
#include <cmath>

void SSAOPass::init(const TierResourceView& res) {
    if (res.ssao.shader)
        ub_ssao_.init(res.ssao.shader);
    if (res.ssao.blur_shader)
        ub_ssao_blur_.init(res.ssao.blur_shader);
}

void SSAOPass::execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                       const DemoTierConfig& cfg, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)scene;
    renderSSAO(r, fd, res, cfg);
    renderSSAOBlur(r, fd, res);
}

void SSAOPass::renderSSAO(Renderer* r, FrameData& fd, const TierResourceView& res,
                          const DemoTierConfig& cfg) {
    r->bindRenderTarget(res.ssao.rt);
    r->setViewport(0, 0, fd.viewport_w / 2, fd.viewport_h / 2);
    r->clear(1.0f, 1.0f, 1.0f, 1.0f);  // white = no occlusion
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_ssao_.use();

    // Bind depth texture from scene FBO
    r->bindTextureUnit(0, res.ssao.scene_depth_tex);
    ub_ssao_.set(U::DepthTex, 0);

    // Bind noise texture
    r->bindTextureUnit(1, res.ssao.noise_tex);
    ub_ssao_.set(U::NoiseTex, 1);

    // Projection parameters (perspective: fov=60, near=0.1, far=50)
    float fov_rad = kDemoFovDeg * CB_PI / 180.0f;
    float aspect = static_cast<float>(fd.viewport_w) / static_cast<float>(fd.viewport_h > 0 ? fd.viewport_h : 1);
    ub_ssao_.set(U::ScreenSize,
                 static_cast<float>(fd.viewport_w),
                 static_cast<float>(fd.viewport_h));
    ub_ssao_.set(U::Near, kDemoNear);
    ub_ssao_.set(U::Far, kDemoFar);
    ub_ssao_.set(U::Aspect, aspect);
    ub_ssao_.set(U::TanHalfFov, tanf(fov_rad * 0.5f));
    ub_ssao_.set(U::Radius, cfg.ssao_radius);
    ub_ssao_.set(U::Bias, 0.025f);
    ub_ssao_.set(U::Intensity, cfg.ssao_intensity);

    r->drawMesh(res.bloom.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}

void SSAOPass::renderSSAOBlur(Renderer* r, FrameData& fd, const TierResourceView& res) {
    r->bindRenderTarget(res.ssao.blur_rt);
    r->setViewport(0, 0, fd.viewport_w / 2, fd.viewport_h / 2);
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_ssao_blur_.use();

    r->bindRenderTargetTexture(res.ssao.rt, 0);
    ub_ssao_blur_.set(U::SsaoTex, 0);
    ub_ssao_blur_.set(U::TexelSize,
                      2.0f / static_cast<float>(fd.viewport_w),
                      2.0f / static_cast<float>(fd.viewport_h));

    r->drawMesh(res.bloom.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}
