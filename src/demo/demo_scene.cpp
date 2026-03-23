#include "demo/demo_scene.h"
#include "demo/demo_utils.h"
#include "demo/tier_config_validate.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cmath>
#include <cstring>
#include <cstdio>

// ============================================================
// DemoScene implementation
// ============================================================

DemoScene::DemoScene()
    : r_(nullptr)
    , tier_(DemoTier::Basic)
    , viewport_w_(0), viewport_h_(0)
    , initialized_(false)
    , passes_logged_(false)
    , dest_rt_(INVALID_RENDER_TARGET)
{
}

DemoScene::~DemoScene() {
}

// ============================================================
// buildScene: single model + ground plane
// ============================================================

void DemoScene::buildScene(Renderer* r) {
    opaque_objects_.clear();
    cloud_objects_.clear();
    model_mesh_ = MeshHandle();

    placeModel(r);
    placeGroundPlane(r);
    placeRocks(r);
    placeGrass(r);
    placePuddles(r);

    int total = static_cast<int>(opaque_objects_.size());
    LOG_INF("Demo scene: %d objects, fur shells=%d, particles=%d",
              total, config_.fur_shells, config_.particle_count);
}

// ============================================================
// placeModel: use pre-loaded model from resources
// ============================================================

void DemoScene::placeModel(Renderer* r) {
    (void)r;

    model_mesh_ = res_.core.model_mesh;

    model_transform_ = Mat4();

    SceneObject obj;
    obj.mesh = res_.core.model_mesh;
    obj.transform = model_transform_;
    obj.material = MaterialType::Model;
    obj.color = Vec3(0.55f, 0.38f, 0.32f);  // pinkish skin visible under fur
    obj.specular = 0.08f;
    setBounds(obj, res_.core.model_bounding_radius);
    opaque_objects_.push_back(obj);
}

// ============================================================
// placeGroundPlane: use pre-loaded ground mesh from resources
// ============================================================

void DemoScene::placeGroundPlane(Renderer* r) {
    (void)r;

    SceneObject ground;
    ground.mesh = res_.core.ground_mesh;
    ground.transform = Mat4::translate(0.0f, -1.0f, 0.0f);
    ground.material = MaterialType::Island;
    ground.color = Vec3(0.45f, 0.42f, 0.38f);
    ground.specular = 0.05f;
    setBounds(ground, 8.5f);
    opaque_objects_.push_back(ground);
}

// ============================================================
// placeRocks: scattered rocks (batched mesh, single draw call)
// ============================================================

void DemoScene::placeRocks(Renderer* r) {
    (void)r;
    if (res_.core.rock_mesh == MeshHandle()) return;

    SceneObject rocks;
    rocks.mesh = res_.core.rock_mesh;
    rocks.transform = Mat4();
    rocks.material = MaterialType::Model;  // flat color, no procedural terrain
    rocks.color = Vec3(0.45f, 0.42f, 0.38f);
    rocks.specular = 0.08f;
    rocks.vertex_wind = false;
    rocks.two_sided = true;
    setBounds(rocks, 6.0f);
    opaque_objects_.push_back(rocks);
}

// ============================================================
// placeGrass: scattered grass tufts (batched mesh, single draw call)
// ============================================================

void DemoScene::placeGrass(Renderer* r) {
    (void)r;
    // T2+ uses instanced grass rendering instead
    if (config_.instanced_grass_count > 0) return;

    if (res_.core.grass_mesh == MeshHandle()) return;

    SceneObject grass;
    grass.mesh = res_.core.grass_mesh;
    grass.transform = Mat4();
    grass.material = MaterialType::Model;  // flat color for grass
    grass.color = Vec3(0.30f, 0.50f, 0.20f);
    grass.specular = 0.02f;
    grass.vertex_wind = true;
    grass.two_sided = true;  // grass sways with wind
    setBounds(grass, 6.0f);
    opaque_objects_.push_back(grass);
}

// ============================================================
// placePuddles: reflective water discs around bunny (T4 Ultra)
// ============================================================

void DemoScene::placePuddles(Renderer* r) {
    (void)r;
    if (res_.t4.puddle_meshes[0] == MeshHandle()) return;
    if (!config_.enable_ssr) return;

    // Place 3 puddles around the bunny (larger, slightly above ground)
    static const float positions[][3] = {
        { 2.5f, -0.95f, 0.8f },
        { -1.8f, -0.95f, 2.2f },
        { 0.5f, -0.95f, -2.5f },
    };
    static const float scales[] = { 1.5f, 1.2f, 1.0f };

    for (int i = 0; i < 3; i++) {
        SceneObject puddle;
        puddle.mesh = res_.t4.puddle_meshes[i];
        // Scale + translate
        Mat4 s = Mat4::scale(scales[i], 1.0f, scales[i]);
        Mat4 t = Mat4::translate(positions[i][0], positions[i][1], positions[i][2]);
        puddle.transform = t * s;
        puddle.material = MaterialType::Model;
        puddle.color = Vec3(0.08f, 0.10f, 0.14f); // dark water
        puddle.specular = 0.95f;
        puddle.vertex_wind = false;
        puddle.two_sided = true;
        puddle.metallic = 0.0f;
        puddle.roughness = 0.02f;
        puddle.is_water = true;
        setBounds(puddle, 0.8f * scales[i]);
        opaque_objects_.push_back(puddle);
    }
}

