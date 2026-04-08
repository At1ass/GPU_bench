#include "demo/passes/fur_pass.h"
#include "engine/pass_context.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>

void FurPass::init(const TierResourceView& res) {
    ub_.init(res.core.fur_shader);
}

void FurPass::execute(PassContext& ctx, FrameData& fd,
                      const TierResourceView& res,
                      const DemoTierConfig& cfg,
                      const SceneData& scene) {
    Renderer* r = ctx.renderer();
    if (!res.core.fur_shader || scene.model_mesh == MeshHandle() || res.core.fur_tex == INVALID_TEXTURE) return;

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::Model, scene.model_transform);
    ub_.set(U::LightDir, fd.sun_dir);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::FogColor, FOG_COLOR);
    ub_.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
    ub_.set(U::FurLength, cfg.fur_length);
    ub_.set(U::FurAoPower, 1.5f);
    ub_.set(U::Time, fd.time);

    // Breeze: slowly varying direction (shared formula in demo_utils.h)
    Vec3 wind = computeWindDir(fd.time);
    ub_.set(U::WindDir, wind.x, wind.y, wind.z);

    // Viewport size for vignette + color grading
    ub_.set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));

    // Shadow uniforms (T2+)
    if (fd.has_shadows) {
        // Texel size for PCF kernel spacing (all shadow tiers)
        float texel = 1.0f / static_cast<float>(cfg.shadow_map_size);
        ub_.set(U::ShadowTexelSize, texel, texel);
        if (cfg.enable_pcss) ub_.set(U::LightSize, cfg.light_size);
        ub_.set(U::LightVP, fd.light_vp);
        r->bindTextureUnit(3, res.shadow.depth_tex);
        ub_.set(U::ShadowMap, 3);
        ub_.set(U::HasShadow, 1.0f);
    } else {
        ub_.set(U::HasShadow, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res.core.fur_shader, fd, cfg.point_light_count);

    // Fur strand texture: unit 0
    r->bindTextureUnit(0, res.core.fur_tex);
    ub_.set(U::FurTex, 0);

    // Fur intensity mask: unit 1
    if (res.core.fur_mask_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(1, res.core.fur_mask_tex);
        ub_.set(U::FurMask, 1);
        ub_.set(U::HasFurMask, 1.0f);
    } else {
        ub_.set(U::HasFurMask, 0.0f);
    }

    float tex_scale = cfg.fur_density * 0.05f;
    ub_.set(U::FurTexScale, tex_scale);

    // PBR material: fur should be rough and non-metallic
    ub_.set(U::Metallic, 0.0f);
    ub_.set(U::Roughness, 0.85f);

    ub_.set(U::FurColorRoot, 0.30f, 0.18f, 0.08f);
    ub_.set(U::FurColorTip, 0.72f, 0.55f, 0.32f);

    r->setDepthTest(true);
    r->setDepthMask(false); // fur shells don't write depth (opaque pass already did)
    r->setBlending(true);   // alpha blend for soft fur edges
    r->setCullFace(false);  // fur visible from both sides

    // Shell 0 is the base mesh (already rendered in opaque pass) -- skip it.
    // Shells 1..N-1 are the fur layers, rendered inner to outer.
    int num_shells = cfg.fur_shells;

    // Check for instancing support (T2+ with GL3Features)
    GL3Features* g3 = r->features<GL3Features>();
    bool use_instancing = (cfg.tier >= DemoTier::Enhanced) && g3 && g3->hasInstancing();

    if (use_instancing) {
        ub_.set(U::UseInstancing, 1);
        ub_.set(U::FurShells, num_shells);
        g3->drawMeshInstanced(scene.model_mesh, num_shells - 1);
    } else {
        ub_.set(U::UseInstancing, 0);
        for (int i = 1; i < num_shells; i++) {
            float shell_index = static_cast<float>(i) / static_cast<float>(num_shells - 1);
            ub_.set(U::ShellIndex, shell_index);
            ctx.drawMesh(scene.model_mesh);
        }
    }

    r->setBlending(false);
    r->setCullFace(true);
    r->setDepthMask(true);
}
