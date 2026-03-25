#include "demo/passes/tessellated_model_pass.h"
#include "demo/demo_utils.h"
#include "demo/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "renderer/features.h"
#include "platform/logger.h"

void TessellatedModelPass::init(const TierResourceView& res) {
    ub_.init(res.t4.tess_shader);
}

void TessellatedModelPass::execute(Renderer* r, FrameData& fd,
                                   const TierResourceView& res,
                                   const DemoTierConfig& cfg,
                                   const SceneData& scene) {
    GL4Features* g4 = r->features<GL4Features>();
    if (!g4 || scene.model_mesh == MeshHandle()) return;

    ub_.use();
    ub_.set(U::Proj, fd.proj);
    ub_.set(U::View, fd.view);
    ub_.set(U::Model, scene.model_transform);
    ub_.set(U::LightDir, fd.sun_dir);
    ub_.set(U::CamPos, fd.cam_pos);
    ub_.set(U::FogColor, FOG_COLOR);
    ub_.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
    ub_.set(U::Time, fd.time);
    ub_.set(U::TessInner, static_cast<float>(cfg.tess_level));
    ub_.set(U::TessOuter, static_cast<float>(cfg.tess_level));
    ub_.set(U::DisplacementStr, cfg.displacement_strength);
    ub_.set(U::DisplacementFreq, 5.0f);  // organic low-freq for model
    ub_.set(U::Metallic, 0.0f);
    ub_.set(U::Roughness, 0.6f);
    ub_.set(U::MatColor, 0.55f, 0.35f, 0.20f);
    ub_.set(U::ProcTex, 0.0f);
    ub_.set(U::NormalStrength, 0.0f);

    // SSS uniforms
    if (cfg.enable_sss) {
        ub_.set(U::SssStrength, cfg.sss_strength);
    } else {
        ub_.set(U::SssStrength, 0.0f);
    }

    if (fd.has_shadows) {
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

    setPointLightUniforms(res.t4.tess_shader, fd, cfg.point_light_count);
    ub_.set(U::HasNormalMap, 0.0f);

    g4->setPatchVertices(3);
    r->setDepthTest(true);
    r->setCullFace(true);
    g4->drawMeshAsPatches(scene.model_mesh);

    // Still draw fur on top of tessellated model
    if (fur_pass_)
        fur_pass_->execute(r, fd, res, cfg, scene);

    // Render all tessellated scene objects (stones, terrain, etc.)
    // Re-activate tess shader after fur pass changed it
    if (scene.opaque_objects) {
        const std::vector<SceneObject>& objects = *scene.opaque_objects;
        bool has_tess_objects = false;
        for (size_t i = 0; i < objects.size(); i++) {
            if (objects[i].tessellated && !objects[i].is_water && objects[i].mesh != scene.model_mesh)
                { has_tess_objects = true; break; }
        }
        if (has_tess_objects) {
            // Restore tess shader state (fur pass switched to a different shader)
            ub_.use();
            ub_.set(U::Proj, fd.proj);
            ub_.set(U::View, fd.view);
            ub_.set(U::LightDir, fd.sun_dir);
            ub_.set(U::CamPos, fd.cam_pos);
            ub_.set(U::FogColor, FOG_COLOR);
            ub_.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
            ub_.set(U::Time, fd.time);
            ub_.set(U::TessInner, static_cast<float>(cfg.tess_level));
            ub_.set(U::TessOuter, static_cast<float>(cfg.tess_level));
            ub_.set(U::HasNormalMap, 0.0f);
            ub_.set(U::SssStrength, 0.0f);
            if (fd.has_shadows) {
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
            setPointLightUniforms(res.t4.tess_shader, fd, cfg.point_light_count);

            g4->setPatchVertices(3);
            r->setDepthTest(true);
            r->setCullFace(true);

            for (size_t i = 0; i < objects.size(); i++) {
                const SceneObject& obj = objects[i];
                if (!obj.tessellated) continue;
                if (obj.is_water) continue;
                if (obj.mesh == scene.model_mesh) continue;
                if (!sphereInFrustum(fd.frustum, obj.bounds_center, obj.bounds_radius))
                    continue;

                ub_.set(U::Model, obj.transform);
                ub_.set(U::MatColor, obj.color);
                ub_.set(U::MatSpec, obj.specular);
                ub_.set(U::ProcTex, obj.material == MaterialType::Island ? 1.0f : 0.0f);
                ub_.set(U::NormalStrength, 0.0f);
                // Fine stone grain: high freq, low amplitude
                ub_.set(U::DisplacementStr, 0.03f);
                ub_.set(U::DisplacementFreq, 25.0f);
                ub_.set(U::Metallic, obj.metallic >= 0.0f ? obj.metallic : 0.0f);
                ub_.set(U::Roughness, obj.roughness >= 0.0f ? obj.roughness : 0.6f);

                if (obj.two_sided) r->setCullFace(false);
                g4->drawMeshAsPatches(obj.mesh);
                if (obj.two_sided) r->setCullFace(true);
            }
        }
    }
}
