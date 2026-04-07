#include "demo/passes/compute_particles_draw_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "renderer/gl_funcs.h"
#include "platform/logger.h"

void ComputeParticlesDrawPass::init(const TierResourceView& res) {
    ub_.init(res.t4.particle_render_shader);
}

void ComputeParticlesDrawPass::execute(PassContext& ctx, FrameData& fd,
                                       const TierResourceView& res,
                                       const DemoTierConfig& cfg,
                                       const SceneData& scene) {
    Renderer* r = ctx.renderer();
    (void)cfg;
    (void)scene;

    ComputeFeatures* cf = r->features<ComputeFeatures>();
    if (!cf || !res.t4.particle_render_shader) return;

    ub_.use();
    cf->bindSSBO(res.t4.particle_ssbo, 0);
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);

    // Camera right/up vectors from view matrix
    Vec3 cam_right(fd.view.m[0], fd.view.m[4], fd.view.m[8]);
    Vec3 cam_up(fd.view.m[1], fd.view.m[5], fd.view.m[9]);
    ub_.set(U::CamRight, cam_right);
    ub_.set(U::CamUp, cam_up);

    r->setDepthTest(true);
    r->setDepthMask(false);
    r->setBlendingAdditive(true);
    r->setCullFace(false);

    // Draw quads: 6 vertices per particle (2 triangles each, no strip artifacts)
    glDrawArrays(GL_TRIANGLES, 0, res.t4.compute_particle_count * 6);

    r->setDepthMask(true);
    r->setBlendingAdditive(false);
    r->setCullFace(true);
}
