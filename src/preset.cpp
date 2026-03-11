#include "preset.h"
#include "compat.h"
#include <cstdio>

static const BenchPreset PRESETS[PRESET_COUNT] = {
    // Light
    {
        "Light", 60, 300, 3.0,
        { 100 },                           // fillrate layers
        { 10 },                            // geometry grid
        { 512, 100 },                      // texturing tex/layers
        { 128, 512, 512, 32, 16, 5 },     // scene
        { 100, 100 },                      // drawcall mesh_count/draws
        { 50, 0.3f },                      // overdraw layers/alpha
        { 256, 10 },                       // texupload size/count
        { 50, 4, 4 },                      // statechange switches/shaders/textures
        { 100000 },                        // vertex count
        { 50 },                            // shader_alu iterations
    },
    // Medium
    {
        "Medium", 120, 600, 5.0,
        { 500 },
        { 25 },
        { 1024, 300 },
        { 256, 1024, 1024, 64, 32, 10 },
        { 500, 500 },
        { 200, 0.3f },
        { 512, 20 },
        { 200, 8, 8 },
        { 500000 },
        { 200 },
    },
    // Heavy
    {
        "Heavy", 120, 600, 5.0,
        { 1500 },
        { 35 },
        { 2048, 500 },
        { 512, 2048, 2048, 96, 64, 15 },
        { 2000, 2000 },
        { 500, 0.3f },
        { 1024, 30 },
        { 500, 12, 12 },
        { 2000000 },
        { 500 },
    },
    // Ultra
    {
        "Ultra", 180, 900, 8.0,
        { 4000 },
        { 50 },
        { 4096, 800 },
        { 1024, 4096, 4096, 128, 96, 20 },
        { 5000, 5000 },
        { 1000, 0.3f },
        { 2048, 50 },
        { 1000, 16, 16 },
        { 8000000 },
        { 1500 },
    },
};

const BenchPreset& getPreset(int index) {
    if (index < 0 || index >= PRESET_COUNT) index = PRESET_MEDIUM;
    return PRESETS[index];
}

BenchPreset getCustomPreset() {
    BenchPreset p = PRESETS[PRESET_MEDIUM];
    p.name = "Custom";
    return p;
}

PresetValidation validatePreset(const BenchPreset& p, const RenderCaps& caps) {
    PresetValidation v;
    v.ok = true;

    // Check texture sizes
    if (p.texturing.tex_size > caps.max_texture_size) {
        v.ok = false;
        char buf[256];
        snprintf(buf, sizeof(buf), "Texturing test requires %d textures but GPU max is %d",
                 p.texturing.tex_size, caps.max_texture_size);
        v.reason = buf;
        return v;
    }
    if (p.scene.terrain_tex > caps.max_texture_size) {
        v.ok = false;
        char buf[256];
        snprintf(buf, sizeof(buf), "Scene test requires %d terrain texture but GPU max is %d",
                 p.scene.terrain_tex, caps.max_texture_size);
        v.reason = buf;
        return v;
    }
    if (p.scene.obj_tex > caps.max_texture_size) {
        v.ok = false;
        char buf[256];
        snprintf(buf, sizeof(buf), "Scene test requires %d object texture but GPU max is %d",
                 p.scene.obj_tex, caps.max_texture_size);
        v.reason = buf;
        return v;
    }
    if (p.texupload.tex_size > caps.max_texture_size) {
        v.ok = false;
        char buf[256];
        snprintf(buf, sizeof(buf), "TexUpload test requires %d textures but GPU max is %d",
                 p.texupload.tex_size, caps.max_texture_size);
        v.reason = buf;
        return v;
    }

    // Check 16-bit index limits for geometry grid
    if (!caps.supports_32bit_indices) {
        // cubeGrid: grid^3 * 24 vertices must fit in 16-bit (65535)
        int max_grid = 13; // 13^3 * 24 = 52728
        if (p.geometry.grid_size > max_grid) {
            v.ok = false;
            char buf[256];
            snprintf(buf, sizeof(buf), "Geometry grid %d requires 32-bit indices (max grid=%d for 16-bit)",
                     p.geometry.grid_size, max_grid);
            v.reason = buf;
            return v;
        }
        if (p.scene.cube_grid > max_grid) {
            v.ok = false;
            char buf[256];
            snprintf(buf, sizeof(buf), "Scene cube grid %d requires 32-bit indices (max=%d for 16-bit)",
                     p.scene.cube_grid, max_grid);
            v.reason = buf;
            return v;
        }
    }

    return v;
}

int suggestPresetIndex(const RenderCaps& caps) {
    if (caps.estimated_vram_mb <= 0) return PRESET_MEDIUM; // Unknown → Medium
    if (caps.estimated_vram_mb < 64)  return PRESET_LIGHT;
    if (caps.estimated_vram_mb < 256) return PRESET_MEDIUM;
    if (caps.estimated_vram_mb < 1024) return PRESET_HEAVY;
    return PRESET_ULTRA;
}
