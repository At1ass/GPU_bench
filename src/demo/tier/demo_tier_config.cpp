#include "demo/scene/demo_scene.h"
#include "renderer/features.h"

// ============================================================
// Tier configuration: progressive initialization
// ============================================================

DemoTierConfig getTierConfig(DemoTier tier) {
    DemoTierConfig c;
    c.tier = tier;

    // Sky + normals (same for all tiers)
    c.enable_sky = true;
    c.enable_normal_maps = true;
    c.normal_map_strength = 0.4f;

    // Scene
    c.fog_density = 0.045f;

    // T4 Ultra defaults (disabled for lower tiers)
    c.enable_pcss = false;
    c.enable_gtao = false;
    c.enable_compute_bloom = false;
    c.enable_auto_exposure = false;
    c.enable_ssr = false;
    c.enable_dof = false;
    c.enable_sss = false;
    c.sss_strength = 0.0f;
    c.chromatic_strength = 0.0f;
    c.grain_strength = 0.0f;
    c.dof_focal_distance = 3.5f;
    c.dof_strength = 1.0f;
    c.light_size = 0.0f;
    c.fog_steps = 32;
    c.displacement_strength = 0.02f;

    // Fur: varies by tier
    // density = cells per world unit. Model is ~2 units across.
    // With height-per-cell approach, higher density = finer fur.
    switch (tier) {
        case DemoTier::Basic:
            c.fur_shells = 24;
            c.fur_length = 0.08f;
            c.fur_density = 80.0f;
            c.fur_thickness = 0.9f;
            c.rock_count = 15;
            c.grass_count = 800;
            c.particle_count = 200;
            c.enable_wind = true;
            c.instanced_grass_count = 0;
            c.grass_area_size = 20.0f;
            c.enable_shadows = false;
            c.shadow_map_size = 0;
            c.enable_bloom = false;
            c.bloom_strength = 0.0f;
            c.enable_ssao = false;
            c.ssao_radius = 0.0f;
            c.ssao_intensity = 0.0f;
            c.point_light_count = 0;
            c.enable_normal_map_texture = false;
            c.enable_pbr = false;
            c.enable_tessellation = false;
            c.tess_level = 0;
            c.enable_compute_particles = false;
            c.compute_particle_count = 0;
            c.enable_volumetric_fog = false;
            c.enable_hdr = false;
            break;
        case DemoTier::Enhanced:
            c.fur_shells = 24;
            c.fur_length = 0.06f;
            c.fur_density = 120.0f;
            c.fur_thickness = 0.75f;
            c.rock_count = 15;
            c.grass_count = 30;
            c.particle_count = 300;
            c.enable_wind = true;
            c.instanced_grass_count = 30000;
            c.grass_area_size = 20.0f;
            c.enable_shadows = true;
            c.shadow_map_size = 1024;
            c.enable_bloom = true;
            c.bloom_strength = 0.3f;
            c.enable_ssao = true;
            c.ssao_radius = 0.35f;
            c.ssao_intensity = 1.0f;
            c.point_light_count = 0;
            c.enable_normal_map_texture = false;
            c.enable_pbr = false;
            c.enable_tessellation = false;
            c.tess_level = 0;
            c.enable_compute_particles = false;
            c.compute_particle_count = 0;
            c.enable_volumetric_fog = false;
            c.enable_hdr = false;
            break;
        case DemoTier::Quality:
            c.fur_shells = 48;
            c.fur_length = 0.07f;
            c.fur_density = 180.0f;
            c.fur_thickness = 0.7f;
            c.rock_count = 25;
            c.grass_count = 30;
            c.particle_count = 500;
            c.enable_wind = true;
            c.instanced_grass_count = 50000;
            c.grass_area_size = 20.0f;
            c.enable_shadows = true;
            c.shadow_map_size = 2048;
            c.enable_bloom = true;
            c.bloom_strength = 0.3f;
            c.enable_ssao = true;
            c.ssao_radius = 0.35f;
            c.ssao_intensity = 1.0f;
            c.point_light_count = 3;
            c.enable_normal_map_texture = true;
            c.enable_pbr = false;
            c.enable_tessellation = false;
            c.tess_level = 0;
            c.enable_compute_particles = false;
            c.compute_particle_count = 0;
            c.enable_volumetric_fog = false;
            c.enable_hdr = false;
            break;
        case DemoTier::Ultra:
            c.fur_shells = 64;
            c.fur_length = 0.07f;
            c.fur_density = 250.0f;
            c.fur_thickness = 0.65f;
            c.rock_count = 25;
            c.grass_count = 30;
            c.particle_count = 800;
            c.enable_wind = true;
            c.instanced_grass_count = 80000;
            c.grass_area_size = 20.0f;
            c.enable_shadows = true;
            c.shadow_map_size = 4096;
            c.enable_bloom = true;
            c.bloom_strength = 0.25f;
            c.enable_ssao = true;
            c.ssao_radius = 0.5f;
            c.ssao_intensity = 1.0f;
            c.point_light_count = 3;
            c.enable_normal_map_texture = true;
            c.enable_pbr = true;
            c.enable_tessellation = true;
            c.tess_level = 6;
            c.enable_compute_particles = true;
            c.compute_particle_count = 1024;
            c.enable_volumetric_fog = true;
            c.enable_hdr = true;
            // T4 Ultra enhancements
            c.enable_pcss = true;
            c.enable_gtao = true;
            c.enable_compute_bloom = true;
            c.enable_auto_exposure = true;
            c.enable_ssr = true;
            c.enable_dof = true;
            c.enable_sss = true;
            c.sss_strength = 0.8f;
            c.chromatic_strength = 0.0005f;
            c.grain_strength = 0.025f;
            c.dof_focal_distance = 3.5f;
            c.dof_strength = 0.5f;
            c.light_size = 0.04f;
            c.fog_steps = 64;
            c.displacement_strength = 0.03f;
            break;
    }

    return c;
}

