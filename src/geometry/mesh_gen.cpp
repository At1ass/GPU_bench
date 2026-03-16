#include "geometry/mesh_gen.h"
#include <cmath>

static const float PI = 3.14159265358979323846f;

MeshData MeshGen::quad() {
    MeshData m;
    // Vertex layout: pos(xyz), normal(xyz), uv(xy)
    // For 2D shaders, only xy of pos and uv are used
    Vertex v;
    v.normal = Vec3(0, 0, 1);

    v.pos = Vec3(-1, -1, 0); v.uv = Vec2(0, 0); m.vertices.push_back(v);
    v.pos = Vec3( 1, -1, 0); v.uv = Vec2(1, 0); m.vertices.push_back(v);
    v.pos = Vec3( 1,  1, 0); v.uv = Vec2(1, 1); m.vertices.push_back(v);
    v.pos = Vec3(-1,  1, 0); v.uv = Vec2(0, 1); m.vertices.push_back(v);

    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

MeshData MeshGen::cube() {
    MeshData m;
    // 6 faces, 4 vertices each, with normals
    struct Face { Vec3 n; Vec3 u; Vec3 v; }; // normal, right, up
    Face faces[6] = {
        { Vec3( 0, 0, 1), Vec3( 1, 0, 0), Vec3(0, 1, 0) },  // front
        { Vec3( 0, 0,-1), Vec3(-1, 0, 0), Vec3(0, 1, 0) },  // back
        { Vec3( 1, 0, 0), Vec3( 0, 0,-1), Vec3(0, 1, 0) },  // right
        { Vec3(-1, 0, 0), Vec3( 0, 0, 1), Vec3(0, 1, 0) },  // left
        { Vec3( 0, 1, 0), Vec3( 1, 0, 0), Vec3(0, 0,-1) },  // top
        { Vec3( 0,-1, 0), Vec3( 1, 0, 0), Vec3(0, 0, 1) },  // bottom
    };

    for (int f = 0; f < 6; f++) {
        Vec3 n = faces[f].n;
        Vec3 r = faces[f].u;
        Vec3 u = faces[f].v;
        unsigned int base = (unsigned int)m.vertices.size();

        Vertex vt;
        vt.normal = n;

        vt.pos = (n + r * (-1) + u * (-1)) * 0.5f; vt.uv = Vec2(0, 0); m.vertices.push_back(vt);
        vt.pos = (n + r *   1  + u * (-1)) * 0.5f; vt.uv = Vec2(1, 0); m.vertices.push_back(vt);
        vt.pos = (n + r *   1  + u *   1 ) * 0.5f; vt.uv = Vec2(1, 1); m.vertices.push_back(vt);
        vt.pos = (n + r * (-1) + u *   1 ) * 0.5f; vt.uv = Vec2(0, 1); m.vertices.push_back(vt);

        m.indices.push_back(base + 0);
        m.indices.push_back(base + 1);
        m.indices.push_back(base + 2);
        m.indices.push_back(base + 0);
        m.indices.push_back(base + 2);
        m.indices.push_back(base + 3);
    }
    return m;
}

MeshData MeshGen::sphere(int segments, int rings) {
    MeshData m;
    for (int r = 0; r <= rings; r++) {
        float v = static_cast<float>(r) / rings;
        float phi = v * PI;
        for (int s = 0; s <= segments; s++) {
            float u = static_cast<float>(s) / segments;
            float theta = u * 2.0f * PI;

            Vec3 pos(
                sinf(phi) * cosf(theta),
                cosf(phi),
                sinf(phi) * sinf(theta)
            );
            Vertex vt;
            vt.pos = pos;
            vt.normal = pos; // unit sphere: normal = position
            vt.uv = Vec2(u, v);
            m.vertices.push_back(vt);
        }
    }
    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segments; s++) {
            unsigned int a = r * (segments + 1) + s;
            unsigned int b = a + segments + 1;
            m.indices.push_back(a);
            m.indices.push_back(b);
            m.indices.push_back(a + 1);
            m.indices.push_back(a + 1);
            m.indices.push_back(b);
            m.indices.push_back(b + 1);
        }
    }
    return m;
}

MeshData MeshGen::torus(int ring_segments, int tube_segments, float ring_radius, float tube_radius) {
    MeshData m;
    for (int i = 0; i <= ring_segments; i++) {
        float u = static_cast<float>(i) / ring_segments;
        float theta = u * 2.0f * PI;
        float ct = cosf(theta), st = sinf(theta);
        for (int j = 0; j <= tube_segments; j++) {
            float v = static_cast<float>(j) / tube_segments;
            float phi = v * 2.0f * PI;
            float cp = cosf(phi), sp = sinf(phi);

            float r = ring_radius + tube_radius * cp;
            Vertex vt;
            vt.pos = Vec3(r * ct, tube_radius * sp, r * st);
            vt.normal = Vec3(cp * ct, sp, cp * st);
            vt.uv = Vec2(u, v);
            m.vertices.push_back(vt);
        }
    }
    for (int i = 0; i < ring_segments; i++) {
        for (int j = 0; j < tube_segments; j++) {
            unsigned int a = i * (tube_segments + 1) + j;
            unsigned int b = a + tube_segments + 1;
            m.indices.push_back(a);
            m.indices.push_back(b);
            m.indices.push_back(a + 1);
            m.indices.push_back(a + 1);
            m.indices.push_back(b);
            m.indices.push_back(b + 1);
        }
    }
    return m;
}

MeshData MeshGen::terrain(float size, int resolution) {
    MeshData m;
    float half = size * 0.5f;
    float step = size / (resolution - 1);

    for (int z = 0; z < resolution; z++) {
        for (int x = 0; x < resolution; x++) {
            float wx = -half + x * step;
            float wz = -half + z * step;
            // Simple procedural heightmap
            float h = sinf(wx * 0.5f) * cosf(wz * 0.5f) * 2.0f
                    + sinf(wx * 1.3f + 0.7f) * 0.5f
                    + cosf(wz * 0.9f + 1.2f) * 0.8f;

            Vertex vt;
            vt.pos = Vec3(wx, h, wz);
            vt.uv = Vec2(static_cast<float>(x) / (resolution - 1), static_cast<float>(z) / (resolution - 1));
            // Normal computed below
            vt.normal = Vec3(0, 1, 0);
            m.vertices.push_back(vt);
        }
    }

    // Compute normals from adjacent vertices
    for (int z = 0; z < resolution; z++) {
        for (int x = 0; x < resolution; x++) {
            int idx = z * resolution + x;
            Vec3 pos = m.vertices[idx].pos;

            Vec3 dx, dz;
            if (x + 1 < resolution)
                dx = m.vertices[idx + 1].pos - pos;
            else if (x > 0)
                dx = pos - m.vertices[idx - 1].pos;

            if (z + 1 < resolution)
                dz = m.vertices[idx + resolution].pos - pos;
            else if (z > 0)
                dz = pos - m.vertices[idx - resolution].pos;

            m.vertices[idx].normal = Vec3::cross(dz, dx).normalized();
        }
    }

    // Indices
    for (int z = 0; z < resolution - 1; z++) {
        for (int x = 0; x < resolution - 1; x++) {
            unsigned int tl = z * resolution + x;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + resolution;
            unsigned int br = bl + 1;
            m.indices.push_back(tl);
            m.indices.push_back(bl);
            m.indices.push_back(tr);
            m.indices.push_back(tr);
            m.indices.push_back(bl);
            m.indices.push_back(br);
        }
    }
    return m;
}

