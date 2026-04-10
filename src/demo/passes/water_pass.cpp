#include "demo/passes/water_pass.h"
#include "engine/uniform_id.h"
#include "demo/tier_resource_view.h"
#include "demo/demo_scene.h"
#include "demo/demo_utils.h"

void WaterPass::init(const TierResourceView& res) {
    island_shader_ = res.core.island_shader;
    ub().init(res.core.island_shader);
    setShader(res.core.island_shader);
    // Water needs alpha blending for shore transparency, no depth write
    setState(RenderState::transparent());
    setPipelineManagedRT();
}

void WaterPass::setup(const TierResourceView& res) {
    (void)res;
}

void WaterPass::sceneSetup(UniformBlock& ub, PassContext& ctx,
                           const FrameData& fd,
                           const TierResourceView& res,
                           const DemoTierConfig& cfg) {
    // Set all standard uniforms (same as OpaquePass)
    ub.set(U::Proj, fd.proj);
    ub.set(U::View, fd.view);
    ub.set(U::LightDir, fd.sun_dir);
    ub.set(U::CamPos, fd.cam_pos);
    ub.set(U::FogColor, FOG_COLOR);
    ub.set(U::FogDensity, cfg.enable_volumetric_fog ? cfg.fog_density * 0.3f : cfg.fog_density);
    ub.set(U::Time, fd.time);
    ub.set(U::NormalStrength, 0.0f);

    if (cfg.enable_pbr) {
        ub.set(U::Metallic, 0.0f);
        ub.set(U::Roughness, 0.02f);
    }
    ub.set(U::SssStrength, 0.0f);

    if (fd.has_shadows) {
        float texel = 1.0f / static_cast<float>(cfg.shadow_map_size);
        ub.set(U::ShadowTexelSize, texel, texel);
        if (cfg.enable_pcss) ub.set(U::LightSize, cfg.light_size);
        ub.set(U::LightVP, fd.light_vp);
        ctx.bindTexture(3, res.shadow.depth_tex);
        ub.set(U::ShadowMap, 3);
        ub.set(U::HasShadow, 1.0f);
    } else {
        ub.set(U::HasShadow, 0.0f);
    }
    ub.set(U::HasNormalMap, 0.0f);
    ub.set(U::PointLightCount, 0);

    // Bind SSR snapshot textures (copied before water pass to avoid read-write hazard).
    // These are format-matched copies: RGBA16F for color, DEPTH_COMPONENT24 for depth.
    bool has_reflection = (res.t4.ssr.color_snapshot != INVALID_TEXTURE &&
                           res.t4.ssr.depth_snapshot != INVALID_TEXTURE);
    if (has_reflection) {
        ctx.bindTexture(5, res.t4.ssr.color_snapshot);
        ub.set(U::ReflectionTex, 5);
        ctx.bindTexture(6, res.t4.ssr.depth_snapshot);
        ub.set(U::DepthTex, 6);
        ub.set(U::HasReflection, 1.0f);
        ub.set(U::ScreenSize,
            static_cast<float>(fd.viewport_w), static_cast<float>(fd.viewport_h));
        ub.set(U::Near, kDemoNear);
        ub.set(U::Far, kDemoFar);
    } else {
        ub.set(U::HasReflection, 0.0f);
    }
}

bool WaterPass::objectFilter(const SceneObject& obj,
                             const FrameData& fd) {
    if (!obj.is_water) return false;
    return sphereInFrustum(fd.frustum, obj.bounds_center, obj.bounds_radius);
}

void WaterPass::perObject(UniformBlock& ub, PassContext& ctx,
                          const SceneObject& obj) {
    (void)ctx;
    ub.set(U::Model, obj.transform);
    ub.set(U::MatColor, obj.mat.color_a);
    ub.set(U::MatSpec, obj.mat.specular);
    ub.set(U::Alpha, 1.0f);
    ub.set(U::MaterialId, 0);  // water has its own code path
    ub.set(U::VertexWind, 0.0f);
    ub.set(U::IsWater, 1.0f);
}