// ============================================================
// Technique info for UI overlay
// ============================================================

TechniqueInfo getTierTechniqueInfo(DemoTier tier, int object_count) {
    TechniqueInfo info;
    info.object_count = object_count;

    int fur_shells = getTierConfig(tier).fur_shells;

    switch (tier) {
        case DemoTier::Basic:
            info.shading = "Blinn-Phong";
            info.shadows = "None";
            info.lighting = "1 Sun + Fill + Rim";
            info.ambient = "Hemisphere";
            info.postfx = "Vignette + Color Grade";
            info.particles = "Billboard Dust (200)";
            info.extras = "Shell Fur (24), Sky, Rocks, Grass+Wind";
            info.estimated_draw_calls = object_count + fur_shells + 2; // +1 sky, +1 particles
            break;
        case DemoTier::Enhanced:
            info.shading = "Blinn-Phong";
            info.shadows = "Shadow Map (1024) + PCF 3x3";
            info.lighting = "1 Sun + Fill + Rim";
            info.ambient = "Hemisphere + SSAO";
            info.postfx = "Bloom + Vignette + Color Grade";
            info.particles = "Billboard Dust (200)";
            info.extras = "Instanced Fur (24), Sky+Clouds, Rocks, Instanced Grass (300)+Wind";
            info.estimated_draw_calls = object_count * 2 + fur_shells + 3; // shadow pass + main pass + sky + particles
            break;
        case DemoTier::Quality:
            info.shading = "Blinn-Phong + Normal Map";
            info.shadows = "Shadow Map (2048) + PCF 5x5";
            info.lighting = "1 Sun + 3 Point Lights + Fill + Rim";
            info.ambient = "Hemisphere + SSAO";
            info.postfx = "Bloom + Vignette + Color Grade";
            info.particles = "Billboard Dust (400)";
            info.extras = "Instanced Fur (48), Sky+Clouds, Rocks, Instanced Grass (40000)+Wind";
            info.estimated_draw_calls = object_count * 2 + fur_shells + 4;
            break;
        case DemoTier::Ultra:
            info.shading = "PBR (Cook-Torrance GGX) + SSS";
            info.shadows = "PCSS (4096, variable penumbra)";
            info.lighting = "1 Sun + 3 Point + Fill + Rim + Auto-Exposure";
            info.ambient = "Compute GTAO (full-res)";
            info.postfx = "HDR + ACES + Compute Bloom + SSR + Vol Fog + DoF + Chromatic + Grain";
            info.particles = "Compute Particles (2K, HDR glow)";
            info.extras = "Tessellation (PN-tri), Fur (64), Grass (60K), Puddles";
            info.estimated_draw_calls = object_count * 2 + fur_shells + 6;
            break;
    }
    return info;
}

// ============================================================
// Max supported tier detection
// ============================================================

int maxSupportedTier(const Renderer& r) {
    const ComputeFeatures* cf = r.features<ComputeFeatures>();
    if (cf && cf->hasCompute()) {
        const GL4Features* g4 = r.features<GL4Features>();
        if (g4 && g4->hasTessellation())
            return 4;
    }
    const GL3Features* g3 = r.features<GL3Features>();
    if (g3 && g3->hasVAO()) {
        const RenderCaps& caps = r.getCaps();
        if (caps.gl_major > 3 || (caps.gl_major == 3 && caps.gl_minor >= 3))
            return 3;
    }
    if (r.supportsRenderTargets())
        return 2;
    return 1;
}
