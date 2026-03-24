#include "demo/passes/opaque_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "platform/logger.h"
#include <cmath>

void OpaquePass::init(const TierResourceView& res) {
    ub_.init(res.core.island_shader);
}

void OpaquePass::execute(Renderer* r, FrameData& fd,
                         const TierResourceView& res,
                         const DemoTierConfig& cfg,
                         const SceneData& scene) {
    if (!res.core.island_shader) return;

    r->setBlending(false);
    ub_.use();

    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::LightDir, fd.sun_dir);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::FogColor, FOG_COLOR);
    // Reduce per-object fog when volumetric fog handles atmospheric scattering;
    // keep minimal density for terrain edge fade (smoothstep in shader)
    ub_.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
    ub_.set(U::Time, fd.time);
    ub_.set(U::NormalStrength, cfg.enable_normal_maps ? cfg.normal_map_strength : 0.0f);

    // Wind direction (same as fur pass for consistency)
    if (cfg.enable_wind) {
        float wind_x = sinf(fd.time * 0.7f) * 1.8f;
        float wind_z = cosf(fd.time * 0.5f) * 1.2f;
        ub_.set(U::WindDir, wind_x, 0.0f, wind_z);
    } else {
        ub_.set(U::WindDir, 0.0f, 0.0f, 0.0f);
    }

    // Viewport size for vignette + color grading
    ub_.set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));

    // PBR default uniforms (T4) -- overridden per-object if set
    float default_metallic = 0.0f;
    float default_roughness = 0.6f;
    if (cfg.enable_pbr) {
        ub_.set(U::Metallic, default_metallic);
        ub_.set(U::Roughness, default_roughness);
    }

    // PCSS / SSS uniforms (T4 Ultra)
    if (cfg.enable_pcss) {
        float texel = 1.0f / static_cast<float>(cfg.shadow_map_size);
        ub_.set(U::ShadowTexelSize, texel, texel);
        ub_.set(U::LightSize, cfg.light_size);
    }
    if (cfg.enable_sss) {
        ub_.set(U::SssStrength, cfg.sss_strength);
    } else {
        ub_.set(U::SssStrength, 0.0f);
    }

    // Shadow uniforms (T2+)
    if (fd.has_shadows) {
        ub_.set(U::LightVP, fd.light_vp);
        r->bindTextureUnit(3, res.shadow.depth_tex);
        ub_.set(U::ShadowMap, 3);
        ub_.set(U::HasShadow, 1.0f);
    } else {
        ub_.set(U::HasShadow, 0.0f);
    }

    // Normal map texture (T3+)
    if (res.normal_map_tex != INVALID_TEXTURE) {
        r->bindTextureUnit(4, res.normal_map_tex);
        ub_.set(U::NormalMap, 4);
        ub_.set(U::HasNormalMap, 1.0f);
    } else {
        ub_.set(U::HasNormalMap, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(res.core.island_shader, fd, cfg.point_light_count);

    const std::vector<SceneObject>& objects = *scene.opaque_objects;
    for (size_t i = 0; i < objects.size(); i++) {
        const SceneObject& obj = objects[i];
        // Skip water -- rendered separately in renderWaterPass after scene copy
        if (obj.is_water) continue;
        if (!sphereInFrustum(fd.frustum, obj.bounds_center, obj.bounds_radius))
            continue;

        ub_.set(U::Model, obj.transform);
        ub_.set(U::MatColor, obj.color);
        ub_.set(U::MatSpec, obj.specular);
        ub_.set(U::Alpha, 1.0f);
        ub_.set(U::ProcTex, obj.material == MaterialType::Island ? 1.0f : 0.0f);
        ub_.set(U::VertexWind, obj.vertex_wind ? 1.0f : 0.0f);

        // Per-object PBR overrides
        if (cfg.enable_pbr && obj.metallic >= 0.0f) {
            ub_.set(U::Metallic, obj.metallic);
            ub_.set(U::Roughness, obj.roughness);
        }

        // Water flag
        ub_.set(U::IsWater, 0.0f);

        if (obj.two_sided) r->setCullFace(false);
        r->drawMesh(obj.mesh);
        if (obj.two_sided) r->setCullFace(true);

        // Restore defaults after per-object override
        if (cfg.enable_pbr && obj.metallic >= 0.0f) {
            ub_.set(U::Metallic, default_metallic);
            ub_.set(U::Roughness, default_roughness);
        }
    }
}
