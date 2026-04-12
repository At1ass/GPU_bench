#include "demo/passes/shadow_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"

void ShadowPass::init(const TierResourceView& res) {
    ub().init(res.shadow.shader);
    setShader(res.shadow.shader);
    setOutputRT(res.shadow.rt, res.shadow.map_size, res.shadow.map_size);
    setState(RenderState::shadow());
    setClearColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void ShadowPass::setup(const TierResourceView& res) {
    (void)res;
}

void ShadowPass::sceneSetup(UniformBlock& ub, PassContext& ctx,
                            const FrameData& fd,
                            const TierResourceView& res,
                            const DemoTierConfig& cfg) {
    (void)ctx;
    (void)res;
    (void)cfg;
    ub.set(U::LightVP, fd.light_vp);
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