MeshData MeshGen::cone(int segments, float height, float radius) {
    MeshData m;
    // Apex vertex
    Vertex apex;
    apex.pos = Vec3(0, height, 0);
    apex.normal = Vec3(0, 1, 0);
    apex.uv = Vec2(0.5f, 1.0f);
    m.vertices.push_back(apex);

    // Base ring + center
    float slope = radius / height;
    float ny = 1.0f / sqrtf(1.0f + slope * slope);
    float nr = slope * ny;
    for (int i = 0; i <= segments; i++) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * PI;
        float cs = cosf(theta), sn = sinf(theta);

        Vertex vt;
        vt.pos = Vec3(cs * radius, 0, sn * radius);
        vt.normal = Vec3(cs * nr, ny, sn * nr);
        vt.uv = Vec2(u, 0);
        m.vertices.push_back(vt);
    }

    // Side triangles
    for (int i = 0; i < segments; i++) {
        m.indices.push_back(0);
        m.indices.push_back(static_cast<unsigned int>(i + 1));
        m.indices.push_back(static_cast<unsigned int>(i + 2));
    }

    // Base cap
    unsigned int center_idx = static_cast<unsigned int>(m.vertices.size());
    Vertex center;
    center.pos = Vec3(0, 0, 0);
    center.normal = Vec3(0, -1, 0);
    center.uv = Vec2(0.5f, 0.5f);
    m.vertices.push_back(center);

    for (int i = 0; i < segments; i++) {
        m.indices.push_back(center_idx);
        m.indices.push_back(static_cast<unsigned int>(i + 2));
        m.indices.push_back(static_cast<unsigned int>(i + 1));
    }

    return m;
}

MeshData MeshGen::cylinder(int segments, float height, float radius) {
    MeshData m;
    float half_h = height * 0.5f;

    // Side vertices: top and bottom rings
    for (int i = 0; i <= segments; i++) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * PI;
        float cs = cosf(theta), sn = sinf(theta);
        Vec3 n(cs, 0, sn);

        Vertex top;
        top.pos = Vec3(cs * radius, half_h, sn * radius);
        top.normal = n;
        top.uv = Vec2(u, 1);
        m.vertices.push_back(top);

        Vertex bot;
        bot.pos = Vec3(cs * radius, -half_h, sn * radius);
        bot.normal = n;
        bot.uv = Vec2(u, 0);
        m.vertices.push_back(bot);
    }

    // Side quads
    for (int i = 0; i < segments; i++) {
        unsigned int t0 = i * 2;
        unsigned int b0 = t0 + 1;
        unsigned int t1 = t0 + 2;
        unsigned int b1 = t0 + 3;
        m.indices.push_back(t0); m.indices.push_back(b0); m.indices.push_back(t1);
        m.indices.push_back(t1); m.indices.push_back(b0); m.indices.push_back(b1);
    }

    // Top cap
    unsigned int top_center = static_cast<unsigned int>(m.vertices.size());
    Vertex tc; tc.pos = Vec3(0, half_h, 0); tc.normal = Vec3(0, 1, 0); tc.uv = Vec2(0.5f, 0.5f);
    m.vertices.push_back(tc);
    for (int i = 0; i < segments; i++) {
        m.indices.push_back(top_center);
        m.indices.push_back(static_cast<unsigned int>(i * 2));
        m.indices.push_back(static_cast<unsigned int>((i + 1) * 2));
    }

    // Bottom cap
    unsigned int bot_center = static_cast<unsigned int>(m.vertices.size());
    Vertex bc; bc.pos = Vec3(0, -half_h, 0); bc.normal = Vec3(0, -1, 0); bc.uv = Vec2(0.5f, 0.5f);
    m.vertices.push_back(bc);
    for (int i = 0; i < segments; i++) {
        m.indices.push_back(bot_center);
        m.indices.push_back(static_cast<unsigned int>((i + 1) * 2 + 1));
        m.indices.push_back(static_cast<unsigned int>(i * 2 + 1));
    }

    return m;
}

void MeshGen::appendTransformed(MeshData& dst, const MeshData& src, const Mat4& mat) {
    unsigned int base = static_cast<unsigned int>(dst.vertices.size());
    for (const auto& v : src.vertices) {
        Vertex vt;
        vt.pos = mat.transformPoint(v.pos);
        vt.normal = mat.transformNormal(v.normal);
        vt.uv = v.uv;
        dst.vertices.push_back(vt);
    }
    for (unsigned int idx : src.indices) {
        dst.indices.push_back(idx + base);
    }
}

// Simple hash for per-vertex noise
static float hash_noise(float x, float y, float z, unsigned int seed) {
    unsigned int h = seed;
    h ^= static_cast<unsigned int>(x * 73856093.0f);
    h ^= static_cast<unsigned int>(y * 19349663.0f);
    h ^= static_cast<unsigned int>(z * 83492791.0f);
    h = (h * 1664525u + 1013904223u);
    return (static_cast<float>(h & 0xFFFF) / 65535.0f) * 2.0f - 1.0f;
}

MeshData MeshGen::rock(int segments, int rings, float radius, unsigned int seed) {
    // Start with a sphere, displace vertices with noise
    MeshData m = sphere(segments, rings);
    for (auto& v : m.vertices) {
        float noise = hash_noise(v.pos.x * 3.7f, v.pos.y * 3.7f, v.pos.z * 3.7f, seed) * 0.3f
                    + hash_noise(v.pos.x * 7.1f, v.pos.y * 7.1f, v.pos.z * 7.1f, seed + 1) * 0.15f;
        float displaced_r = radius * (1.0f + noise);
        v.pos = v.pos * displaced_r;
    }
    // Recompute normals from triangle faces
    // Zero all normals first
    for (auto& v : m.vertices) v.normal = Vec3(0, 0, 0);
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        Vec3 a = m.vertices[m.indices[i]].pos;
        Vec3 b = m.vertices[m.indices[i+1]].pos;
        Vec3 c = m.vertices[m.indices[i+2]].pos;
        Vec3 n = Vec3::cross(b - a, c - a);
        m.vertices[m.indices[i]].normal = m.vertices[m.indices[i]].normal + n;
        m.vertices[m.indices[i+1]].normal = m.vertices[m.indices[i+1]].normal + n;
        m.vertices[m.indices[i+2]].normal = m.vertices[m.indices[i+2]].normal + n;
    }
    for (auto& v : m.vertices) v.normal = v.normal.normalized();
    return m;
}

