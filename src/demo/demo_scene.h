#pragma once
#include "renderer/renderer.h"
#include "renderer/scoped_handle.h"
#include "demo/demo_camera.h"
#include <vector>

// Demo tier levels matching GL capability
enum class DemoTier {
    Basic    = 1,  // GL 2.1: forward Blinn-Phong, no shadows
    Enhanced = 2,  // GL 3.0+: shadow map, bloom
    Quality  = 3,  // GL 3.3+: PCF shadows, SSS, multi-pass bloom
    Ultra    = 4   // GL 4.3+: PBR, god rays, ACES
};

// Configuration for a single tier
struct DemoTierConfig {
    DemoTier tier;
    int terrain_resolution;     // vertices per side (48..128)
    int shadow_map_size;        // 0 = no shadows
    bool enable_bloom;
    int bloom_passes;           // 0 = no bloom
    bool use_pcf;               // T3+: 16-tap Poisson PCF
    bool use_pbr;               // T4: Cook-Torrance
    bool use_god_rays;          // T4: radial god rays
};

DemoTierConfig getTierConfig(DemoTier tier);
int maxSupportedTier(const Renderer& r);

// Material types for city objects
enum class MaterialType {
    Terrain,    // dried grass / dirt
    Building,   // concrete / brick
    Foliage,    // tree canopy
    Wood,       // trunks, fences
    Stone,      // rocks, arches
    Metal,      // street lamps
    Water       // puddle
};

// A placed object in the scene
struct SceneObject {
    MeshHandle mesh;
    Mat4 transform;
    MaterialType material;
    Vec3 color;
    float specular;
};

// Procedural city scene — abandoned outpost with buildings, trees, rocks.
// Camera follows a Catmull-Rom spline path through the town.
class DemoScene {
public:
    DemoScene();
    ~DemoScene();

    // Non-copyable, non-movable (owns GPU resources via Renderer*)
    DemoScene(const DemoScene&) = delete;
    DemoScene& operator=(const DemoScene&) = delete;
    DemoScene(DemoScene&&) = delete;
    DemoScene& operator=(DemoScene&&) = delete;

    bool setup(Renderer* r, DemoTier tier, int viewport_w, int viewport_h);
    void renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h);
    void cleanup(Renderer* r);

private:
    Renderer* r_;  // stored during setup for RAII cleanup
    DemoTier tier_;
    DemoTierConfig config_;
    CameraPath camera_;

    // Scene objects (terrain, buildings, trees, rocks, lamps, fences, arches, water)
    // Raw handles — cleaned up in cleanup()/destructor by iterating
    std::vector<SceneObject> objects_;
    int water_index_;           // index of water quad (-1 if none)
    ScopedMesh screen_quad_;

    // Shaders
    ScopedShader city_shader_;
    ScopedShader shadow_shader_;
    ScopedShader bloom_blur_shader_;
    ScopedShader bloom_composite_shader_;

    // City shader uniforms
    int u_proj_, u_view_, u_model_;
    int u_light_dir_, u_cam_pos_;
    int u_fog_color_, u_fog_density_;
    int u_time_;
    int u_mat_color_, u_mat_spec_, u_alpha_;
    int u_light_vp_, u_shadow_map_;
    int u_roughness_, u_metallic_;

    // Shadow shader uniforms
    int u_shd_lvp_, u_shd_model_;

    // Bloom uniforms
    int u_bl_tex_, u_bl_dir_, u_bl_threshold_;
    int u_bc_scene_, u_bc_bloom_, u_bc_intensity_;
    int u_bc_sun_pos_, u_bc_ray_density_, u_bc_ray_decay_;
    int u_bc_time_;

    // Render targets
    ScopedRenderTarget shadow_rt_;
    TextureHandle shadow_tex_;          // alias into shadow_rt_ depth, NOT owned
    ScopedRenderTarget scene_rt_;
    ScopedRenderTarget bloom_rt_a_, bloom_rt_b_;

    void buildCity(Renderer* r);
    void renderShadowPass(Renderer* r, const Mat4& light_vp);
    void renderBloom(Renderer* r, float time);
    Mat4 computeLightVP() const;

    int viewport_w_, viewport_h_;
    bool initialized_;
};
