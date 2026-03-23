#include "demo/passes/shadow_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "platform/logger.h"

void ShadowPass::init(const TierResourceView& res) {
    ub_.init(res.shadow.shader);
}

void ShadowPass::execute(Renderer* r, FrameData& fd,
                         const TierResourceView& res,
                         const DemoTierConfig& cfg,
                         const SceneData& scene) {
    (void)cfg;

    if (!res.shadow.shader || res.shadow.rt == INVALID_RENDER_TARGET) return;

    computeLightMatrix(fd);

    r->bindRenderTarget(res.shadow.rt);
    r->setViewport(0, 0, res.shadow.map_size, res.shadow.map_size);
    r->clear(1.0f, 1.0f, 1.0f, 1.0f);
    r->setDepthTest(true);
    r->setDepthMask(true);
    r->setCullFace(true);

    ub_.use();
    ub_.set(U::LightVP, fd.light_vp);

    // Render all opaque objects into shadow map
    // (model base mesh is already in opaque_objects_, so fur also casts shadow)
    const std::vector<SceneObject>& objects = *scene.opaque_objects;
    for (size_t i = 0; i < objects.size(); i++) {
        const SceneObject& obj = objects[i];
        ub_.set(U::Model, obj.transform);
        r->drawMesh(obj.mesh);
    }

    r->bindRenderTarget(INVALID_RENDER_TARGET);
}
