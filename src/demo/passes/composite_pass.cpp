#include "demo/passes/composite_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"

void CompositePass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                         const DemoDebugOverrides& dbg) {
    (void)dbg;
    res_ = &res;
    cfg_ = &cfg;
    if (res.shader(ShaderBank::BloomComposite)) {
        ub().init(res.shader(ShaderBank::BloomComposite));
        setShader(res.shader(ShaderBank::BloomComposite));
    }
    setPipelineManagedRT();
}

bool CompositePass::isEnabled() const {
    return cfg_ && cfg_->enable_bloom && !cfg_->enable_hdr;
}

void CompositePass::onExecute(PassContext& ctx, FrameData& fd, const SceneData& scene) {
    (void)scene;
    const TierResourceView& res = *res_;

    // Inputs
    ctx.bindRTTexture(0, res.scene_rt);
    ctx.bindRTTexture(1, res.bloom.bright_rt);
    if (fd.has_ssao) {
        ctx.bindRTTexture(2, res.ssao.blur_rt);
    }

    // Uniforms
    ub().set(U::SceneTex, 0);
    ub().set(U::BloomTex, 1);
    ub().set(U::BloomStrength, res.bloom.strength);
    ub().set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
    ub().set(U::HasSsao, fd.has_ssao ? 1.0f : 0.0f);
    if (fd.has_ssao) ub().set(U::SsaoTex, 2);
}
