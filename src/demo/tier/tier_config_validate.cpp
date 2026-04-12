#include "demo/tier/tier_config_validate.h"
#include "demo/scene/demo_scene.h"
#include "platform/logger.h"

// Helper: true if v is a power of two
static bool isPow2(int v) { return v > 0 && (v & (v - 1)) == 0; }

bool validateTierConfig(const DemoTierConfig& cfg) {
    bool ok = true;

    // Fur
    if (cfg.fur_shells < 1 || cfg.fur_shells > 256) {
        LOG_WRN("TierConfig: fur_shells=%d out of range [1,256]", cfg.fur_shells);
        ok = false;
    }
    if (cfg.fur_length < 0.001f || cfg.fur_length > 1.0f) {
        LOG_WRN("TierConfig: fur_length=%.4f out of range [0.001,1.0]", static_cast<double>(cfg.fur_length));
        ok = false;
    }
    if (cfg.fur_density < 1.0f || cfg.fur_density > 1000.0f) {
        LOG_WRN("TierConfig: fur_density=%.1f out of range [1.0,1000.0]", static_cast<double>(cfg.fur_density));
        ok = false;
    }
    if (cfg.fur_thickness < 0.01f || cfg.fur_thickness > 1.0f) {
        LOG_WRN("TierConfig: fur_thickness=%.3f out of range [0.01,1.0]", static_cast<double>(cfg.fur_thickness));
        ok = false;
    }

    // Scene
    if (cfg.fog_density < 0.0f || cfg.fog_density > 1.0f) {
        LOG_WRN("TierConfig: fog_density=%.3f out of range [0.0,1.0]", static_cast<double>(cfg.fog_density));
        ok = false;
    }

    // Normals
    if (cfg.normal_map_strength < 0.0f || cfg.normal_map_strength > 2.0f) {
        LOG_WRN("TierConfig: normal_map_strength=%.3f out of range [0.0,2.0]", static_cast<double>(cfg.normal_map_strength));
        ok = false;
    }

    // Scene enrichment
    if (cfg.rock_count < 0 || cfg.rock_count > 100) {
        LOG_WRN("TierConfig: rock_count=%d out of range [0,100]", cfg.rock_count);
        ok = false;
    }
    if (cfg.grass_count < 0 || cfg.grass_count > 10000) {
        LOG_WRN("TierConfig: grass_count=%d out of range [0,10000]", cfg.grass_count);
        ok = false;
    }
    if (cfg.particle_count < 0 || cfg.particle_count > 10000) {
        LOG_WRN("TierConfig: particle_count=%d out of range [0,10000]", cfg.particle_count);
        ok = false;
    }

    // Instanced grass
    if (cfg.instanced_grass_count < 0 || cfg.instanced_grass_count > 500000) {
        LOG_WRN("TierConfig: instanced_grass_count=%d out of range [0,500000]", cfg.instanced_grass_count);
        ok = false;
    }
    if (cfg.grass_area_size < 1.0f || cfg.grass_area_size > 100.0f) {
        LOG_WRN("TierConfig: grass_area_size=%.1f out of range [1.0,100.0]", static_cast<double>(cfg.grass_area_size));
        ok = false;
    }

    // Shadow mapping
    if (cfg.shadow_map_size != 0) {
        if (cfg.shadow_map_size < 256 || cfg.shadow_map_size > 8192 || !isPow2(cfg.shadow_map_size)) {
            LOG_WRN("TierConfig: shadow_map_size=%d must be 0 or power-of-2 in [256,8192]", cfg.shadow_map_size);
            ok = false;
        }
    }

    // Bloom
    if (cfg.bloom_strength < 0.0f || cfg.bloom_strength > 2.0f) {
        LOG_WRN("TierConfig: bloom_strength=%.3f out of range [0.0,2.0]", static_cast<double>(cfg.bloom_strength));
        ok = false;
    }

    // SSAO
    if (cfg.ssao_radius < 0.0f || cfg.ssao_radius > 5.0f) {
        LOG_WRN("TierConfig: ssao_radius=%.3f out of range [0.0,5.0]", static_cast<double>(cfg.ssao_radius));
        ok = false;
    }
    if (cfg.ssao_intensity < 0.0f || cfg.ssao_intensity > 5.0f) {
        LOG_WRN("TierConfig: ssao_intensity=%.3f out of range [0.0,5.0]", static_cast<double>(cfg.ssao_intensity));
        ok = false;
    }

    // Point lights
    if (cfg.point_light_count < 0 || cfg.point_light_count > 16) {
        LOG_WRN("TierConfig: point_light_count=%d out of range [0,16]", cfg.point_light_count);
        ok = false;
    }

    // Tessellation
    if (cfg.tess_level < 0 || cfg.tess_level > 64) {
        LOG_WRN("TierConfig: tess_level=%d out of range [0,64]", cfg.tess_level);
        ok = false;
    }

    // Compute particles
    if (cfg.compute_particle_count < 0 || cfg.compute_particle_count > 100000) {
        LOG_WRN("TierConfig: compute_particle_count=%d out of range [0,100000]", cfg.compute_particle_count);
        ok = false;
    }
    if (cfg.compute_particle_count > 0 && (cfg.compute_particle_count % 256) != 0) {
        LOG_WRN("TierConfig: compute_particle_count=%d should be a multiple of 256", cfg.compute_particle_count);
        ok = false;
    }

    // T4 Ultra enhancements
    if (cfg.sss_strength < 0.0f || cfg.sss_strength > 2.0f) {
        LOG_WRN("TierConfig: sss_strength=%.3f out of range [0.0,2.0]", static_cast<double>(cfg.sss_strength));
        ok = false;
    }
    if (cfg.chromatic_strength < 0.0f || cfg.chromatic_strength > 0.01f) {
        LOG_WRN("TierConfig: chromatic_strength=%.4f out of range [0.0,0.01]", static_cast<double>(cfg.chromatic_strength));
        ok = false;
    }
    if (cfg.grain_strength < 0.0f || cfg.grain_strength > 0.2f) {
        LOG_WRN("TierConfig: grain_strength=%.3f out of range [0.0,0.2]", static_cast<double>(cfg.grain_strength));
        ok = false;
    }
    if (cfg.dof_focal_distance < 0.1f || cfg.dof_focal_distance > 100.0f) {
        LOG_WRN("TierConfig: dof_focal_distance=%.2f out of range [0.1,100.0]", static_cast<double>(cfg.dof_focal_distance));
        ok = false;
    }
    if (cfg.dof_strength < 0.0f || cfg.dof_strength > 2.0f) {
        LOG_WRN("TierConfig: dof_strength=%.3f out of range [0.0,2.0]", static_cast<double>(cfg.dof_strength));
        ok = false;
    }
    if (cfg.light_size < 0.0f || cfg.light_size > 1.0f) {
        LOG_WRN("TierConfig: light_size=%.3f out of range [0.0,1.0]", static_cast<double>(cfg.light_size));
        ok = false;
    }
    if (cfg.fog_steps < 1 || cfg.fog_steps > 256) {
        LOG_WRN("TierConfig: fog_steps=%d out of range [1,256]", cfg.fog_steps);
        ok = false;
    }
    if (cfg.displacement_strength < 0.0f || cfg.displacement_strength > 0.5f) {
        LOG_WRN("TierConfig: displacement_strength=%.3f out of range [0.0,0.5]", static_cast<double>(cfg.displacement_strength));
        ok = false;
    }

    return ok;
}