MeshData MeshGen::building_detailed(float width, float height, float depth, unsigned int seed) {
    MeshData m;
    MeshData box = cube();

    // Main body
    Mat4 body = Mat4::translate(0, height * 0.5f, 0) * Mat4::scale(width * 0.5f, height * 0.5f, depth * 0.5f);
    appendTransformed(m, box, body);

    // Top ledge (wider rim near top)
    float ledge_h = height * 0.05f;
    float ledge_ext = 0.15f;
    Mat4 ledge = Mat4::translate(0, height * 0.85f, 0)
               * Mat4::scale(width * 0.5f + ledge_ext, ledge_h, depth * 0.5f + ledge_ext);
    appendTransformed(m, box, ledge);

    // Base/foundation (slightly wider)
    float base_h = height * 0.08f;
    Mat4 base_mat = Mat4::translate(0, base_h * 0.5f, 0)
                  * Mat4::scale(width * 0.55f, base_h * 0.5f, depth * 0.55f);
    appendTransformed(m, box, base_mat);

    // Simple LCG for window placement
    unsigned int ws = seed;
    auto wnext = [&]() -> float {
        ws = ws * 1664525u + 1013904223u;
        return static_cast<float>(ws & 0xFFFF) / 65535.0f;
    };

    // Window columns on front/back faces
    int num_floors = static_cast<int>(height / 2.5f);
    if (num_floors < 1) num_floors = 1;
    int windows_per_floor = static_cast<int>(width / 1.5f);
    if (windows_per_floor < 1) windows_per_floor = 1;

    float win_w = 0.2f;
    float win_h = 0.3f;
    float win_d = 0.06f;

    for (int floor_i = 0; floor_i < num_floors && floor_i < 8; floor_i++) {
        float y = height * 0.15f + static_cast<float>(floor_i) / num_floors * height * 0.7f;
        for (int wi = 0; wi < windows_per_floor && wi < 6; wi++) {
            float x = -width * 0.35f + static_cast<float>(wi) / (windows_per_floor) * width * 0.7f;
            if (wnext() > 0.3f) {
                // Front face window protrusion
                Mat4 wm = Mat4::translate(x, y, depth * 0.5f + win_d)
                        * Mat4::scale(win_w, win_h, win_d);
                appendTransformed(m, box, wm);
            }
            if (wnext() > 0.3f) {
                // Back face
                Mat4 wm = Mat4::translate(x, y, -depth * 0.5f - win_d)
                        * Mat4::scale(win_w, win_h, win_d);
                appendTransformed(m, box, wm);
            }
        }
    }

    // Roof element: flat box or small tower
    if ((seed & 3) == 0) {
        // Small tower on top
        float tw = width * 0.25f;
        float th = height * 0.2f;
        Mat4 tower = Mat4::translate(0, height + th * 0.5f, 0) * Mat4::scale(tw * 0.5f, th * 0.5f, tw * 0.5f);
        appendTransformed(m, box, tower);
    } else {
        // Flat roof rim
        Mat4 roof_rim = Mat4::translate(0, height, 0)
                      * Mat4::scale(width * 0.52f, 0.1f, depth * 0.52f);
        appendTransformed(m, box, roof_rim);
    }

    return m;
}

MeshData MeshGen::tree_branching(float height, float trunk_radius, int max_depth, unsigned int seed) {
    MeshData m;
    MeshData cyl = cylinder(8, 1.0f, 1.0f);
    MeshData leaf_sphere = sphere(6, 4);

    struct Branch {
        Vec3 base;
        float height;
        float radius;
        float angle_y;
        float angle_x;
        int depth;
        unsigned int seed;
    };

    // Iterative branch stack (avoids recursion)
    std::vector<Branch> stack;
    Branch root;
    root.base = Vec3(0, 0, 0);
    root.height = height;
    root.radius = trunk_radius;
    root.angle_y = 0;
    root.angle_x = 0;
    root.depth = 0;
    root.seed = seed;
    stack.push_back(root);

    while (!stack.empty()) {
        Branch br = stack.back();
        stack.pop_back();

        // Draw cylinder for this branch
        Mat4 branch_mat = Mat4::translate(br.base.x, br.base.y, br.base.z)
                        * Mat4::rotateY(br.angle_y)
                        * Mat4::rotateX(br.angle_x)
                        * Mat4::scale(br.radius, br.height, br.radius)
                        * Mat4::translate(0, 0.5f, 0);
        appendTransformed(m, cyl, branch_mat);

        // Compute end point of branch
        Vec3 dir(0, br.height, 0);
        Mat4 rot = Mat4::rotateY(br.angle_y) * Mat4::rotateX(br.angle_x);
        Vec3 end = br.base + rot.transformPoint(dir);

        if (br.depth >= max_depth) {
            // Leaf canopy at tip
            float leaf_r = br.height * 0.8f;
            Mat4 leaf_mat = Mat4::translate(end.x, end.y, end.z)
                          * Mat4::scale(leaf_r, leaf_r * 0.7f, leaf_r);
            appendTransformed(m, leaf_sphere, leaf_mat);
            continue;
        }

        // Spawn 2-3 child branches
        unsigned int cs = br.seed;
        auto cnext = [&]() -> float {
            cs = cs * 1664525u + 1013904223u;
            return static_cast<float>(cs & 0xFFFF) / 65535.0f;
        };

        int children = 2 + static_cast<int>(cnext() * 1.5f);
        for (int i = 0; i < children && i < 3; i++) {
            Branch child;
            child.base = end;
            child.height = br.height * (0.55f + cnext() * 0.15f);
            child.radius = br.radius * (0.5f + cnext() * 0.2f);
            child.angle_y = br.angle_y + (cnext() - 0.5f) * 120.0f;
            child.angle_x = br.angle_x + 20.0f + cnext() * 30.0f;
            child.depth = br.depth + 1;
            child.seed = cs + static_cast<unsigned int>(i * 7919);
            stack.push_back(child);
        }
    }

    return m;
}

MeshData MeshGen::arch(float width, float height, float thickness, int segments) {
    MeshData m;
    MeshData box = cube();

    float pillar_w = thickness;
    float pillar_h = height * 0.6f;
    float half_w = width * 0.5f;

    // Left pillar
    Mat4 left = Mat4::translate(-half_w + pillar_w * 0.5f, pillar_h * 0.5f, 0)
              * Mat4::scale(pillar_w * 0.5f, pillar_h * 0.5f, thickness * 0.5f);
    appendTransformed(m, box, left);

    // Right pillar
    Mat4 right = Mat4::translate(half_w - pillar_w * 0.5f, pillar_h * 0.5f, 0)
               * Mat4::scale(pillar_w * 0.5f, pillar_h * 0.5f, thickness * 0.5f);
    appendTransformed(m, box, right);

    // Arch (half-circle of boxes)
    float arch_radius = half_w - pillar_w;
    float arch_center_y = pillar_h;
    for (int i = 0; i < segments; i++) {
        float a0 = PI * static_cast<float>(i) / segments;
        float a1 = PI * static_cast<float>(i + 1) / segments;
        float mid_a = (a0 + a1) * 0.5f;
        float seg_len = arch_radius * (a1 - a0);

        float cx = -cosf(mid_a) * arch_radius;
        float cy = arch_center_y + sinf(mid_a) * arch_radius;
        float angle_deg = mid_a * 180.0f / PI - 90.0f;

        Mat4 seg_mat = Mat4::translate(cx, cy, 0)
                     * Mat4::rotateZ(angle_deg)
                     * Mat4::scale(seg_len * 0.55f, thickness * 0.5f, thickness * 0.5f);
        appendTransformed(m, box, seg_mat);
    }

    return m;
}

