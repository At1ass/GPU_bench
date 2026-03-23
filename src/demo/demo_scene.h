#pragma once
#include "renderer/renderer.h"
#include "demo/demo_camera.h"
#include "demo/material.h"
#include "demo/tier_resource_view.h"
#include <vector>

// Demo tier levels matching GL capability
enum class DemoTier {
    Basic    = 1,  // GL 2.1: forward Blinn-Phong, basic fog
    Enhanced = 2,  // GL 3.0+: shadow map, SSAO, bloom
    Quality  = 3,  // GL 3.3+: PCF, point lights, particles, DoF
    Ultra    = 4   // GL 4.3+: PBR, compute particles, tessellation, vol fog
};

// Information about active rendering techniques (for UI overlay)
struct TechniqueInfo {
    const char* shading;
    const char* shadows;
    const char* lighting;
    const char* ambient;
    const char* postfx;
    const char* particles;
    const char* extras;
    int object_count;
    int estimated_draw_calls;
};

// Configuration for a single demo tier
struct DemoTierConfig {
    DemoTier tier;

    // Fur
    int fur_shells;              // 16, 24, 48, 64
    float fur_length;            // world-space fur length
    float fur_density;           // strands per UV unit
    float fur_thickness;         // strand radius factor

    // Scene
    float fog_density;

    // Sky + normals
    bool enable_sky;
    bool enable_normal_maps;
    float normal_map_strength;

    // Scene enrichment
    int rock_count;
    int grass_count;
    int particle_count;
    bool enable_wind;

    // Instanced grass (T2+)
    int instanced_grass_count;  // T2+: number of instanced grass blades (0 = use batched)
    float grass_area_size;      // area to scatter grass

    // Shadow mapping (T2+)
    bool enable_shadows;
    int shadow_map_size;

    // Bloom post-processing (T2+)
    bool enable_bloom;
    float bloom_strength;

    // SSAO (T2+)
    bool enable_ssao;
    float ssao_radius;
    float ssao_intensity;

    // Point lights (T3+)
    int point_light_count;

    // Normal map texture (T3+)
    bool enable_normal_map_texture;

    // T4 features
    bool enable_pbr;
    bool enable_tessellation;
    int tess_level;
    bool enable_compute_particles;
    int compute_particle_count;
    bool enable_volumetric_fog;
    bool enable_hdr;

    // T4 Ultra enhancements
    bool enable_pcss;
    bool enable_gtao;
    bool enable_compute_bloom;
    bool enable_auto_exposure;
    bool enable_ssr;
    bool enable_dof;
    bool enable_sss;
    float sss_strength;
    float chromatic_strength;
    float grain_strength;
    float dof_focal_distance;
    float dof_strength;
    float light_size;            // PCSS light size
    int fog_steps;               // volumetric fog raymarch steps
    float displacement_strength; // tessellation displacement
};

DemoTierConfig getTierConfig(DemoTier tier);
TechniqueInfo getTierTechniqueInfo(DemoTier tier, int object_count);
int maxSupportedTier(const Renderer& r);

// A placed object in the scene
struct SceneObject {
    MeshHandle mesh;
    Mat4 transform;
    MaterialType material;
    Vec3 color;
    float specular;
    Vec3 bounds_center;   // world-space bounding sphere center
    float bounds_radius;  // world-space bounding sphere radius
    bool vertex_wind;     // enable wind vertex displacement (grass only)
    bool two_sided;       // disable backface culling for this object
    float metallic;       // PBR metallic override (-1 = use default)
    float roughness;      // PBR roughness override (-1 = use default)
    bool is_water;        // water material with ripples + Fresnel

    SceneObject() : mesh(), transform(), material(MaterialType::Model),
        color(0,0,0), specular(0), bounds_center(0,0,0), bounds_radius(0),
        vertex_wind(false), two_sided(false), metallic(-1.0f), roughness(-1.0f), is_water(false) {}
};

// Frustum culling planes
struct FrustumPlanes {
    float planes[6][4]; // A,B,C,D for each plane
};

