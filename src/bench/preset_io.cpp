#include "bench/preset_io.h"
#include "platform/compat.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

bool saveConfig(const char* path, const BenchPreset& preset) {
    FileGuard f(fopen(path, "w"));
    if (!f) return false;

    fprintf(f.get(), "[preset]\n");
    fprintf(f.get(), "name=%s\n", preset.name);
    fprintf(f.get(), "warmup_frames=%d\n", preset.warmup_frames);
    fprintf(f.get(), "measure_frames=%d\n", preset.measure_frames);
    fprintf(f.get(), "min_duration_sec=%.1f\n", preset.min_duration_sec);

    fprintf(f.get(), "\n[fillrate]\n");
    fprintf(f.get(), "layers=%d\n", preset.fillrate.layers);

    fprintf(f.get(), "\n[geometry]\n");
    fprintf(f.get(), "grid_size=%d\n", preset.geometry.grid_size);

    fprintf(f.get(), "\n[texturing]\n");
    fprintf(f.get(), "tex_size=%d\n", preset.texturing.tex_size);
    fprintf(f.get(), "layers=%d\n", preset.texturing.layers);

    fprintf(f.get(), "\n[scene]\n");
    fprintf(f.get(), "terrain_res=%d\n", preset.scene.terrain_res);
    fprintf(f.get(), "terrain_tex=%d\n", preset.scene.terrain_tex);
    fprintf(f.get(), "obj_tex=%d\n", preset.scene.obj_tex);
    fprintf(f.get(), "sphere_segs=%d\n", preset.scene.sphere_segs);
    fprintf(f.get(), "sphere_rings=%d\n", preset.scene.sphere_rings);
    fprintf(f.get(), "cube_grid=%d\n", preset.scene.cube_grid);

    fprintf(f.get(), "\n[drawcall]\n");
    fprintf(f.get(), "mesh_count=%d\n", preset.drawcall.mesh_count);
    fprintf(f.get(), "draws_per_frame=%d\n", preset.drawcall.draws_per_frame);

    fprintf(f.get(), "\n[overdraw]\n");
    fprintf(f.get(), "layers=%d\n", preset.overdraw.layers);
    fprintf(f.get(), "alpha=%.2f\n", preset.overdraw.alpha);

    fprintf(f.get(), "\n[texupload]\n");
    fprintf(f.get(), "tex_size=%d\n", preset.texupload.tex_size);
    fprintf(f.get(), "uploads_per_frame=%d\n", preset.texupload.uploads_per_frame);

    fprintf(f.get(), "\n[statechange]\n");
    fprintf(f.get(), "switches=%d\n", preset.statechange.switches);
    fprintf(f.get(), "shader_count=%d\n", preset.statechange.shader_count);
    fprintf(f.get(), "tex_count=%d\n", preset.statechange.tex_count);

    fprintf(f.get(), "\n[vertex]\n");
    fprintf(f.get(), "vertex_count=%d\n", preset.vertex.vertex_count);

    fprintf(f.get(), "\n[shader_alu]\n");
    fprintf(f.get(), "iterations=%d\n", preset.shader_alu.iterations);

    return true;
}

static void parseLine(const char* line, const std::string& section, BenchPreset& p) {
    const char* eq = strchr(line, '=');
    if (!eq) return;
    std::string key(line, eq - line);
    std::string val(eq + 1);
    // Trim whitespace
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    while (!val.empty() && (val.back() == '\n' || val.back() == '\r' || val.back() == ' ')) val.pop_back();

    if (section == "preset") {
        if (key == "warmup_frames")    p.warmup_frames = atoi(val.c_str());
        if (key == "measure_frames")   p.measure_frames = atoi(val.c_str());
        if (key == "min_duration_sec") p.min_duration_sec = atof(val.c_str());
    } else if (section == "fillrate") {
        if (key == "layers") p.fillrate.layers = atoi(val.c_str());
    } else if (section == "geometry") {
        if (key == "grid_size") p.geometry.grid_size = atoi(val.c_str());
    } else if (section == "texturing") {
        if (key == "tex_size") p.texturing.tex_size = atoi(val.c_str());
        if (key == "layers")   p.texturing.layers = atoi(val.c_str());
    } else if (section == "scene") {
        if (key == "terrain_res")  p.scene.terrain_res = atoi(val.c_str());
        if (key == "terrain_tex")  p.scene.terrain_tex = atoi(val.c_str());
        if (key == "obj_tex")      p.scene.obj_tex = atoi(val.c_str());
        if (key == "sphere_segs")  p.scene.sphere_segs = atoi(val.c_str());
        if (key == "sphere_rings") p.scene.sphere_rings = atoi(val.c_str());
        if (key == "cube_grid")    p.scene.cube_grid = atoi(val.c_str());
    } else if (section == "drawcall") {
        if (key == "mesh_count")      p.drawcall.mesh_count = atoi(val.c_str());
        if (key == "draws_per_frame") p.drawcall.draws_per_frame = atoi(val.c_str());
    } else if (section == "overdraw") {
        if (key == "layers") p.overdraw.layers = atoi(val.c_str());
        if (key == "alpha")  p.overdraw.alpha = static_cast<float>(atof(val.c_str()));
    } else if (section == "texupload") {
        if (key == "tex_size")          p.texupload.tex_size = atoi(val.c_str());
        if (key == "uploads_per_frame") p.texupload.uploads_per_frame = atoi(val.c_str());
    } else if (section == "statechange") {
        if (key == "switches")     p.statechange.switches = atoi(val.c_str());
        if (key == "shader_count") p.statechange.shader_count = atoi(val.c_str());
        if (key == "tex_count")    p.statechange.tex_count = atoi(val.c_str());
    } else if (section == "vertex") {
        if (key == "vertex_count") p.vertex.vertex_count = atoi(val.c_str());
    } else if (section == "shader_alu") {
        if (key == "iterations") p.shader_alu.iterations = atoi(val.c_str());
    }
}

bool loadConfig(const char* path, BenchPreset& preset) {
    FileGuard f(fopen(path, "r"));
    if (!f) return false;

    // Start from Medium as base
    preset = getPreset(static_cast<int>(PresetIndex::Medium));
    preset.name = "Custom";

    char line[512];
    std::string section;

    while (fgets(line, sizeof(line), f.get())) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\r')
            continue;

        // Section header
        if (line[0] == '[') {
            char* end = strchr(line, ']');
            if (end) {
                section = std::string(line + 1, end - line - 1);
            }
            continue;
        }

        parseLine(line, section, preset);
    }

    return true;
}
