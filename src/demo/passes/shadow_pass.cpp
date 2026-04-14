#include "demo/passes/shadow_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"

void ShadowPass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                      const DemoDebugOverrides& dbg) {
    (void)dbg;
    cfg_ = &cfg;
    ShaderProgram* s = res.shader(ShaderBank::ShadowDepth);
    ub().init(s);
    setShader(s);
    setOutputRT(res.shadow.rt, res.shadow.map_size, res.shadow.map_size);
    setState(RenderState::shadow());
    setClearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void ShadowPass::onSceneSetup(UniformBlock& ub, PassContext& ctx,
                               const FrameData& fd) {
    (void)ctx;
    ub.set(U::LightVP, fd.light_vp);
}

bool ShadowPass::isEnabled() const {
    return cfg_ && cfg_->enable_shadows;
}

bool ShadowPass::objectFilter(const SceneObject& obj,
                              const FrameData& fd) {
    (void)fd;
    return !obj.is_water;  // water doesn't cast shadows
}

void ShadowPass::perObject(UniformBlock& ub, PassContext& ctx,
                           const SceneObject& obj) {
    (void)ctx;
    ub.set(U::Model, obj.transform);
}
