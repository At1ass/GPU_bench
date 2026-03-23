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

    Mat4 model_transform_;

    // Render passes (still inline methods — will be extracted to pass classes later)
    void renderShadowPass(Renderer* r, const FrameData& fd);
    void renderSky(Renderer* r, const FrameData& fd);
    void renderOpaquePass(Renderer* r, const FrameData& fd);
    void renderGrassInstanced(Renderer* r, const FrameData& fd);
    void renderFurPass(Renderer* r, const FrameData& fd);
    void renderParticlePass(Renderer* r, const FrameData& fd);

    // T2+ SSAO
    void renderSSAOPass(Renderer* r, const FrameData& fd);
    void renderSSAOBlur(Renderer* r, const FrameData& fd);

    // T4 render passes
    void renderComputeParticles(Renderer* r, const FrameData& fd);
    void renderTessellatedModel(Renderer* r, const FrameData& fd);
    void renderComputeParticlesDraw(Renderer* r, const FrameData& fd);
    void renderVolumetricFog(Renderer* r, const FrameData& fd);
    void renderHDRComposite(Renderer* r, const FrameData& fd);

    // T4 Ultra compute passes
    void renderGTAOPass(Renderer* r, const FrameData& fd);
    void renderGTAOBlur(Renderer* r, const FrameData& fd);
    void renderBloomCompute(Renderer* r, const FrameData& fd);
    void computeAutoExposure(Renderer* r, const FrameData& fd);
    void renderSSR(Renderer* r, const FrameData& fd);
    void renderWaterPass(Renderer* r, const FrameData& fd);
    void renderDoF(Renderer* r, const FrameData& fd);

    // Puddle placement
    void placePuddles(Renderer* r);

    // T2+ bloom post-processing
    void renderSceneToFBO(Renderer* r, const FrameData& fd);
    void renderBloomPasses(Renderer* r, const FrameData& fd);
    void renderComposite(Renderer* r, const FrameData& fd);

    // Cached uniform blocks (one per shader, zero string ops in hot path)
    UniformBlock ub_island_;
    UniformBlock ub_fur_;
    UniformBlock ub_grass_;
    UniformBlock ub_sky_;
    UniformBlock ub_particle_;
    UniformBlock ub_shadow_;
    UniformBlock ub_ssao_;
    UniformBlock ub_ssao_blur_;
    UniformBlock ub_bloom_extract_;
    UniformBlock ub_bloom_blur_;
    UniformBlock ub_bloom_composite_;
    UniformBlock ub_tess_;
    UniformBlock ub_compute_particle_;
    UniformBlock ub_particle_render_;
    UniformBlock ub_vol_fog_;
    UniformBlock ub_tone_map_;
    UniformBlock ub_gtao_;
    UniformBlock ub_gtao_blur_;
    UniformBlock ub_bloom_down_;
    UniformBlock ub_bloom_up_;
    UniformBlock ub_histogram_;
    UniformBlock ub_exposure_;
    UniformBlock ub_ssr_;
    UniformBlock ub_dof_;

    int viewport_w_, viewport_h_;
    bool initialized_;
    bool passes_logged_;
    float prev_exposure_;
    DemoDebugOverrides debug_;

    RenderTargetHandle dest_rt_;
};
