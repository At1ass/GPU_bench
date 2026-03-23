#include "demo/passes/composite_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"

void CompositePass::init(const TierResourceView& res) {
    if (res.bloom.composite_shader)
        ub_bloom_composite_.init(res.bloom.composite_shader);
}

void CompositePass::execute(Renderer* r, FrameData& fd, const TierResourceView& res,
                            const DemoTierConfig& cfg, const SceneData& scene) {
    (void)cfg;
    (void)scene;

    // Restore destination render target for composite output
    r->bindRenderTarget(fd.dest_rt);
    r->setViewport(0, 0, fd.viewport_w, fd.viewport_h);
    r->setDepthTest(false);
    r->setCullFace(false);

    ub_bloom_composite_.use();
    r->bindRenderTargetTexture(res.bloom.scene_rt, 0);
    ub_bloom_composite_.set(U::SceneTex, 0);
    r->bindRenderTargetTexture(res.bloom.bright_rt, 1);
    ub_bloom_composite_.set(U::BloomTex, 1);
    ub_bloom_composite_.set(U::BloomStrength, res.bloom.strength);
    ub_bloom_composite_.set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));

    // SSAO: bind blurred AO texture
    if (fd.has_ssao) {
        r->bindRenderTargetTexture(res.ssao.blur_rt, 2);
        ub_bloom_composite_.set(U::SsaoTex, 2);
        ub_bloom_composite_.set(U::HasSsao, 1.0f);
    } else {
        ub_bloom_composite_.set(U::HasSsao, 0.0f);
    }

    r->drawMesh(res.bloom.fullscreen_quad);

    r->setDepthTest(true);
    r->setCullFace(true);
}
