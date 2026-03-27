#include "bench/preset.h"
#include "tests/test_registry.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <cstdio>

static const BenchPreset PRESETS[static_cast<int>(PresetIndex::Count)] = {
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
        { 50 },                            // shader_fma iterations
        { 1000 },                          // instanced_draw instance_count
        { 50, 256 },                       // compute_fma iterations/work_groups
        { 512, 100 },                      // fbo_fillrate rt_size/layers
        { 512, 2, 50 },                    // mrt_fill rt_size/targets/layers
        { 256, 8, 50 },                    // texarray_sample tex/arr_layers/samp
        { 10000, 4 },                      // geom_shader points/tris_per_point
        { 8, 100 },                        // ubo_switch ubo_count/switches
        { 100000 },                        // transform_feedback vertex_count
        { 4, 256 },                        // compute_bw buffer_mb/work_groups
        { 100, 12 },                       // indirect_draw commands/mesh_tris
        { 100, 256 },                      // compute_smem iterations/work_groups
        { 50, 256 },                       // ssbo_atomics iterations/work_groups
        { 4, 1000 },                       // tess_tp subdivisions/patches
        { 256, 50 },                       // tex_gather tex_size/iterations
        { 256, 50 },                       // image_ls image_size/iterations
        { 65536, 256 },                    // compute_prefix elements/work_groups
        { 4, 2 },                          // persistent_map buffer_mb/frames_in_flight
        { 16, 100 },                       // bindless_tex tex_count/draws
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
        { 100 },
        { 5000 },                          // instanced_draw
        { 100, 1024 },                     // compute_fma
        { 1024, 300 },                     // fbo_fillrate
        { 1024, 3, 100 },                  // mrt_fill
        { 512, 16, 100 },                  // texarray_sample
        { 50000, 6 },                      // geom_shader
        { 16, 500 },                       // ubo_switch
        { 500000 },                        // transform_feedback
        { 16, 1024 },                      // compute_bw
        { 1000, 12 },                      // indirect_draw
        { 500, 1024 },                     // compute_smem
        { 200, 1024 },                     // ssbo_atomics
        { 8, 5000 },                       // tess_tp
        { 512, 100 },                      // tex_gather
        { 512, 100 },                      // image_ls
        { 262144, 1024 },                  // compute_prefix
        { 16, 3 },                         // persistent_map
        { 64, 500 },                       // bindless_tex
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
        { 200 },
        { 20000 },                         // instanced_draw
        { 200, 4096 },                     // compute_fma
        { 2048, 500 },                     // fbo_fillrate
        { 2048, 4, 200 },                  // mrt_fill
        { 1024, 32, 200 },                 // texarray_sample
        { 200000, 8 },                     // geom_shader
        { 32, 2000 },                      // ubo_switch
        { 2000000 },                       // transform_feedback
        { 64, 4096 },                      // compute_bw
        { 5000, 36 },                      // indirect_draw
        { 2000, 4096 },                    // compute_smem
        { 500, 4096 },                     // ssbo_atomics
        { 16, 20000 },                     // tess_tp
        { 1024, 200 },                     // tex_gather
        { 1024, 200 },                     // image_ls
        { 1048576, 4096 },                 // compute_prefix
        { 64, 3 },                         // persistent_map
        { 256, 2000 },                     // bindless_tex
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
        { 400 },
        { 50000 },                         // instanced_draw
        { 400, 16384 },                    // compute_fma
        { 4096, 800 },                     // fbo_fillrate
        { 4096, 4, 400 },                  // mrt_fill
        { 2048, 64, 400 },                 // texarray_sample
        { 500000, 12 },                    // geom_shader
        { 64, 5000 },                      // ubo_switch
        { 8000000 },                       // transform_feedback
        { 256, 16384 },                    // compute_bw
        { 20000, 36 },                     // indirect_draw
        { 5000, 16384 },                   // compute_smem
        { 1500, 16384 },                   // ssbo_atomics
        { 32, 50000 },                     // tess_tp
        { 2048, 400 },                     // tex_gather
        { 2048, 400 },                     // image_ls
        { 4194304, 16384 },                // compute_prefix
        { 256, 3 },                        // persistent_map
        { 512, 5000 },                     // bindless_tex
    },
    // Extreme — for modern high-end GPUs (RTX 3060+, RX 6700+, 6+ GB VRAM)
    {
        "Extreme", 240, 1200, 10.0,
        { 8000 },                          // fillrate layers
        { 80 },                            // geometry grid
        { 4096, 1500 },                    // texturing tex/layers
        { 2048, 4096, 4096, 192, 128, 30 }, // scene
        { 10000, 10000 },                  // drawcall mesh_count/draws
        { 2000, 0.3f },                    // overdraw layers/alpha
        { 4096, 80 },                      // texupload size/count
        { 2000, 24, 24 },                  // statechange switches/shaders/textures
        { 16000000 },                      // vertex count
        { 3000 },                          // shader_alu iterations
        { 800 },                           // shader_fma iterations
        { 100000 },                        // instanced_draw
        { 800, 32768 },                    // compute_fma
        { 4096, 1500 },                    // fbo_fillrate
        { 4096, 4, 800 },                  // mrt_fill
        { 4096, 128, 800 },               // texarray_sample
        { 1000000, 16 },                   // geom_shader
        { 128, 10000 },                    // ubo_switch
        { 16000000 },                      // transform_feedback
        { 512, 32768 },                    // compute_bw
        { 50000, 72 },                     // indirect_draw
        { 10000, 32768 },                  // compute_smem
        { 3000, 32768 },                   // ssbo_atomics
        { 64, 200000 },                    // tess_tp
        { 4096, 800 },                     // tex_gather
        { 4096, 800 },                     // image_ls
        { 16777216, 32768 },               // compute_prefix
        { 512, 4 },                        // persistent_map
        { 1024, 10000 },                   // bindless_tex
    },
};

