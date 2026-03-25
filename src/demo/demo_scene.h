#pragma once
#include "renderer/renderer.h"
#include "demo/demo_camera.h"
#include "demo/demo_debug.h"
#include "demo/demo_frame_data.h"
#include "demo/demo_pipeline.h"
#include "demo/pass_factory.h"
#include "demo/pipeline_builder.h"
#include "demo/resource_id.h"
#include "demo/scene_data.h"
#include "demo/tier_resource_view.h"
#include <memory>
#include <vector>

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
    int fur_shells;
    float fur_length;
    float fur_density;
    float fur_thickness;

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
    int instanced_grass_count;
    float grass_area_size;

    // Shadow mapping (T2+)
    bool enable_shadows;
    int shadow_map_size;

    // Bloom (T2+)
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
    float light_size;
    int fog_steps;
    float displacement_strength;
};

DemoTierConfig getTierConfig(DemoTier tier);
TechniqueInfo getTierTechniqueInfo(DemoTier tier, int object_count);
int maxSupportedTier(const Renderer& r);

// Demo scene: OBJ model with fur, orbiting camera.
// Render passes are created by pass_factory and accessed through
// DemoRenderPass* base pointers — no concrete pass types visible here.
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
    void placePedestal(Renderer* r);
    void placeColumns(Renderer* r);
    void placeArch(Renderer* r);
    void placeRuins(Renderer* r);
    void placeTrees(Renderer* r);
    void placePond(Renderer* r);

    Mat4 model_transform_;

    // Render passes (owned, accessed only through pipeline)
    std::vector<std::unique_ptr<DemoRenderPass>> passes_;
    DemoPipeline pipeline_;

    int viewport_w_, viewport_h_;
    bool initialized_;
    bool passes_logged_;
    DemoDebugOverrides debug_;

    RenderTargetHandle dest_rt_;

    FrameData buildFrameData(float t, float time, int w, int h, RenderTargetHandle dest_rt);
};