MeshData MeshGen::cubeGrid(int count) {
    MeshData result;
    MeshData unit = cube();
    float spacing = 1.5f;
    float offset = -(count - 1) * spacing * 0.5f;

    for (int iz = 0; iz < count; iz++) {
        for (int iy = 0; iy < count; iy++) {
            for (int ix = 0; ix < count; ix++) {
                Vec3 pos(
                    offset + ix * spacing,
                    offset + iy * spacing,
                    offset + iz * spacing
                );
                unsigned int base = (unsigned int)result.vertices.size();
                for (auto vt : unit.vertices) {
                    vt.pos = vt.pos + pos;
                    result.vertices.push_back(vt);
                }
                for (unsigned int idx : unit.indices) {
                    result.indices.push_back(idx + base);
                }
            }
        }
    }
    return result;
}

// ============================================================
// Frustum (truncated cone)
// ============================================================

MeshData MeshGen::frustum(int segments, float height, float r_bottom, float r_top) {
    MeshData m;
    float half_h = height * 0.5f;
    float slope_len = sqrtf((r_bottom - r_top) * (r_bottom - r_top) + height * height);
    float ny = (r_bottom - r_top) / slope_len;
    float nr = height / slope_len;

    for (int i = 0; i <= segments; i++) {
        float u = static_cast<float>(i) / segments;
        float theta = u * 2.0f * PI;
        float cs = cosf(theta), sn = sinf(theta);
        Vec3 n(cs * nr, ny, sn * nr);

        Vertex top;
        top.pos = Vec3(cs * r_top, half_h, sn * r_top);
        top.normal = n;
        top.uv = Vec2(u, 1);
        m.vertices.push_back(top);

        Vertex bot;
        bot.pos = Vec3(cs * r_bottom, -half_h, sn * r_bottom);
        bot.normal = n;
        bot.uv = Vec2(u, 0);
        m.vertices.push_back(bot);
    }
    for (int i = 0; i < segments; i++) {
        unsigned int t0 = i * 2, b0 = t0 + 1, t1 = t0 + 2, b1 = t0 + 3;
        m.indices.push_back(t0); m.indices.push_back(b0); m.indices.push_back(t1);
        m.indices.push_back(t1); m.indices.push_back(b0); m.indices.push_back(b1);
    }
    // Top cap
    unsigned int tc = static_cast<unsigned int>(m.vertices.size());
    Vertex tcv; tcv.pos = Vec3(0, half_h, 0); tcv.normal = Vec3(0, 1, 0); tcv.uv = Vec2(0.5f, 0.5f);
    m.vertices.push_back(tcv);
    for (int i = 0; i < segments; i++) {
        m.indices.push_back(tc);
        m.indices.push_back(static_cast<unsigned int>(i * 2));
        m.indices.push_back(static_cast<unsigned int>((i + 1) * 2));
    }
    // Bottom cap
    unsigned int bc = static_cast<unsigned int>(m.vertices.size());
    Vertex bcv; bcv.pos = Vec3(0, -half_h, 0); bcv.normal = Vec3(0, -1, 0); bcv.uv = Vec2(0.5f, 0.5f);
    m.vertices.push_back(bcv);
    for (int i = 0; i < segments; i++) {
        m.indices.push_back(bc);
        m.indices.push_back(static_cast<unsigned int>((i + 1) * 2 + 1));
        m.indices.push_back(static_cast<unsigned int>(i * 2 + 1));
    }
    return m;
}

// ============================================================
// Recompute normals (smooth, area-weighted)
// ============================================================

void MeshGen::recomputeNormals(MeshData& m) {
    for (auto& v : m.vertices) v.normal = Vec3(0, 0, 0);
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        Vec3 a = m.vertices[m.indices[i]].pos;
        Vec3 b = m.vertices[m.indices[i+1]].pos;
        Vec3 c = m.vertices[m.indices[i+2]].pos;
        Vec3 n = Vec3::cross(b - a, c - a); // area-weighted
        m.vertices[m.indices[i]].normal = m.vertices[m.indices[i]].normal + n;
        m.vertices[m.indices[i+1]].normal = m.vertices[m.indices[i+1]].normal + n;
        m.vertices[m.indices[i+2]].normal = m.vertices[m.indices[i+2]].normal + n;
    }
    for (auto& v : m.vertices) v.normal = v.normal.normalized();
}

// ============================================================
// Coherent 3D value noise + FBM
// ============================================================

static float smoothstep_f(float t) { return t * t * (3.0f - 2.0f * t); }

static float hash_grid(int ix, int iy, int iz, unsigned int seed) {
    unsigned int h = seed;
    h ^= static_cast<unsigned int>(ix) * 73856093u;
    h ^= static_cast<unsigned int>(iy) * 19349663u;
    h ^= static_cast<unsigned int>(iz) * 83492791u;
    h = (h * 1664525u + 1013904223u);
    return static_cast<float>(h & 0xFFFF) / 65535.0f;
}

static float value_noise_3d(float x, float y, float z, unsigned int seed) {
    int ix = static_cast<int>(floorf(x)), iy = static_cast<int>(floorf(y)), iz = static_cast<int>(floorf(z));
    float fx = x - ix, fy = y - iy, fz = z - iz;
    float sx = smoothstep_f(fx), sy = smoothstep_f(fy), sz = smoothstep_f(fz);

    float c000 = hash_grid(ix, iy, iz, seed),     c100 = hash_grid(ix+1, iy, iz, seed);
    float c010 = hash_grid(ix, iy+1, iz, seed),   c110 = hash_grid(ix+1, iy+1, iz, seed);
    float c001 = hash_grid(ix, iy, iz+1, seed),   c101 = hash_grid(ix+1, iy, iz+1, seed);
    float c011 = hash_grid(ix, iy+1, iz+1, seed), c111 = hash_grid(ix+1, iy+1, iz+1, seed);

    float x00 = c000 + sx * (c100 - c000), x10 = c010 + sx * (c110 - c010);
    float x01 = c001 + sx * (c101 - c001), x11 = c011 + sx * (c111 - c011);
    float y0 = x00 + sy * (x10 - x00), y1 = x01 + sy * (x11 - x01);
    return y0 + sz * (y1 - y0);
}

