#include "demo/passes/grass_instanced_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>

void GrassInstancedPass::init(const TierResourceView& res) {
    ub_.init(res.grass.shader);
}

void GrassInstancedPass::execute(Renderer* r, FrameData& fd,
                                 const TierResourceView& res,
                                 const DemoTierConfig& cfg,
                                 const SceneData& scene) {
    (void)scene;

    if (!res.grass.shader || res.grass.blade_mesh == MeshHandle()) return;
    if (cfg.instanced_grass_count <= 0) return;

    GL3Features* g3 = r->features<GL3Features>();
    if (!g3 || !g3->hasInstancing()) return;

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::Model, Mat4());
    ub_.set(U::LightDir, fd.sun_dir);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::FogColor, FOG_COLOR);
    ub_.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
    ub_.set(U::Time, fd.time);
    ub_.set(U::GrassCount, cfg.instanced_grass_count);
    ub_.set(U::AreaSize, cfg.grass_area_size);

    // Wind (same as fur)
    if (cfg.enable_wind) {
        float wind_x = sinf(fd.time * 0.7f) * 1.8f;
        float wind_z = cosf(fd.time * 0.5f) * 1.2f;
        ub_.set(U::WindDir, wind_x, 0.0f, wind_z);
    } else {
        ub_.set(U::WindDir, 0.0f, 0.0f, 0.0f);
    }

    // PCSS uniforms (T4 Ultra)
    if (cfg.enable_pcss) {
        float texel = 1.0f / static_cast<float>(cfg.shadow_map_size);
        ub_.set(U::ShadowTexelSize, texel, texel);
        ub_.set(U::LightSize, cfg.light_size);
    }

    // Puddle exclusion zones (T4 Ultra) -- array uniforms, keep string-based
    if (cfg.enable_ssr) {
        ub_.set(U::PuddleCount, 3);
        res.grass.shader->set3f("u_puddle_pos[0]", 2.5f, 0.0f, 0.8f);
        res.grass.shader->set3f("u_puddle_pos[1]", -1.8f, 0.0f, 2.2f);
        res.grass.shader->set3f("u_puddle_pos[2]", 0.5f, 0.0f, -2.5f);
        res.grass.shader->set1f("u_puddle_radius[0]", 1.5f);
        res.grass.shader->set1f("u_puddle_radius[1]", 1.2f);
        res.grass.shader->set1f("u_puddle_radius[2]", 1.0f);
    } else {
        ub_.set(U::PuddleCount, 0);
    }

    // Shadow
    if (fd.has_shadows) {
        ub_.set(U::LightVP, fd.light_vp);
        r->bindTextureUnit(3, res.shadow.depth_tex);
        ub_.set(U::ShadowMap, 3);
        ub_.set(U::HasShadow, 1.0f);
    } else {
        ub_.set(U::HasShadow, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res.grass.shader, fd, cfg.point_light_count);

    r->setDepthTest(true);
    r->setDepthMask(true);
    r->setBlending(true);   // for tip alpha fade
    r->setCullFace(false);  // grass visible from both sides

    g3->drawMeshInstanced(res.grass.blade_mesh, cfg.instanced_grass_count);

    r->setBlending(false);
    r->setCullFace(true);
}
