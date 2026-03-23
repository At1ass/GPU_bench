#include "demo/passes/water_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

void WaterPass::init(const TierResourceView& res) {
    ub_.init(res.core.island_shader);
}

void WaterPass::execute(Renderer* r, FrameData& fd,
                        const TierResourceView& res,
                        const DemoTierConfig& cfg,
                        const SceneData& scene) {
    if (!res.core.island_shader) return;

    // Count water objects
    const std::vector<SceneObject>& objects = *scene.opaque_objects;
    bool has_water = false;
    for (size_t i = 0; i < objects.size(); i++) {
        if (objects[i].is_water) { has_water = true; break; }
    }
    if (!has_water) return;

    ub_.use();

    // Set all standard uniforms (same as renderOpaquePass)
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::LightDir, fd.sun_dir);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::FogColor, FOG_COLOR);
    ub_.set(U::FogDensity, cfg.fog_density);
    ub_.set(U::Time, fd.time);
    ub_.set(U::NormalStrength, 0.0f);

    if (cfg.enable_pbr) {
        ub_.set(U::Metallic, 0.0f);
        ub_.set(U::Roughness, 0.02f);
    }
    if (cfg.enable_pcss) {
        float texel = 1.0f / static_cast<float>(cfg.shadow_map_size);
        ub_.set(U::ShadowTexelSize, texel, texel);
        ub_.set(U::LightSize, cfg.light_size);
    }
    ub_.set(U::SssStrength, 0.0f);

    if (fd.has_shadows) {
        ub_.set(U::LightVP, fd.light_vp);
        r->bindTextureUnit(3, res.shadow.depth_tex);
        ub_.set(U::ShadowMap, 3);
        ub_.set(U::HasShadow, 1.0f);
    } else {
        ub_.set(U::HasShadow, 0.0f);
    }
    ub_.set(U::HasNormalMap, 0.0f);
    ub_.set(U::PointLightCount, 0);

    // Bind scene copy for water reflections (ssr_tex has the scene before water)
    bool has_reflection = (res.t4.ssr_tex != INVALID_TEXTURE);
    if (has_reflection) {
        r->bindTextureUnit(5, res.t4.ssr_tex);
        ub_.set(U::ReflectionTex, 5);
        r->bindTextureUnit(6, res.t4.hdr_depth_tex);
        ub_.set(U::DepthTex, 6);
        ub_.set(U::HasReflection, 1.0f);
        ub_.set(U::ScreenSize,
            static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));

        ub_.set(U::Near, kDemoNear);
        ub_.set(U::Far, kDemoFar);
    } else {
        ub_.set(U::HasReflection, 0.0f);
    }

    // Render only water objects
    for (size_t i = 0; i < objects.size(); i++) {
        const SceneObject& obj = objects[i];
        if (!obj.is_water) continue;
        if (!sphereInFrustum(fd.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        ub_.set(U::Model, obj.transform);
        ub_.set(U::MatColor, obj.color);
        ub_.set(U::MatSpec, obj.specular);
        ub_.set(U::Alpha, 1.0f);
        ub_.set(U::ProcTex, 0.0f);
        ub_.set(U::VertexWind, 0.0f);
        ub_.set(U::IsWater, 1.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);
    }
}
