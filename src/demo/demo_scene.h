#pragma once
#include "renderer/renderer.h"
#include "demo/demo_camera.h"
#include "demo/demo_debug.h"
#include "demo/demo_frame_data.h"
#include "demo/demo_pipeline.h"
#include "demo/material.h"
#include "demo/scene_data.h"
#include "demo/tier_resource_view.h"
#include "demo/uniform_block.h"
#include "demo/passes/sky_pass.h"
#include "demo/passes/shadow_pass.h"
#include "demo/passes/opaque_pass.h"
#include "demo/passes/grass_instanced_pass.h"
#include "demo/passes/fur_pass.h"
#include "demo/passes/particle_pass.h"
#include "demo/passes/ssao_pass.h"
#include "demo/passes/bloom_pass.h"
#include "demo/passes/composite_pass.h"
#include "demo/passes/scene_to_fbo_pass.h"
#include "demo/passes/hdr_composite_pass.h"
#include "demo/passes/compute_particles_pass.h"
#include "demo/passes/compute_particles_draw_pass.h"
#include "demo/passes/tessellated_model_pass.h"
#include "demo/passes/gtao_pass.h"
#include "demo/passes/bloom_compute_pass.h"
#include "demo/passes/auto_exposure_pass.h"
#include "demo/passes/volumetric_fog_pass.h"
#include "demo/passes/water_pass.h"
#include "demo/passes/ssr_pass.h"
#include "demo/passes/dof_pass.h"
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
    SceneData scene_data_;

    // Scene objects (owned, referenced by scene_data_)
    std::vector<SceneObject> opaque_objects_;
    std::vector<SceneObject> cloud_objects_;
    MeshHandle model_mesh_;

    // Scene building
    void buildScene(Renderer* r);
    void placeModel(Renderer* r);
    void placeGroundPlane(Renderer* r);
    void placeRocks(Renderer* r);
    void placeGrass(Renderer* r);
    void placePuddles(Renderer* r);

    Mat4 model_transform_;

    // Pass objects (owned by DemoScene)
    SkyPass sky_pass_;
    ShadowPass shadow_pass_;
    OpaquePass opaque_pass_;
    GrassInstancedPass grass_pass_;
    FurPass fur_pass_;
    ParticlePass particle_pass_;
    SSAOPass ssao_pass_;
    BloomPass bloom_pass_;
    CompositePass composite_pass_;
    SceneToFBOPass scene_to_fbo_pass_;
    HDRCompositePass hdr_composite_pass_;
    ComputeParticlesPass compute_particles_pass_;
    ComputeParticlesDrawPass compute_particles_draw_pass_;
    TessellatedModelPass tess_model_pass_;
    GTAOPass gtao_pass_;
    BloomComputePass bloom_compute_pass_;
    AutoExposurePass auto_exposure_pass_;
    VolumetricFogPass vol_fog_pass_;
    WaterPass water_pass_;
    SSRPass ssr_pass_;
    DoFPass dof_pass_;

    DemoPipeline pipeline_;

    int viewport_w_, viewport_h_;
    bool initialized_;
    bool passes_logged_;
    DemoDebugOverrides debug_;

    RenderTargetHandle dest_rt_;
};