static float fbm_3d(float x, float y, float z, unsigned int seed, int octaves) {
    float val = 0, amp = 1.0f, freq = 1.0f, total_amp = 0;
    for (int i = 0; i < octaves; i++) {
        val += value_noise_3d(x * freq, y * freq, z * freq, seed + static_cast<unsigned int>(i) * 7919) * amp;
        total_amp += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return val / total_amp;
}

// Deterministic LCG for mesh generators
struct MeshLCG {
    unsigned int state;
    MeshLCG(unsigned int seed) : state(seed) {}
    unsigned int next() { state = state * 1664525u + 1013904223u; return state; }
    float flt() { return static_cast<float>(next() & 0xFFFF) / 65535.0f; }
    float range(float lo, float hi) { return lo + flt() * (hi - lo); }
    int irange(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<unsigned int>(hi - lo + 1)); }
};

// ============================================================
// Rock V2: coherent noise, directional deformation, stratification
// ============================================================

MeshData MeshGen::rock_v2(int detail, float radius, unsigned int seed) {
    int segments = detail * 2;
    int rings = detail;
    MeshData m = sphere(segments, rings);

    MeshLCG rng(seed);
    // Rock type: 0=boulder, 1=standing stone, 2=normal, 3=slab
    int rock_type = seed & 3;
    float sy = 1.0f;
    if (rock_type == 0) sy = 0.55f + rng.flt() * 0.15f;       // flat boulder
    else if (rock_type == 1) sy = 1.5f + rng.flt() * 0.5f;     // standing stone
    else if (rock_type == 3) sy = 0.3f + rng.flt() * 0.15f;    // slab

    for (auto& v : m.vertices) {
        // Apply directional deformation first
        Vec3 p = Vec3(v.pos.x, v.pos.y * sy, v.pos.z);

        // Coherent noise displacement (3 octaves FBM)
        float n = fbm_3d(p.x * 2.5f, p.y * 2.5f, p.z * 2.5f, seed, 3) * 2.0f - 1.0f;

        // Stratification: horizontal layers
        float strat = sinf(p.y * 6.0f) * 0.12f;

        // Erosion pits
        float erosion = fbm_3d(p.x * 8.0f, p.y * 8.0f, p.z * 8.0f, seed + 100, 2);
        float pit = (erosion > 0.7f) ? (erosion - 0.7f) * 1.5f : 0.0f;

        float displacement = n * 0.25f + strat - pit;
        float r = radius * (1.0f + displacement);
        v.pos = p.normalized() * r;
    }

    recomputeNormals(m);
    return m;
}

// ============================================================
// Building V2: multi-story, varied facades, proper rooftops
// ============================================================

