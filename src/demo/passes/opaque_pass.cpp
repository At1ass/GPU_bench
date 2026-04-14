#include "demo/passes/opaque_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier/tier_resource_view.h"
#include "demo/scene/demo_scene.h"
#include "demo/demo_debug.h"
#include "demo/demo_utils.h"
#include <cmath>

void OpaquePass::init(const TierResourceView& res, const DemoTierConfig& cfg,
                      const DemoDebugOverrides& dbg) {
    (void)dbg;
    res_ = &res;
    cfg_ = &cfg;
    ShaderProgram* s = res.shader(ShaderBank::Island);
    island_shader_ = s;
    ub().init(s);
    setShader(s);
    setState(RenderState::opaque());
    setPipelineManagedRT();
}

void OpaquePass::onSceneSetup(UniformBlock& ub, PassContext& ctx,
                               const FrameData& fd) {
    const TierResourceView& res = *res_;
    const DemoTierConfig& cfg = *cfg_;

    ctx.renderer()->setBlending(false);

    ub.set(U::Proj, fd.proj);
    ub.set(U::View, fd.view);
    ub.set(U::LightDir, fd.sun_dir);
    ub.set(U::CamPos, fd.cam_pos);
    ub.set(U::FogColor, FOG_COLOR);
    // Reduce per-object fog when volumetric fog handles atmospheric scattering;
    // keep minimal density for terrain edge fade (smoothstep in shader)
    ub.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
    ub.set(U::Time, fd.time);
    ub.set(U::NormalStrength, cfg.enable_normal_maps ? cfg.normal_map_strength : 0.0f);

    // Wind direction (shared formula in demo_utils.h)
    if (cfg.enable_wind) {
        Vec3 wind = computeWindDir(fd.time);
        ub.set(U::WindDir, wind.x, wind.y, wind.z);
    } else {
        ub.set(U::WindDir, 0.0f, 0.0f, 0.0f);
    }

    // Viewport size for vignette + color grading
    ub.set(U::ViewportSize,
        static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));

    // PBR default uniforms (T4) -- overridden per-object if set
    if (cfg.enable_pbr) {
        ub.set(U::Metallic, 0.0f);
        ub.set(U::Roughness, 0.6f);
    }

    // SSS uniforms (T4 Ultra)
    if (cfg.enable_sss) {
        ub.set(U::SssStrength, cfg.sss_strength);
    } else {
        ub.set(U::SssStrength, 0.0f);
    }

    // Shadow uniforms (T2+)
    if (fd.has_shadows) {
        ub.set(U::LightVP, fd.light_vp);
        ctx.bindTexture(3, res.shadow.depth_tex);
        ub.set(U::ShadowMap, 3);
        ub.set(U::HasShadow, 1.0f);
        // Texel size for PCF kernel spacing (needed by ALL shadow tiers, not just PCSS)
        float texel = 1.0f / static_cast<float>(cfg.shadow_map_size);
        ub.set(U::ShadowTexelSize, texel, texel);
        // PCSS-specific uniforms (T4 Ultra)
        if (cfg.enable_pcss) {
            ub.set(U::LightSize, cfg.light_size);
        }
    } else {
        ub.set(U::HasShadow, 0.0f);
    }

    // Normal map texture (T3+)
    if (res.normal_map_tex != INVALID_TEXTURE) {
        ctx.bindTexture(4, res.normal_map_tex);
        ub.set(U::NormalMap, 4);
        ub.set(U::HasNormalMap, 1.0f);
    } else {
        ub.set(U::HasNormalMap, 0.0f);
    }

    // Point lights (T3+)
    setPointLightUniforms(ub, fd, cfg.point_light_count);
}

bool OpaquePass::objectFilter(const SceneObject& obj,
                              const FrameData& fd) {
    if (obj.is_water) return false;
    if (obj.tessellated) return false;
    return sphereInFrustum(fd.frustum, obj.bounds_center, obj.bounds_radius);
}

void OpaquePass::perObject(UniformBlock& ub, PassContext& ctx,
                           const SceneObject& obj) {
    (void)ctx;
    const MaterialDef& m = obj.mat;
    ub.set(U::Model, obj.transform);
    ub.set(U::MatColor, m.color_a);
    ub.set(U::MatColorB, m.color_b);
    ub.set(U::MatSpec, m.specular);
    ub.set(U::Alpha, 1.0f);
    ub.set(U::MaterialId, static_cast<int>(m.proc_type));
    ub.set(U::NoiseScale, m.noise_scale);
    ub.set(U::NoiseIntensity, m.noise_intensity);
    ub.set(U::WarpStrength, m.warp_strength);
    ub.set(U::DetailScale, m.detail_scale);
    ub.set(U::VertexWind, obj.vertex_wind ? 1.0f : 0.0f);
    ub.set(U::Metallic, m.metallic);
    ub.set(U::Roughness, m.roughness);
    ub.set(U::IsWater, 0.0f);
}
