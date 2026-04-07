#include "demo/passes/composite_pass.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"

void CompositePass::init(const TierResourceView& res) {
    res_ = &res;
    if (res.bloom.composite_shader) {
        ub().init(res.bloom.composite_shader);
        setShader(res.bloom.composite_shader);
    }
    setQuad(res.bloom.fullscreen_quad);
    setPipelineManagedRT();
}

void CompositePass::setup(const TierResourceView& res) {
    (void)res;
    // Output is dest_rt — handled by pipeline (passRole = FinalComposite)
}

void CompositePass::inputs(PassContext& ctx, const TierResourceView& res,
                           const FrameData& fd) {
    ctx.bindRTTexture(0, res.bloom.scene_rt);
    ctx.bindRTTexture(1, res.bloom.bright_rt);

    if (fd.has_ssao) {
        ctx.bindRTTexture(2, res.ssao.blur_rt);
    }
}

void CompositePass::uniforms(UniformBlock& ub, const FrameData& fd,
                             const DemoTierConfig& cfg) {
    (void)cfg;
    ub.set(U::SceneTex, 0);
    ub.set(U::BloomTex, 1);
    ub.set(U::BloomStrength, res_->bloom.strength);
    ub.set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub.set(U::HasSsao, fd.has_ssao ? 1.0f : 0.0f);
    if (fd.has_ssao) ub.set(U::SsaoTex, 2);
}