MeshData MeshGen::building_v2(float width, float height, float depth, unsigned int seed) {
    MeshData m;
    MeshData box = cube();
    MeshData cyl = cylinder(8, 1.0f, 1.0f);
    // Fix: cube is ±0.5, but transform code uses scale as half-extent (expects ±1).
    // Double cube vertices so scale(s) gives half-extent s, total 2s.
    for (size_t i = 0; i < box.vertices.size(); i++) {
        box.vertices[i].pos = box.vertices[i].pos * 2.0f;
    }
    // Cylinder: Y is ±0.5 (same issue), but radius=1.0 is already correct.
    for (size_t i = 0; i < cyl.vertices.size(); i++) {
        cyl.vertices[i].pos.y *= 2.0f;
    }
    MeshLCG rng(seed);

    // Roof type: 0=flat parapet, 1=gabled, 2=tower, 3=dome
    int roof_type = seed & 3;
    float floor_h = rng.range(2.5f, 3.5f);
    int num_floors = static_cast<int>(height / floor_h);
    if (num_floors < 1) num_floors = 1;

    // Main body: always a single solid block spanning full width
    Mat4 body = Mat4::translate(0, height * 0.5f, 0)
              * Mat4::scale(width * 0.5f, height * 0.5f, depth * 0.5f);
    appendTransformed(m, box, body);

    // Optional setback (upper section, 50% chance for taller buildings)
    int num_sections = 1 + static_cast<int>(rng.flt() * 2.0f);
    if (num_sections > 1 && height > 6.0f) {
        float step_w = width * (0.5f + rng.flt() * 0.3f);
        float step_h = height * (0.2f + rng.flt() * 0.15f);
        float step_d = depth * (0.5f + rng.flt() * 0.3f);
        float step_x = rng.range(-width * 0.1f, width * 0.1f);
        Mat4 setback = Mat4::translate(step_x, height + step_h * 0.5f, 0)
                     * Mat4::scale(step_w * 0.5f, step_h * 0.5f, step_d * 0.5f);
        appendTransformed(m, box, setback);
    }

    // Foundation (wider base)
    float base_h = height * 0.06f;
    Mat4 base_mat = Mat4::translate(0, base_h * 0.5f, 0)
                  * Mat4::scale(width * 0.54f, base_h * 0.5f, depth * 0.54f);
    appendTransformed(m, box, base_mat);

    // Floor cornices (horizontal bands)
    for (int fi = 1; fi <= num_floors && fi < 10; fi++) {
        float cy = fi * floor_h;
        if (cy > height * 0.9f) break;
        Mat4 cornice = Mat4::translate(0, cy, 0)
                     * Mat4::scale(width * 0.52f, 0.04f, depth * 0.52f);
        appendTransformed(m, box, cornice);
    }

    // Windows (recessed niches on front/back/side faces)
    int win_per_floor = static_cast<int>(width / 1.8f);
    if (win_per_floor < 1) win_per_floor = 1;
    if (win_per_floor > 5) win_per_floor = 5;
    float win_w = width * 0.12f;
    float win_h = floor_h * 0.45f;
    float win_recess = 0.08f;

    for (int fi = 0; fi < num_floors && fi < 8; fi++) {
        float fy = base_h + fi * floor_h + floor_h * 0.55f;
        if (fy + win_h > height * 0.85f) break;

        for (int wi = 0; wi < win_per_floor; wi++) {
            float wx = -width * 0.38f + static_cast<float>(wi) / (win_per_floor) * width * 0.76f;

            // Window frame (front face) — protruding frame
            Mat4 frame_f = Mat4::translate(wx, fy, depth * 0.5f + win_recess * 0.5f)
                         * Mat4::scale(win_w * 0.55f, win_h * 0.55f, win_recess * 0.5f);
            appendTransformed(m, box, frame_f);

            // Window sill (front face)
            Mat4 sill_f = Mat4::translate(wx, fy - win_h * 0.5f, depth * 0.5f + win_recess)
                        * Mat4::scale(win_w * 0.65f, 0.03f, win_recess * 0.8f);
            appendTransformed(m, box, sill_f);

            // Back face windows (less detail)
            if (rng.flt() > 0.3f) {
                Mat4 frame_b = Mat4::translate(wx, fy, -depth * 0.5f - win_recess * 0.5f)
                             * Mat4::scale(win_w * 0.5f, win_h * 0.5f, win_recess * 0.5f);
                appendTransformed(m, box, frame_b);
            }
        }

        // Side face windows (1-2 per floor)
        if (rng.flt() > 0.4f) {
            float sz = rng.range(-depth * 0.2f, depth * 0.2f);
            Mat4 side_w = Mat4::translate(width * 0.5f + win_recess * 0.5f, fy, sz)
                        * Mat4::scale(win_recess * 0.5f, win_h * 0.45f, win_w * 0.45f);
            appendTransformed(m, box, side_w);
        }
    }

    // Entrance (ground floor)
    float door_w = width * 0.15f;
    float door_h = floor_h * 0.7f;
    // Door frame
    Mat4 door = Mat4::translate(0, door_h * 0.5f, depth * 0.5f + 0.06f)
              * Mat4::scale(door_w * 0.55f, door_h * 0.5f, 0.05f);
    appendTransformed(m, box, door);
    // Awning/canopy
    Mat4 canopy = Mat4::translate(0, door_h + 0.1f, depth * 0.5f + 0.2f)
                * Mat4::scale(door_w * 0.8f, 0.04f, 0.15f);
    appendTransformed(m, box, canopy);
    // Steps (2 steps)
    for (int si = 0; si < 2; si++) {
        float sw = door_w * (1.0f + si * 0.3f);
        float step_y = si * 0.12f + 0.06f;
        Mat4 step = Mat4::translate(0, step_y, depth * 0.5f + 0.15f + si * 0.12f)
                  * Mat4::scale(sw * 0.5f, 0.06f, 0.08f);
        appendTransformed(m, box, step);
    }

    // Roof
    switch (roof_type) {
        case 0: { // Flat with parapet
            float par_h = 0.3f;
            // 4 parapet walls
            Mat4 pf = Mat4::translate(0, height + par_h * 0.5f, depth * 0.5f)
                    * Mat4::scale(width * 0.52f, par_h * 0.5f, 0.06f);
            appendTransformed(m, box, pf);
            Mat4 pb = Mat4::translate(0, height + par_h * 0.5f, -depth * 0.5f)
                    * Mat4::scale(width * 0.52f, par_h * 0.5f, 0.06f);
            appendTransformed(m, box, pb);
            Mat4 pl = Mat4::translate(-width * 0.5f, height + par_h * 0.5f, 0)
                    * Mat4::scale(0.06f, par_h * 0.5f, depth * 0.52f);
            appendTransformed(m, box, pl);
            Mat4 pr = Mat4::translate(width * 0.5f, height + par_h * 0.5f, 0)
                    * Mat4::scale(0.06f, par_h * 0.5f, depth * 0.52f);
            appendTransformed(m, box, pr);
            break;
        }
        case 1: { // Gabled roof (two angled slabs)
            float roof_h = height * 0.2f;
            float roof_angle = 25.0f;
            Mat4 r1 = Mat4::translate(0, height + roof_h * 0.35f, -depth * 0.13f)
                    * Mat4::rotateX(roof_angle)
                    * Mat4::scale(width * 0.54f, 0.06f, depth * 0.35f);
            appendTransformed(m, box, r1);
            Mat4 r2 = Mat4::translate(0, height + roof_h * 0.35f, depth * 0.13f)
                    * Mat4::rotateX(-roof_angle)
                    * Mat4::scale(width * 0.54f, 0.06f, depth * 0.35f);
            appendTransformed(m, box, r2);
            // Ridge beam
            Mat4 ridge = Mat4::translate(0, height + roof_h * 0.6f, 0)
                       * Mat4::scale(width * 0.52f, 0.05f, 0.05f);
            appendTransformed(m, box, ridge);
            break;
        }
        case 2: { // Tower/chimney
            float tw = width * 0.2f;
            float th = height * 0.25f;
            Mat4 tower = Mat4::translate(rng.range(-width * 0.2f, width * 0.2f),
                                         height + th * 0.5f,
                                         rng.range(-depth * 0.2f, depth * 0.2f))
                       * Mat4::scale(tw * 0.5f, th * 0.5f, tw * 0.5f);
            appendTransformed(m, box, tower);
            // Roof slab
            Mat4 slab = Mat4::translate(0, height + 0.05f, 0)
                      * Mat4::scale(width * 0.52f, 0.05f, depth * 0.52f);
            appendTransformed(m, box, slab);
            break;
        }
        case 3: { // Dome (half-sphere approximation)
            MeshData dome_mesh = sphere(8, 4);
            float dome_r = (width < depth ? width : depth) * 0.35f;
            Mat4 dome_mat = Mat4::translate(0, height, 0) * Mat4::scale(dome_r, dome_r * 0.6f, dome_r);
            appendTransformed(m, dome_mesh, dome_mat);
            break;
        }
    }

    // Chimney/antenna (40% chance)
    if (rng.flt() > 0.6f) {
        float ch_h = rng.range(1.0f, 2.5f);
        float ch_x = rng.range(-width * 0.3f, width * 0.3f);
        float ch_z = rng.range(-depth * 0.3f, depth * 0.3f);
        Mat4 chimney = Mat4::translate(ch_x, height + ch_h * 0.5f, ch_z)
                     * Mat4::scale(0.15f, ch_h * 0.5f, 0.15f);
        appendTransformed(m, box, chimney);
    }

    // Drainpipe on corner (30% chance)
    if (rng.flt() > 0.7f) {
        float dp_h = height * 0.9f;
        Mat4 pipe = Mat4::translate(width * 0.48f, dp_h * 0.5f, depth * 0.48f)
                  * Mat4::scale(0.04f, dp_h * 0.5f, 0.04f);
        appendTransformed(m, cyl, pipe);
    }

    return m;
}

// ============================================================
// Tree V2: tapered trunks, ellipsoid canopies, tree types
// ============================================================

