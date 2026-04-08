#include "demo/passes/bloom_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"

void BloomPass::init(const TierResourceView& res) {
    if (res.bloom.extract_shader)
        ub_bloom_extract_.init(res.bloom.extract_shader);
    if (res.bloom.blur_shader)
        ub_bloom_blur_.init(res.bloom.blur_shader);
}

void BloomPass::execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
                        const DemoTierConfig& cfg, const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg;
    (void)scene;

    int bw = fd.viewport_w / 2;
    int bh = fd.viewport_h / 2;
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    r->setDepthTest(false);
    r->setBlending(false);
    r->setCullFace(false);

    // Extract bright pixels (use HDR FBO if available, otherwise scene FBO)
    r->bindRenderTarget(res.bloom.bright_rt);
    r->setViewport(0, 0, bw, bh);
    ub_bloom_extract_.use();
    RenderTargetHandle bloom_source = (res.t4.hdr.scene_rt != INVALID_RENDER_TARGET)
                                       ? res.t4.hdr.scene_rt : res.bloom.scene_rt;
    r->bindRenderTargetTexture(bloom_source, 0);
    ub_bloom_extract_.set(U::SceneTex, 0);
    ub_bloom_extract_.set(U::Threshold, 0.8f);
    ctx.drawMesh(res.bloom.fullscreen_quad);

    // Horizontal blur -> blur_rt
    r->bindRenderTarget(res.bloom.blur_rt);
    ub_bloom_blur_.use();
    r->bindRenderTargetTexture(res.bloom.bright_rt, 0);
    ub_bloom_blur_.set(U::Tex, 0);
    ub_bloom_blur_.set(U::Horizontal, 1.0f);
    ub_bloom_blur_.set(U::TexelSize, 1.0f / static_cast<float>(bw),
                                      1.0f / static_cast<float>(bh));
    ctx.drawMesh(res.bloom.fullscreen_quad);

    // Vertical blur -> bright_rt (ping-pong back)
    r->bindRenderTarget(res.bloom.bright_rt);
    r->bindRenderTargetTexture(res.bloom.blur_rt, 0);
    ub_bloom_blur_.set(U::Horizontal, 0.0f);
    ctx.drawMesh(res.bloom.fullscreen_quad);

    r->bindRenderTarget(INVALID_RENDER_TARGET);
    r->setCullFace(true);
}