// ============================================================
// setup: store resources, build scene geometry
// ============================================================

bool DemoScene::setup(Renderer* r, DemoTier tier, int viewport_w, int viewport_h,
                      const TierResourceView& resources) {
    r_ = r;
    tier_ = tier;
    config_ = getTierConfig(tier);
    validateTierConfig(config_);
    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;
    res_ = resources;

    int t = static_cast<int>(tier);
    LOG_INF("Demo scene setup: tier %d, viewport %dx%d", t, viewport_w, viewport_h);

    // Build scene objects (cheap -- just fills SceneObject structs)
    buildScene(r);

    // Populate scene data for render passes
    scene_data_.opaque_objects = &opaque_objects_;
    scene_data_.cloud_objects = &cloud_objects_;
    scene_data_.model_mesh = model_mesh_;
    scene_data_.model_transform = model_transform_;

    // Create all render passes via factory (DemoScene only sees DemoRenderPass*)
    pass_ = createPasses(passes_, res_, config_);

    // Build render pipeline for this tier
    pipeline_.clear();

    bool is_t4_hdr = config_.enable_hdr && res_.t4.hdr_scene_rt != INVALID_RENDER_TARGET && !debug_.skip_hdr;
    bool is_t2t3_bloom = config_.enable_bloom && !is_t4_hdr;

    if (is_t4_hdr) {
        // T4 Ultra HDR pipeline — full scene + post-processing in one pipeline

        // Pre-scene passes
        if (config_.enable_shadows)
            pipeline_.addPass(pass_.shadow);
        if (config_.enable_compute_particles)
            pipeline_.addPass(pass_.compute_particles);

        // Scene to HDR FBO
        pipeline_.addPassWithRT(pass_.sky, res_.t4.hdr_scene_rt,
                                true, FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f,
                                viewport_w_, viewport_h_);
        pipeline_.addPass(pass_.opaque);
        pipeline_.addPass(pass_.grass);
        pipeline_.addPass(pass_.tess_model, config_.enable_tessellation);
        pipeline_.addPass(pass_.fur, !config_.enable_tessellation);
        pipeline_.addPass(pass_.compute_particles_draw, config_.enable_compute_particles);
        pipeline_.addPass(pass_.particle, !config_.enable_compute_particles);

        // SSR copy + water
        pipeline_.addCommand(&DemoScene::ssrCopyCommand,
                             config_.enable_ssr && res_.t4.ssr_tex != INVALID_TEXTURE);
        pipeline_.addPass(pass_.water);

        // Unbind scene FBO
        pipeline_.addRTSwitch(INVALID_RENDER_TARGET);

        // Post-processing
        pipeline_.addPass(pass_.auto_exposure,
                          config_.enable_auto_exposure && !debug_.skip_auto_exposure);

        bool use_gtao = !debug_.skip_gtao && config_.enable_gtao
                        && res_.t4.gtao_shader != nullptr && res_.t4.gtao_tex != INVALID_TEXTURE;
        pipeline_.addPass(pass_.gtao, use_gtao);
        pipeline_.addPass(pass_.ssao, config_.enable_ssao && !use_gtao);

        pipeline_.addPass(pass_.vol_fog,
                          config_.enable_volumetric_fog && !debug_.skip_vol_fog);

        bool use_compute_bloom = !debug_.skip_compute_bloom && config_.enable_compute_bloom
                                 && res_.t4.bloom_down_compute != nullptr
                                 && res_.t4.bloom_mips[0] != INVALID_TEXTURE;
        pipeline_.addPass(pass_.bloom_compute, use_compute_bloom);
        pipeline_.addPass(pass_.bloom, config_.enable_bloom && !use_compute_bloom);

        pipeline_.addPass(pass_.dof,
                          config_.enable_dof && res_.t4.dof_shader != nullptr && !debug_.skip_dof);

        // Barrier + final composite
        pipeline_.addBarrier(4);
        pipeline_.addPassToDest(pass_.hdr_composite, false, 0.0f, 0.0f, 0.0f, 0.0f,
                                viewport_w_, viewport_h_);
    } else if (is_t2t3_bloom) {
        // T2/T3 pipeline
        if (config_.enable_shadows)
            pipeline_.addPass(pass_.shadow);
        pipeline_.addPass(pass_.scene_to_fbo);
        if (config_.enable_ssao)
            pipeline_.addPass(pass_.ssao);
        pipeline_.addPass(pass_.bloom);
        pipeline_.addPass(pass_.composite);
    } else {
        // T1 Basic pipeline
        if (config_.enable_shadows)
            pipeline_.addPass(pass_.shadow);
        pipeline_.addDefaultFBWithClear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f,
                                        viewport_w_, viewport_h_);
        pipeline_.addPass(pass_.sky);
        pipeline_.addPass(pass_.opaque);
        pipeline_.addPass(pass_.grass);
        pipeline_.addPass(pass_.fur);
        pipeline_.addPass(pass_.particle);
    }

    LOG_DBG("Pipeline: %d passes (%d enabled) for tier %d",
            static_cast<int>(pipeline_.passCount()),
            static_cast<int>(pipeline_.enabledCount()),
            t);

    initialized_ = true;
    int total_obj = static_cast<int>(opaque_objects_.size());
    LOG_INF("Demo scene setup complete: %d objects", total_obj);
    return true;
}