MeshData MeshGen::tree_v2(float height, float trunk_radius, int max_depth, unsigned int seed) {
    MeshData m;
    MeshData frust = frustum(8, 1.0f, 1.0f, 0.65f); // tapered segment
    MeshData leaf_lo = sphere(5, 3);  // low-poly canopy cluster
    MeshData leaf_hi = sphere(6, 4);  // higher quality canopy

    MeshLCG rng(seed);

    // Tree type: 0=oak(broad), 1=pine(conical), 2=birch(thin/tall), 3=dead
    int tree_type = seed & 3;

    struct Branch {
        Vec3 base;
        float height;
        float radius;
        float angle_y, angle_x;
        int depth;
        unsigned int seed;
    };

    std::vector<Branch> stack;
    Branch root;
    root.base = Vec3(0, 0, 0);
    root.height = height;
    root.radius = trunk_radius;
    root.angle_y = 0;
    root.angle_x = 0;
    root.depth = 0;
    root.seed = seed;
    stack.push_back(root);

    // Root flare: 3-5 roots at base
    int num_roots = 3 + static_cast<int>(rng.flt() * 3.0f);
    for (int ri = 0; ri < num_roots; ri++) {
        float ra = static_cast<float>(ri) / num_roots * 360.0f + rng.range(-15, 15);
        float root_h = height * 0.15f;
        float root_r = trunk_radius * 0.6f;
        Mat4 root_mat = Mat4::rotateY(ra) * Mat4::rotateX(15.0f + rng.flt() * 15.0f)
                      * Mat4::scale(root_r, root_h, root_r)
                      * Mat4::translate(0, 0.5f, 0);
        appendTransformed(m, frust, root_mat);
    }

    while (!stack.empty()) {
        Branch br = stack.back();
        stack.pop_back();

        // Tapered branch segment
        float r_top = br.radius * 0.65f;
        Mat4 branch_mat = Mat4::translate(br.base.x, br.base.y, br.base.z)
                        * Mat4::rotateY(br.angle_y)
                        * Mat4::rotateX(br.angle_x)
                        * Mat4::scale(br.radius, br.height, br.radius)
                        * Mat4::translate(0, 0.5f, 0);
        appendTransformed(m, frust, branch_mat);

        // End point
        Vec3 dir(0, br.height, 0);
        Mat4 rot = Mat4::rotateY(br.angle_y) * Mat4::rotateX(br.angle_x);
        Vec3 end = br.base + rot.transformPoint(dir);

        MeshLCG brng(br.seed);

        if (br.depth >= max_depth) {
            if (tree_type == 3) continue; // dead tree: no canopy

            // Canopy clusters: 3-5 overlapping ellipsoids, tight around branch tip
            int clusters = 3 + static_cast<int>(brng.flt() * 2.5f);
            for (int ci = 0; ci < clusters; ci++) {
                float lr = br.height * (0.7f + brng.flt() * 0.5f);
                float lx = brng.range(-lr * 0.25f, lr * 0.25f);
                float ly = brng.range(-lr * 0.1f, lr * 0.25f);
                float lz = brng.range(-lr * 0.25f, lr * 0.25f);

                // Vary ellipsoid shape based on tree type
                float sx = lr, sy = lr, sz = lr;
                if (tree_type == 0) { sx = lr * 1.4f; sy = lr * 0.7f; sz = lr * 1.4f; }  // oak: wide flat
                else if (tree_type == 1) { sx = lr * 0.8f; sy = lr * 1.5f; sz = lr * 0.8f; } // pine: tall narrow
                else { sx = lr * 1.1f; sy = lr * 0.9f; sz = lr * 1.1f; } // birch: rounded

                Mat4 leaf_mat = Mat4::translate(end.x + lx, end.y + ly, end.z + lz)
                              * Mat4::scale(sx, sy, sz);
                appendTransformed(m, (br.depth > 1) ? leaf_lo : leaf_hi, leaf_mat);
            }
            continue;
        }

        // Spawn children
        int children;
        if (tree_type == 1) children = 3 + static_cast<int>(brng.flt() * 2.0f); // pine: more branches
        else children = 2 + static_cast<int>(brng.flt() * 1.5f);
        if (children > 4) children = 4;

        for (int i = 0; i < children; i++) {
            Branch child;
            child.base = end;
            child.height = br.height * (0.55f + brng.flt() * 0.2f);
            child.radius = r_top * (0.7f + brng.flt() * 0.25f);
            child.angle_y = br.angle_y + brng.range(-50, 50);
            child.depth = br.depth + 1;
            child.seed = br.seed + static_cast<unsigned int>(i * 7919 + br.depth * 2371);

            if (tree_type == 1) // pine: branches angle more outward
                child.angle_x = br.angle_x + 25.0f + brng.flt() * 20.0f;
            else if (tree_type == 2) // birch: tall with narrow spread
                child.angle_x = br.angle_x + 12.0f + brng.flt() * 15.0f;
            else
                child.angle_x = br.angle_x + 15.0f + brng.flt() * 25.0f;

            stack.push_back(child);
        }

        // Dead branches (20% at depth > 0, not for pine)
        if (tree_type != 1 && br.depth > 0 && brng.flt() > 0.8f) {
            Branch dead;
            dead.base = end;
            dead.height = br.height * 0.3f;
            dead.radius = r_top * 0.3f;
            dead.angle_y = br.angle_y + brng.range(-90, 90);
            dead.angle_x = br.angle_x + brng.range(30, 60);
            dead.depth = max_depth; // terminal
            dead.seed = br.seed + 999;
            // Just the branch, no canopy (will be skipped as tree_type==3 logic doesn't apply,
            // but we set depth=max_depth so it tries canopy — we add it without canopy manually)
            Mat4 dm = Mat4::translate(dead.base.x, dead.base.y, dead.base.z)
                    * Mat4::rotateY(dead.angle_y) * Mat4::rotateX(dead.angle_x)
                    * Mat4::scale(dead.radius, dead.height, dead.radius)
                    * Mat4::translate(0, 0.5f, 0);
            appendTransformed(m, frust, dm);
        }
    }

    return m;
}

// ============================================================
// Arch V2: proper columns with base/capital, keystone
// ============================================================

MeshData MeshGen::arch_v2(float width, float height, float thickness, int segments, unsigned int seed) {
    MeshData m;
    MeshData box = cube();
    MeshData cyl = cylinder(8, 1.0f, 1.0f);
    // Fix: scale cube ±0.5 → ±1, cylinder Y ±0.5 → ±1 (half-extent convention)
    for (size_t i = 0; i < box.vertices.size(); i++) {
        box.vertices[i].pos = box.vertices[i].pos * 2.0f;
    }
    for (size_t i = 0; i < cyl.vertices.size(); i++) {
        cyl.vertices[i].pos.y *= 2.0f;
    }
    (void)seed;

    float pillar_r = thickness * 0.4f;
    float pillar_h = height * 0.6f;
    float half_w = width * 0.5f;

    // Column helper: base + shaft + capital
    for (int side = -1; side <= 1; side += 2) {
        float cx = side * (half_w - pillar_r);

        // Base (wider box)
        Mat4 col_base = Mat4::translate(cx, thickness * 0.3f, 0)
                      * Mat4::scale(pillar_r * 1.4f, thickness * 0.3f, pillar_r * 1.4f);
        appendTransformed(m, box, col_base);

        // Shaft (octagonal cylinder)
        Mat4 shaft = Mat4::translate(cx, thickness * 0.6f + pillar_h * 0.5f, 0)
                   * Mat4::scale(pillar_r, pillar_h * 0.5f, pillar_r);
        appendTransformed(m, cyl, shaft);

        // Capital (2-tier widening)
        float cap_y = thickness * 0.6f + pillar_h;
        Mat4 cap1 = Mat4::translate(cx, cap_y + 0.06f, 0)
                  * Mat4::scale(pillar_r * 1.2f, 0.06f, pillar_r * 1.2f);
        appendTransformed(m, box, cap1);
        Mat4 cap2 = Mat4::translate(cx, cap_y + 0.15f, 0)
                  * Mat4::scale(pillar_r * 1.5f, 0.05f, pillar_r * 1.5f);
        appendTransformed(m, box, cap2);
    }

    // Arch curve (smooth segments)
    float arch_radius = half_w - pillar_r;
    float arch_base_y = thickness * 0.6f + pillar_h + 0.2f;
    for (int i = 0; i < segments; i++) {
        float a0 = PI * static_cast<float>(i) / segments;
        float a1 = PI * static_cast<float>(i + 1) / segments;
        float mid_a = (a0 + a1) * 0.5f;
        float seg_len = arch_radius * (a1 - a0);

        float ax = -cosf(mid_a) * arch_radius;
        float ay = arch_base_y + sinf(mid_a) * arch_radius;
        float angle_deg = mid_a * 180.0f / PI - 90.0f;

        Mat4 seg_mat = Mat4::translate(ax, ay, 0)
                     * Mat4::rotateZ(angle_deg)
                     * Mat4::scale(seg_len * 0.6f, thickness * 0.4f, thickness * 0.4f);
        appendTransformed(m, box, seg_mat);
    }

    // Keystone (larger block at apex)
    Mat4 keystone = Mat4::translate(0, arch_base_y + arch_radius, 0)
                  * Mat4::scale(thickness * 0.5f, thickness * 0.6f, thickness * 0.5f);
    appendTransformed(m, box, keystone);

    // Entablature (horizontal beam on top)
    float ent_y = arch_base_y + arch_radius + thickness * 0.6f;
    Mat4 architrave = Mat4::translate(0, ent_y, 0)
                    * Mat4::scale(half_w + thickness * 0.3f, thickness * 0.15f, thickness * 0.55f);
    appendTransformed(m, box, architrave);
    Mat4 cornice = Mat4::translate(0, ent_y + thickness * 0.2f, 0)
                 * Mat4::scale(half_w + thickness * 0.5f, thickness * 0.1f, thickness * 0.65f);
    appendTransformed(m, box, cornice);

    return m;
}

