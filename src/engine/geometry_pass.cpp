#include "engine/geometry_pass.h"

void GeometryPass::execute(PassContext& ctx, FrameData& fd,
                           const TierResourceView& res,
                           const DemoTierConfig& cfg,
                           const SceneData& scene) {
    ctx.beginRT(output_rt_, out_w_, out_h_, has_clear_ ? clear_ : nullptr);
    ctx.applyState(state_);

    ub_.use();
    sceneSetup(ub_, ctx, fd, res, cfg);

    const std::vector<SceneObject>* objects = objectList(scene);
    if (!objects) { ctx.endRT(); return; }

    for (size_t i = 0; i < objects->size(); i++) {
        const SceneObject& obj = (*objects)[i];
        if (!objectFilter(obj, fd)) continue;

        perObject(ub_, ctx, obj);

        if (obj.mat.two_sided) ctx.setCullFace(false);
        ctx.renderer()->drawMesh(obj.mesh);
        if (obj.mat.two_sided) ctx.setCullFace(state_.cull_face);
    }

    ctx.endRT();
}
