#include "engine/geometry_pass.h"
#include "geometry/math_types.h"
#include <cmath>

void GeometryPass::execute(PassContext& ctx, FrameData& fd,
                           const TierResourceView& res,
                           const DemoTierConfig& cfg,
                           const SceneData& scene) {
    if (!pipeline_managed_rt_)
        ctx.beginRT(output_rt_, out_w_, out_h_, has_clear_ ? clear_ : nullptr);
    ctx.applyState(state_);

    ub_.use();
    sceneSetup(ub_, ctx, fd, res, cfg);

    const std::vector<SceneObject>* objects = objectList(scene);
    if (!objects) {
        if (!pipeline_managed_rt_) ctx.endRT();
        return;
    }

    if (useSortedDraw()) {
        // Sorted path: build DrawList, sort by shader/material/depth, then draw
        DrawList dl;
        for (size_t i = 0; i < objects->size(); i++) {
            const SceneObject& obj = (*objects)[i];
            if (!objectFilter(obj, fd)) continue;
            // Normalized depth: distance from camera / far plane
            float dx = obj.bounds_center.x - fd.cam_pos.x;
            float dy = obj.bounds_center.y - fd.cam_pos.y;
            float dz = obj.bounds_center.z - fd.cam_pos.z;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            float depth = dist / 50.0f; // normalize by far plane (~50)
            dl.push(obj, shaderIdFor(obj), materialIdFor(obj), depth);
        }
        dl.sort();
        for (size_t i = 0; i < dl.size(); i++) {
            const SceneObject& obj = *dl[i].obj;
            perObject(ub_, ctx, obj);
            if (obj.mat.two_sided) ctx.setCullFace(false);
            ctx.renderer()->drawMesh(obj.mesh);
            if (obj.mat.two_sided) ctx.setCullFace(state_.cull_face);
        }
    } else {
        // Unsorted path: iterate in construction order (default)
        for (size_t i = 0; i < objects->size(); i++) {
            const SceneObject& obj = (*objects)[i];
            if (!objectFilter(obj, fd)) continue;

            perObject(ub_, ctx, obj);

            if (obj.mat.two_sided) ctx.setCullFace(false);
            ctx.renderer()->drawMesh(obj.mesh);
            if (obj.mat.two_sided) ctx.setCullFace(state_.cull_face);
        }
    }

    if (!pipeline_managed_rt_)
        ctx.endRT();
}