// ============================================================
// cleanup
// ============================================================

void DemoScene::cleanup(Renderer* r) {
    (void)r;
    if (!initialized_) return;

    // Scene object lists are per-tier, clear them
    opaque_objects_.clear();
    cloud_objects_.clear();

    // Resources are owned by DemoResources, not by DemoScene

    initialized_ = false;
}

// ============================================================
// getTechniqueInfo
// ============================================================

TechniqueInfo DemoScene::getTechniqueInfo() const {
    int total = static_cast<int>(opaque_objects_.size() + cloud_objects_.size());
    return getTierTechniqueInfo(tier_, total);
}

// ============================================================
// buildFrameData: construct per-frame data from camera + config
// ============================================================

FrameData DemoScene::buildFrameData(float t, float time, int w, int h,
                                    RenderTargetHandle dest_rt) {
    FrameData fd;
    fd.cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    fd.aspect = static_cast<float>(w) / static_cast<float>(h > 0 ? h : 1);
    fd.proj = Mat4::perspective(kDemoFovDeg, fd.aspect, kDemoNear, kDemoFar);
    fd.view = Mat4::lookAt(fd.cam_pos, cam_target, Vec3(0.0f, 1.0f, 0.0f));
    fd.sun_dir = SUN_DIR_RAW.normalized();
    fd.frustum = extractFrustum(fd.proj * fd.view);
    fd.time = time;
    fd.tier_int = static_cast<int>(tier_);
    fd.viewport_w = w;
    fd.viewport_h = h;
    fd.dest_rt = dest_rt;

    // Tier capability flags
    fd.has_shadows = config_.enable_shadows && res_.shadow.shader != nullptr
                     && res_.shadow.rt != INVALID_RENDER_TARGET;
    fd.has_bloom = config_.enable_bloom
                   && res_.bloom.extract_shader != nullptr
                   && res_.bloom.scene_rt != INVALID_RENDER_TARGET;
    fd.has_ssao = config_.enable_ssao
                  && res_.ssao.shader != nullptr
                  && res_.ssao.rt != INVALID_RENDER_TARGET;
    fd.has_pbr = config_.enable_pbr;
    fd.has_tessellation = config_.enable_tessellation && res_.t4.tess_shader != nullptr;
    fd.has_compute_particles = config_.enable_compute_particles
                               && res_.t4.compute_particle_shader != nullptr
                               && res_.t4.particle_ssbo != INVALID_BUFFER;
    fd.has_volumetric_fog = config_.enable_volumetric_fog
                            && res_.t4.volumetric_fog_shader != nullptr
                            && res_.t4.fog_rt != INVALID_RENDER_TARGET;
    fd.has_hdr = config_.enable_hdr
                 && res_.t4.tone_map_shader != nullptr
                 && res_.t4.hdr_scene_rt != INVALID_RENDER_TARGET;

    // Log active passes once per tier
    if (!passes_logged_) {
        passes_logged_ = true;
        LOG_DBG("Demo: tier %d passes: shadow=%d ssao=%d bloom=%d pbr=%d tess=%d "
                 "compute_particles=%d vol_fog=%d hdr=%d",
                 fd.tier_int, fd.has_shadows, fd.has_ssao, fd.has_bloom,
                 fd.has_pbr, fd.has_tessellation, fd.has_compute_particles,
                 fd.has_volumetric_fog, fd.has_hdr);
    }

    return fd;
}

// ============================================================
// ssrCopyCommand: copy framebuffer to SSR texture (pipeline command)
// ============================================================

void DemoScene::ssrCopyCommand(Renderer* r, FrameData& fd,
                               const TierResourceView& res,
                               const DemoTierConfig& cfg,
                               const SceneData& scene) {
    (void)cfg; (void)scene;
    if (res.t4.ssr_tex != INVALID_TEXTURE)
        r->copyFramebufferToTexture(res.t4.ssr_tex, fd.viewport_w, fd.viewport_h);
}

// ============================================================
// renderFrame: pipeline orchestrator
// ============================================================

void DemoScene::renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h,
                            RenderTargetHandle dest_rt) {
    if (!initialized_) return;
    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;
    dest_rt_ = dest_rt;

    FrameData fd = buildFrameData(t, time, viewport_w, viewport_h, dest_rt);
    pipeline_.execute(r, fd, res_, config_, scene_data_);

    // Restore GL state
    r->setDepthTest(true);
    r->setCullFace(true);
    r->setBlending(false);
    r->setColorMask(true, true, true, true);
}