// Demo scene: OBJ model with fur, orbiting camera.
class DemoScene {
public:
    DemoScene();
    ~DemoScene();

    DemoScene(const DemoScene&) = delete;
    DemoScene& operator=(const DemoScene&) = delete;
    DemoScene(DemoScene&&) = delete;
    DemoScene& operator=(DemoScene&&) = delete;

    bool setup(Renderer* r, DemoTier tier, int viewport_w, int viewport_h,
               const TierResourceView& resources);
    void renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h,
                     RenderTargetHandle dest_rt = INVALID_RENDER_TARGET);
    void cleanup(Renderer* r);

    TechniqueInfo getTechniqueInfo() const;

private:
    Renderer* r_;
    DemoTier tier_;
    DemoTierConfig config_;
    CameraPath camera_;
    TierResourceView res_;

    // Scene objects
    std::vector<SceneObject> opaque_objects_;
    std::vector<SceneObject> cloud_objects_;
    MeshHandle model_mesh_;        // fur target mesh (for shell rendering)

    // Scene building
    void buildScene(Renderer* r);
    void placeModel(Renderer* r);
    void placeGroundPlane(Renderer* r);
    void placeRocks(Renderer* r);
    void placeGrass(Renderer* r);

    // Model transform (set in placeModel, used in renderFurPass)
    Mat4 model_transform_;

    // Frame context
    struct FrameContext {
        Mat4 proj, view;
        Mat4 light_vp;
        Vec3 cam_pos, sun_dir;
        FrustumPlanes frustum;
        float time;
        int tier_int;
        bool has_shadows;
        bool has_bloom;
        bool has_ssao;
        bool has_pbr;
        bool has_tessellation;
        bool has_compute_particles;
        bool has_volumetric_fog;
        bool has_hdr;
    };

    // Render passes
    void computeLightMatrix(FrameContext& fc);
    void renderShadowPass(Renderer* r, const FrameContext& fc);
    void renderSky(Renderer* r, const FrameContext& fc);
    void renderOpaquePass(Renderer* r, const FrameContext& fc);
    void renderGrassInstanced(Renderer* r, const FrameContext& fc);
    void renderFurPass(Renderer* r, const FrameContext& fc);
    void renderParticlePass(Renderer* r, const FrameContext& fc);

    // T2+ SSAO
    void renderSSAOPass(Renderer* r, const FrameContext& fc);
    void renderSSAOBlur(Renderer* r, const FrameContext& fc);

    // Point light uniform helper
    void setPointLightUniforms(ShaderProgram* shader, const FrameContext& fc);

    // T4 render passes
    void renderComputeParticles(Renderer* r, const FrameContext& fc);
    void renderTessellatedModel(Renderer* r, const FrameContext& fc);
    void renderComputeParticlesDraw(Renderer* r, const FrameContext& fc);
    void renderVolumetricFog(Renderer* r, const FrameContext& fc);
    void renderHDRComposite(Renderer* r, const FrameContext& fc);

    // T4 Ultra compute passes
    void renderGTAOPass(Renderer* r, const FrameContext& fc);
    void renderGTAOBlur(Renderer* r, const FrameContext& fc);
    void renderBloomCompute(Renderer* r, const FrameContext& fc);
    void computeAutoExposure(Renderer* r, const FrameContext& fc);
    void renderSSR(Renderer* r, const FrameContext& fc);
    void renderWaterPass(Renderer* r, const FrameContext& fc);
    void renderDoF(Renderer* r, const FrameContext& fc);

    // Puddle placement
    void placePuddles(Renderer* r);

    // T2+ bloom post-processing
    void renderSceneToFBO(Renderer* r, const FrameContext& fc);
    void renderBloomPasses(Renderer* r, const FrameContext& fc);
    void renderComposite(Renderer* r, const FrameContext& fc);

    int viewport_w_, viewport_h_;
    bool initialized_;
    bool passes_logged_;       // log active passes only once per tier
    float prev_exposure_;

    // Destination render target: the RT that was active when renderFrame was called.
    // Post-processing composite passes render TO this RT instead of the default framebuffer.
    RenderTargetHandle dest_rt_;
};