// ============================================================
// Street lamp
// ============================================================

MeshData MeshGen::street_lamp(float height, unsigned int seed) {
    MeshData m;
    MeshData cyl = cylinder(8, 1.0f, 1.0f);
    MeshData box = cube();
    MeshData lamp_sphere = sphere(6, 4);
    // Fix: scale cube ±0.5 → ±1, cylinder Y ±0.5 → ±1 (half-extent convention)
    for (size_t i = 0; i < box.vertices.size(); i++) {
        box.vertices[i].pos = box.vertices[i].pos * 2.0f;
    }
    for (size_t i = 0; i < cyl.vertices.size(); i++) {
        cyl.vertices[i].pos.y *= 2.0f;
    }
    (void)seed;

    // Base plate
    Mat4 base = Mat4::translate(0, 0.05f, 0) * Mat4::scale(0.2f, 0.05f, 0.2f);
    appendTransformed(m, cyl, base);

    // Base detail ring
    Mat4 ring = Mat4::translate(0, 0.15f, 0) * Mat4::scale(0.15f, 0.03f, 0.15f);
    appendTransformed(m, cyl, ring);

    // Main pole
    float pole_r = 0.04f;
    Mat4 pole = Mat4::translate(0, height * 0.5f, 0) * Mat4::scale(pole_r, height * 0.5f, pole_r);
    appendTransformed(m, cyl, pole);

    // Arm (horizontal bracket)
    float arm_len = 0.6f;
    // Rotate so it goes horizontal (Z axis)
    Mat4 arm_h = Mat4::translate(0, height * 0.92f, arm_len * 0.4f)
               * Mat4::rotateX(80.0f)
               * Mat4::scale(0.025f, arm_len * 0.5f, 0.025f);
    appendTransformed(m, cyl, arm_h);

    // Lamp housing (inverted frustum shape)
    Mat4 housing = Mat4::translate(0, height * 0.88f, arm_len * 0.7f)
                 * Mat4::scale(0.1f, 0.12f, 0.1f);
    appendTransformed(m, lamp_sphere, housing);

    // Top finial
    Mat4 finial = Mat4::translate(0, height + 0.05f, 0) * Mat4::scale(0.06f, 0.05f, 0.06f);
    appendTransformed(m, lamp_sphere, finial);

    return m;
}

// ============================================================
// Fence segment
// ============================================================

MeshData MeshGen::fence_segment(float length, float height, unsigned int seed) {
    MeshData m;
    MeshData box = cube();
    // Fix: scale cube ±0.5 → ±1 (half-extent convention)
    for (size_t i = 0; i < box.vertices.size(); i++) {
        box.vertices[i].pos = box.vertices[i].pos * 2.0f;
    }
    MeshLCG rng(seed);

    // Fence type: 0=picket, 1=wall, 2=railing
    int fence_type = seed & 3;
    if (fence_type == 3) fence_type = 0;

    if (fence_type == 1) {
        // Solid wall
        Mat4 wall = Mat4::translate(0, height * 0.5f, 0)
                  * Mat4::scale(length * 0.5f, height * 0.5f, 0.1f);
        appendTransformed(m, box, wall);
        // Top cap
        Mat4 cap = Mat4::translate(0, height + 0.03f, 0)
                 * Mat4::scale(length * 0.52f, 0.03f, 0.12f);
        appendTransformed(m, box, cap);
    } else {
        // Posts
        int num_posts = static_cast<int>(length / 1.2f) + 1;
        if (num_posts < 2) num_posts = 2;
        float spacing = length / (num_posts - 1);
        float post_w = 0.06f;

        for (int i = 0; i < num_posts; i++) {
            float px = -length * 0.5f + i * spacing;
            float post_h = height + (i == 0 || i == num_posts - 1 ? 0.15f : 0.0f);
            Mat4 post = Mat4::translate(px, post_h * 0.5f, 0)
                      * Mat4::scale(post_w, post_h * 0.5f, post_w);
            appendTransformed(m, box, post);

            // Post cap (pointed top for picket)
            if (fence_type == 0) {
                Mat4 pcap = Mat4::translate(px, post_h + 0.04f, 0)
                          * Mat4::scale(post_w * 0.7f, 0.04f, post_w * 0.7f);
                appendTransformed(m, box, pcap);
            }
        }

        // Horizontal rails
        int num_rails = (fence_type == 2) ? 3 : 2;
        for (int ri = 0; ri < num_rails; ri++) {
            float ry = height * (0.3f + 0.4f * static_cast<float>(ri) / (num_rails - 1));
            Mat4 rail = Mat4::translate(0, ry, 0)
                      * Mat4::scale(length * 0.5f, 0.025f, 0.025f);
            appendTransformed(m, box, rail);
        }

        // Pickets (vertical slats between posts) for picket fence
        if (fence_type == 0) {
            int num_pickets = static_cast<int>(length / 0.15f);
            float picket_spacing = length / num_pickets;
            for (int pi = 0; pi < num_pickets; pi++) {
                float px = -length * 0.5f + (pi + 0.5f) * picket_spacing;
                Mat4 picket = Mat4::translate(px, height * 0.5f, 0)
                            * Mat4::scale(0.02f, height * 0.45f, 0.015f);
                appendTransformed(m, box, picket);
            }
        }
    }

    return m;
}
