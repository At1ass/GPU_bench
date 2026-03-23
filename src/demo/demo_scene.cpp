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

    // Initialize pass objects
    sky_pass_.init(res_);
    shadow_pass_.init(res_);
    opaque_pass_.init(res_);
    grass_pass_.init(res_);
    fur_pass_.init(res_);
    particle_pass_.init(res_);
    ssao_pass_.init(res_);
    bloom_pass_.init(res_);
    composite_pass_.init(res_);
    hdr_composite_pass_.init(res_);
    compute_particles_pass_.init(res_);
    compute_particles_draw_pass_.init(res_);
    tess_model_pass_.init(res_);
    gtao_pass_.init(res_);
    bloom_compute_pass_.init(res_);
    auto_exposure_pass_.init(res_);
    vol_fog_pass_.init(res_);
    water_pass_.init(res_);
    ssr_pass_.init(res_);
    dof_pass_.init(res_);

    // Wire sub-pass dependencies
    tess_model_pass_.setFurPass(&fur_pass_);
    scene_to_fbo_pass_.setSubPasses(&sky_pass_, &opaque_pass_, &grass_pass_, &fur_pass_, &particle_pass_);

    // Build render pipeline for this tier
    pipeline_.clear();

    bool is_t4_hdr = config_.enable_hdr && res_.t4.hdr_scene_rt != INVALID_RENDER_TARGET && !debug_.skip_hdr;
    bool is_t2t3_bloom = config_.enable_bloom && !is_t4_hdr;

    if (is_t4_hdr) {
        // T4 Ultra HDR pipeline (post-processing passes only --
        // scene objects are rendered directly in renderFrame due to FBO bind/clear)
        if (config_.enable_auto_exposure && !debug_.skip_auto_exposure)
            pipeline_.addPass(&auto_exposure_pass_);
        if (!debug_.skip_gtao && config_.enable_gtao && res_.t4.gtao_shader)
            pipeline_.addPass(&gtao_pass_);
        else if (config_.enable_ssao)
            pipeline_.addPass(&ssao_pass_);
        if (config_.enable_volumetric_fog && !debug_.skip_vol_fog)
            pipeline_.addPass(&vol_fog_pass_);
        if (!debug_.skip_compute_bloom && config_.enable_compute_bloom && res_.t4.bloom_down_compute)
            pipeline_.addPass(&bloom_compute_pass_);
        else if (config_.enable_bloom)
            pipeline_.addPass(&bloom_pass_);
        if (config_.enable_dof && res_.t4.dof_shader && !debug_.skip_dof)
            pipeline_.addPass(&dof_pass_);
    } else if (is_t2t3_bloom) {
        // T2/T3 pipeline
        if (config_.enable_shadows)
            pipeline_.addPass(&shadow_pass_);
        pipeline_.addPass(&scene_to_fbo_pass_);
        if (config_.enable_ssao)
            pipeline_.addPass(&ssao_pass_);
        pipeline_.addPass(&bloom_pass_);
        pipeline_.addPass(&composite_pass_);
    } else {
        // T1 Basic pipeline
        if (config_.enable_shadows)
            pipeline_.addPass(&shadow_pass_);
        pipeline_.addPass(&sky_pass_);
        pipeline_.addPass(&opaque_pass_);
        pipeline_.addPass(&grass_pass_);
        pipeline_.addPass(&fur_pass_);
        pipeline_.addPass(&particle_pass_);
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
// renderFrame: pipeline orchestrator
// ============================================================

void DemoScene::renderFrame(Renderer* r, float t, float time, int viewport_w, int viewport_h,
                            RenderTargetHandle dest_rt) {
    if (!initialized_) return;

    viewport_w_ = viewport_w;
    viewport_h_ = viewport_h;
    dest_rt_ = dest_rt;

    // Build frame data
    FrameData fd;
    fd.cam_pos = camera_.getPosition(t);
    Vec3 cam_target = camera_.getTarget(t);
    fd.aspect = static_cast<float>(viewport_w) / static_cast<float>(viewport_h > 0 ? viewport_h : 1);
    fd.proj = Mat4::perspective(kDemoFovDeg, fd.aspect, kDemoNear, kDemoFar);
    fd.view = Mat4::lookAt(fd.cam_pos, cam_target, Vec3(0.0f, 1.0f, 0.0f));
    fd.sun_dir = SUN_DIR_RAW.normalized();
    fd.frustum = extractFrustum(fd.proj * fd.view);
    fd.time = time;
    fd.tier_int = static_cast<int>(tier_);
    fd.viewport_w = viewport_w;
    fd.viewport_h = viewport_h;
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

    bool is_t4_hdr = fd.has_hdr && !debug_.skip_hdr;

    if (is_t4_hdr) {
        // T4: Shadow + compute particles, then scene to HDR FBO, then post-process pipeline
        if (fd.has_shadows) {
            computeLightMatrix(fd);
            shadow_pass_.execute(r, fd, res_, config_, scene_data_);
        }
        if (fd.has_compute_particles)
            compute_particles_pass_.execute(r, fd, res_, config_, scene_data_);

        // Render scene to HDR FBO
        r->bindRenderTarget(res_.t4.hdr_scene_rt);
        r->setViewport(0, 0, viewport_w, viewport_h);
        r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
        r->setDepthTest(true);
        r->setCullFace(true);
        sky_pass_.execute(r, fd, res_, config_, scene_data_);
        opaque_pass_.execute(r, fd, res_, config_, scene_data_);
        grass_pass_.execute(r, fd, res_, config_, scene_data_);
        if (fd.has_tessellation)
            tess_model_pass_.execute(r, fd, res_, config_, scene_data_);
        else
            fur_pass_.execute(r, fd, res_, config_, scene_data_);
        if (fd.has_compute_particles)
            compute_particles_draw_pass_.execute(r, fd, res_, config_, scene_data_);
        else
            particle_pass_.execute(r, fd, res_, config_, scene_data_);

        // SSR texture copy + water
        if (config_.enable_ssr && res_.t4.ssr_tex != INVALID_TEXTURE)
            r->copyFramebufferToTexture(res_.t4.ssr_tex, viewport_w, viewport_h);
        water_pass_.execute(r, fd, res_, config_, scene_data_);

        r->bindRenderTarget(INVALID_RENDER_TARGET);

        // Post-processing via pipeline (auto-exposure, AO, fog, bloom, dof)
        pipeline_.execute(r, fd, res_, config_, scene_data_);

        // Restore full-res viewport after half-res passes
        r->setViewport(0, 0, viewport_w, viewport_h);

        // Compute barriers before final composite
        {
            GL4Features* g4 = r->features<GL4Features>();
            if (g4) {
                for (int iu = 0; iu < 4; iu++)
                    g4->bindImageTexture(INVALID_TEXTURE, iu, false, false);
            }
            ComputeFeatures* cf = r->features<ComputeFeatures>();
            if (cf) cf->computeMemoryBarrier();
        }

        // Final HDR composite
        r->bindRenderTarget(dest_rt);
        r->setViewport(0, 0, viewport_w, viewport_h);
        hdr_composite_pass_.execute(r, fd, res_, config_, scene_data_);
    } else if (fd.has_bloom) {
        // T2/T3: pipeline handles everything (shadow, sceneToFBO, ssao, bloom, composite)
        if (fd.has_shadows)
            computeLightMatrix(fd);
        pipeline_.execute(r, fd, res_, config_, scene_data_);
    } else {
        // T1: pipeline handles everything (shadow, sky, opaque, grass, fur, particles)
        if (fd.has_shadows)
            computeLightMatrix(fd);
        r->setViewport(0, 0, viewport_w, viewport_h);
        r->clear(FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, 1.0f);
        r->setDepthTest(true);
        r->setCullFace(true);
        pipeline_.execute(r, fd, res_, config_, scene_data_);
    }

    // Restore state
    r->setDepthTest(true);
    r->setCullFace(true);
    r->setBlending(false);
    r->setColorMask(true, true, true, true);
}
