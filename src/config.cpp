#include "config.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

bool saveConfig(const char* path, const BenchPreset& preset) {
    FILE* f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "[preset]\n");
    fprintf(f, "name=%s\n", preset.name);
    fprintf(f, "warmup_frames=%d\n", preset.warmup_frames);
    fprintf(f, "measure_frames=%d\n", preset.measure_frames);
    fprintf(f, "min_duration_sec=%.1f\n", preset.min_duration_sec);

    fprintf(f, "\n[fillrate]\n");
    fprintf(f, "layers=%d\n", preset.fillrate.layers);

    fprintf(f, "\n[geometry]\n");
    fprintf(f, "grid_size=%d\n", preset.geometry.grid_size);

    fprintf(f, "\n[texturing]\n");
    fprintf(f, "tex_size=%d\n", preset.texturing.tex_size);
    fprintf(f, "layers=%d\n", preset.texturing.layers);

    fprintf(f, "\n[scene]\n");
    fprintf(f, "terrain_res=%d\n", preset.scene.terrain_res);
    fprintf(f, "terrain_tex=%d\n", preset.scene.terrain_tex);
    fprintf(f, "obj_tex=%d\n", preset.scene.obj_tex);
    fprintf(f, "sphere_segs=%d\n", preset.scene.sphere_segs);
    fprintf(f, "sphere_rings=%d\n", preset.scene.sphere_rings);
    fprintf(f, "cube_grid=%d\n", preset.scene.cube_grid);

    fprintf(f, "\n[drawcall]\n");
    fprintf(f, "mesh_count=%d\n", preset.drawcall.mesh_count);
    fprintf(f, "draws_per_frame=%d\n", preset.drawcall.draws_per_frame);

    fprintf(f, "\n[overdraw]\n");
    fprintf(f, "layers=%d\n", preset.overdraw.layers);
    fprintf(f, "alpha=%.2f\n", preset.overdraw.alpha);

    fprintf(f, "\n[texupload]\n");
    fprintf(f, "tex_size=%d\n", preset.texupload.tex_size);
    fprintf(f, "uploads_per_frame=%d\n", preset.texupload.uploads_per_frame);

    fprintf(f, "\n[statechange]\n");
    fprintf(f, "switches=%d\n", preset.statechange.switches);
    fprintf(f, "shader_count=%d\n", preset.statechange.shader_count);
    fprintf(f, "tex_count=%d\n", preset.statechange.tex_count);

    fprintf(f, "\n[vertex]\n");
    fprintf(f, "vertex_count=%d\n", preset.vertex.vertex_count);

    fprintf(f, "\n[shader_alu]\n");
    fprintf(f, "iterations=%d\n", preset.shader_alu.iterations);

    fclose(f);
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
    FILE* f = fopen(path, "r");
    if (!f) return false;

    // Start from Medium as base
    preset = getPreset(PRESET_MEDIUM);
    preset.name = "Custom";

    char line[512];
    std::string section;

    while (fgets(line, sizeof(line), f)) {
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

    fclose(f);
    return true;
}
