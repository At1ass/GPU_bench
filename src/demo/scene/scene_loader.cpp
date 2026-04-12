#include "demo/scene/scene_loader.h"
#include "platform/data_path.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================
// Helpers
// ============================================================

static const char* skipWhitespace(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static std::string trimRight(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static bool parseBool(const char* val) {
    return strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0;
}

static Vec3 parseVec3(const char* val) {
    Vec3 v;
    sscanf(val, "%f %f %f", &v.x, &v.y, &v.z);
    return v;
}

static float parseFloat(const char* val) {
    float f = 0.0f;
    sscanf(val, "%f", &f);
    return f;
}

static int parseInt(const char* val) {
    return atoi(val);
}

// ============================================================
// SceneLoader::load
// ============================================================

bool SceneLoader::load(const char* path,
                       std::vector<SceneObjectDef>& objects,
                       std::vector<SceneCameraKeypoint>& camera) {
    std::string content = readTextFile(path);
    if (content.empty()) {
        LOG_WRN("SceneLoader: failed to read '%s'", path);
        return false;
    }

    LOG_INF("SceneLoader: parsing '%s' (%d bytes)", path, static_cast<int>(content.size()));

    SceneObjectDef current_obj;
    SceneCameraKeypoint current_cam;
    bool in_block = false;
    bool is_camera = false;

    // Parse line by line
    const char* data = content.c_str();
    const char* end = data + content.size();

    while (data < end) {
        // Extract one line
        const char* line_end = data;
        while (line_end < end && *line_end != '\n') line_end++;

        size_t line_len = static_cast<size_t>(line_end - data);
        // Strip trailing \r
        if (line_len > 0 && data[line_len - 1] == '\r') line_len--;

        std::string line(data, line_len);
        data = (line_end < end) ? line_end + 1 : end;

        const char* s = skipWhitespace(line.c_str());

        // Skip comments and empty lines
        if (*s == '#' || *s == '\0') {
            // Empty line between blocks: finalize current block
            if (*s == '\0' && in_block) {
                if (is_camera) {
                    camera.push_back(current_cam);
                } else {
                    objects.push_back(current_obj);
                }
                in_block = false;
            }
            continue;
        }

        // Section header: [name]
        if (*s == '[') {
            // Finalize previous block if still open
            if (in_block) {
                if (is_camera) {
                    camera.push_back(current_cam);
                } else {
                    objects.push_back(current_obj);
                }
            }

            const char* bracket_end = strchr(s, ']');
            if (!bracket_end) {
                LOG_WRN("SceneLoader: malformed section header: %s", s);
                in_block = false;
                continue;
            }

            std::string name(s + 1, static_cast<size_t>(bracket_end - s - 1));

            current_obj = SceneObjectDef();
            current_cam = SceneCameraKeypoint();
            current_obj.name = name;
            in_block = true;
            is_camera = false;
            continue;
        }

        // Key = value
        if (!in_block) continue;

        const char* eq = strchr(s, '=');
        if (!eq) continue;

        std::string key(s, static_cast<size_t>(eq - s));
        key = trimRight(key);
        const char* val = skipWhitespace(eq + 1);
        std::string val_str = trimRight(std::string(val));
        val = val_str.c_str();

        // Check for camera type
        if (key == "type" && strcmp(val, "camera") == 0) {
            is_camera = true;
            continue;
        }

        // Camera keys
        if (is_camera) {
            if (key == "position") current_cam.position = parseVec3(val);
            else if (key == "target") current_cam.target = parseVec3(val);
            continue;
        }

        // Object keys
        if (key == "mesh") { current_obj.mesh = val; }
        else if (key == "pos") { current_obj.pos = parseVec3(val); }
        else if (key == "rotation") { current_obj.rotation = parseVec3(val); }
        else if (key == "scale") {
            // Support both "scale = 1.0" (uniform) and "scale = 1.0 2.0 3.0"
            float sx = 1.0f, sy = 1.0f, sz = 1.0f;
            int n = sscanf(val, "%f %f %f", &sx, &sy, &sz);
            if (n == 1) { sy = sx; sz = sx; }
            current_obj.scale = Vec3(sx, sy, sz);
        }
        else if (key == "material") { current_obj.material = val; }
        else if (key == "tint") {
            current_obj.tint = parseVec3(val);
            current_obj.has_tint = true;
        }
        else if (key == "shader_type") {
            if (strcmp(val, "island") == 0) current_obj.shader_type = MaterialType::Island;
            else current_obj.shader_type = MaterialType::Model;
        }
        else if (key == "fur") { current_obj.fur = parseBool(val); }
        else if (key == "vertex_wind") { current_obj.vertex_wind = parseBool(val); }
        else if (key == "is_water") { current_obj.is_water = parseBool(val); }
        else if (key == "tessellated") {
            if (strcmp(val, "auto") == 0) {
                current_obj.tessellated_auto = true;
            } else {
                current_obj.tessellated = parseBool(val);
            }
        }
        else if (key == "two_sided") { current_obj.two_sided = parseBool(val); }
        else if (key == "snap_to_terrain") { current_obj.snap_to_terrain = parseBool(val); }
        else if (key == "snap_to_terrain_offset") { current_obj.snap_to_terrain_offset = parseFloat(val); }
        else if (key == "snap_to_terrain_pair") { current_obj.snap_to_terrain_pair = val; }
        else if (key == "arch_on_columns") { current_obj.arch_on_columns = parseBool(val); }
        else if (key == "roughness") { current_obj.roughness = parseFloat(val); }
        else if (key == "noise_scale") { current_obj.noise_scale = parseFloat(val); }
        else if (key == "warp_strength") { current_obj.warp_strength = parseFloat(val); }
        else if (key == "bounds_radius") {
            if (strcmp(val, "model") == 0) {
                current_obj.bounds_radius_is_model = true;
            } else {
                current_obj.bounds_radius = parseFloat(val);
            }
        }
        else if (key == "require_shadows") { current_obj.require_shadows = parseBool(val); }
        else if (key == "require_ssr") { current_obj.require_ssr = parseBool(val); }
        else if (key == "require_point_lights") { current_obj.require_point_lights = parseInt(val); }
        else if (key == "max_instanced_grass") { current_obj.max_instanced_grass = parseInt(val); }
        else if (key == "fallback_for_pond") { current_obj.fallback_for_pond = parseBool(val); }
        else {
            LOG_DBG("SceneLoader: unknown key '%s' in [%s]", key.c_str(), current_obj.name.c_str());
        }
    }

    // Finalize last block
    if (in_block) {
        if (is_camera) {
            camera.push_back(current_cam);
        } else {
            objects.push_back(current_obj);
        }
    }

    int obj_count = static_cast<int>(objects.size());
    int cam_count = static_cast<int>(camera.size());
    LOG_INF("SceneLoader: loaded %d objects, %d camera keypoints", obj_count, cam_count);
    return true;
}