const BenchPreset& getPreset(int index) {
    if (index < 0 || index >= static_cast<int>(PresetIndex::Count)) index = static_cast<int>(PresetIndex::Medium);
    return PRESETS[index];
}

BenchPreset getCustomPreset() {
    BenchPreset p = PRESETS[static_cast<int>(PresetIndex::Medium)];
    p.name = "Custom";
    return p;
}

PresetValidation validatePreset(const BenchPreset& p, const RenderCaps& caps) {
    PresetValidation v;
    v.ok = true;

    char buf[256];
    auto addError = [&v, &buf]() {
        v.ok = false;
        if (!v.reason.empty()) v.reason += '\n';
        v.reason += buf;
    };

    // Check texture sizes
    if (p.texturing.tex_size > caps.max_texture_size) {
        snprintf(buf, sizeof(buf), "Texturing: tex_size %d exceeds GPU max %d",
                 p.texturing.tex_size, caps.max_texture_size);
        addError();
    }
    if (p.scene.terrain_tex > caps.max_texture_size) {
        snprintf(buf, sizeof(buf), "Scene: terrain_tex %d exceeds GPU max %d",
                 p.scene.terrain_tex, caps.max_texture_size);
        addError();
    }
    if (p.scene.obj_tex > caps.max_texture_size) {
        snprintf(buf, sizeof(buf), "Scene: obj_tex %d exceeds GPU max %d",
                 p.scene.obj_tex, caps.max_texture_size);
        addError();
    }
    if (p.texupload.tex_size > caps.max_texture_size) {
        snprintf(buf, sizeof(buf), "TexUpload: tex_size %d exceeds GPU max %d",
                 p.texupload.tex_size, caps.max_texture_size);
        addError();
    }

    // Check 16-bit index limits for geometry grid
    if (!caps.supports_32bit_indices) {
        int max_grid = 13; // 13^3 * 24 = 52728 vertices fits in 16-bit
        if (p.geometry.grid_size > max_grid) {
            snprintf(buf, sizeof(buf), "Geometry: grid %d requires 32-bit indices (max=%d for 16-bit)",
                     p.geometry.grid_size, max_grid);
            addError();
        }
        if (p.scene.cube_grid > max_grid) {
            snprintf(buf, sizeof(buf), "Scene: cube_grid %d requires 32-bit indices (max=%d for 16-bit)",
                     p.scene.cube_grid, max_grid);
            addError();
        }
    }

    if (v.ok) {
        LOG_DBG("Preset: '%s' validated OK (max_tex=%d, 32bit_idx=%s)",
                 p.name, caps.max_texture_size, caps.supports_32bit_indices ? "yes" : "no");
    } else {
        LOG_DBG("Preset: '%s' validation failed: %s", p.name, v.reason.c_str());
    }
    return v;
}

int suggestPresetIndex(const RenderCaps& caps, uint32_t available_caps) {
    // Start from VRAM-based suggestion
    int preset = static_cast<int>(PresetIndex::Medium);
    if (caps.estimated_vram_mb > 0) {
        if (caps.estimated_vram_mb < 64)         preset = static_cast<int>(PresetIndex::Light);
        else if (caps.estimated_vram_mb < 256)   preset = static_cast<int>(PresetIndex::Medium);
        else if (caps.estimated_vram_mb < 1024)  preset = static_cast<int>(PresetIndex::Heavy);
        else if (caps.estimated_vram_mb < 6144)  preset = static_cast<int>(PresetIndex::Ultra);
        else                                     preset = static_cast<int>(PresetIndex::Extreme);
    }

    // Limit by max texture size (Heavy/Ultra need large textures)
    if (caps.max_texture_size <= 2048 && preset > static_cast<int>(PresetIndex::Medium))
        preset = static_cast<int>(PresetIndex::Medium);
    if (caps.max_texture_size <= 1024 && preset > static_cast<int>(PresetIndex::Light))
        preset = static_cast<int>(PresetIndex::Light);

    // Limit by GL version (GL 2.x without VAO -> cap at Medium)
    if (caps.gl_major < 3 && !(available_caps & Cap_VAO) && preset > static_cast<int>(PresetIndex::Medium))
        preset = static_cast<int>(PresetIndex::Medium);

    return preset;
}
