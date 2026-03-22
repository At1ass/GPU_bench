#include "geometry/mesh_gen.h"
#include <cmath>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>

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
            float h = 0.0f;

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

float MeshGen::boundingRadius(const MeshData& md) {
    if (md.vertices.empty()) return 0;
    float max_r2 = 0;
    for (size_t i = 0; i < md.vertices.size(); i++) {
        const Vec3& p = md.vertices[i].pos;
        float r2 = p.x * p.x + p.y * p.y + p.z * p.z;
        if (r2 > max_r2) max_r2 = r2;
    }
    return sqrtf(max_r2);
}

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

void MeshGen::smoothNormals(MeshData& m) {
    // Step 1: compute per-face normals accumulated onto each vertex
    recomputeNormals(m);

    // Step 2: group vertices by position, average their normals.
    // OBJ loader creates duplicate vertices (same pos, different UV/normal),
    // so recomputeNormals alone gives flat shading. We fix this by averaging
    // normals across all vertices at the same spatial position.
    struct PosKey {
        int x, y, z;
        bool operator==(const PosKey& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    struct PosHash {
        size_t operator()(const PosKey& k) const {
            size_t h = static_cast<size_t>(k.x) * 73856093u;
            h ^= static_cast<size_t>(k.y) * 19349663u;
            h ^= static_cast<size_t>(k.z) * 83492791u;
            return h;
        }
    };

    // Quantize positions to ~0.0001 precision
    const float quant = 10000.0f;

    std::unordered_map<PosKey, std::vector<unsigned int>, PosHash> groups;
    for (unsigned int i = 0; i < static_cast<unsigned int>(m.vertices.size()); i++) {
        const Vec3& p = m.vertices[i].pos;
        PosKey key;
        key.x = static_cast<int>(p.x * quant + (p.x >= 0 ? 0.5f : -0.5f));
        key.y = static_cast<int>(p.y * quant + (p.y >= 0 ? 0.5f : -0.5f));
        key.z = static_cast<int>(p.z * quant + (p.z >= 0 ? 0.5f : -0.5f));
        groups[key].push_back(i);
    }

    for (auto& kv : groups) {
        if (kv.second.size() <= 1) continue;

        // Average normals across all vertices at this position
        Vec3 avg(0, 0, 0);
        for (unsigned int idx : kv.second) {
            avg = avg + m.vertices[idx].normal;
        }
        avg = avg.normalized();

        for (unsigned int idx : kv.second) {
            m.vertices[idx].normal = avg;
        }
    }
}

// ============================================================
// Simple deterministic LCG PRNG for mesh generation
// ============================================================

struct MeshRNG {
    unsigned int state;
    MeshRNG(unsigned int seed) : state(seed) {}
    float next() {
        state = state * 1103515245u + 12345u;
        return static_cast<float>((state >> 16) & 0x7FFF) / 32767.0f;
    }
};

// ============================================================
// Scattered rocks (batched into one mesh)
// ============================================================

MeshData MeshGen::scatteredRocks(int count, float area_size, float min_scale, float max_scale, unsigned int seed) {
    MeshData result;
    MeshRNG rng(seed);

    const int segs = 10;
    const int rngs = 6;

    for (int rock = 0; rock < count; rock++) {
        // Random position, avoid center (bunny area)
        float px, pz, dist2;
        do {
            px = (rng.next() - 0.5f) * area_size;
            pz = (rng.next() - 0.5f) * area_size;
            dist2 = px * px + pz * pz;
        } while (dist2 < 1.5f); // min distance ~1.2 from center

        float sc = min_scale + rng.next() * (max_scale - min_scale);
        float ground_y = -1.0f;

        // Per-rock random shape seed (consistent deformation per rock)
        float rock_seed = rng.next() * 100.0f;

        unsigned int base = static_cast<unsigned int>(result.vertices.size());

        for (int r = 0; r <= rngs; r++) {
            float v = static_cast<float>(r) / rngs;
            float phi = v * PI;
            for (int s = 0; s <= segs; s++) {
                float u = static_cast<float>(s) / segs;
                float theta = u * 2.0f * PI;

                float sp = sinf(phi), cp = cosf(phi);
                float st = sinf(theta), ct = cosf(theta);

                Vec3 pos(sp * ct, cp, sp * st);

                // Gentle noise: coherent per-rock deformation (not random per vertex)
                float n1 = sinf(theta * 3.0f + rock_seed) * 0.12f;
                float n2 = sinf(phi * 2.0f + rock_seed * 0.7f) * 0.08f;
                float noise = 1.0f + n1 + n2;
                pos = pos * noise;

                // Flatten Y (rocks are wider than tall) and squash bottom half
                float y_squash = (pos.y < 0.0f) ? 0.3f : 0.5f;
                float final_x = pos.x * sc + px;
                float final_y = pos.y * sc * y_squash + ground_y + sc * 0.25f;
                float final_z = pos.z * sc + pz;

                Vertex vt;
                vt.pos = Vec3(final_x, final_y, final_z);
                vt.normal = pos.normalized();
                vt.uv = Vec2(u, v);
                result.vertices.push_back(vt);
            }
        }

        for (int r = 0; r < rngs; r++) {
            for (int s = 0; s < segs; s++) {
                unsigned int a = base + r * (segs + 1) + s;
                unsigned int b = a + segs + 1;
                result.indices.push_back(a);
                result.indices.push_back(b);
                result.indices.push_back(a + 1);
                result.indices.push_back(a + 1);
                result.indices.push_back(b);
                result.indices.push_back(b + 1);
            }
        }
    }

    recomputeNormals(result);
    return result;
}

// ============================================================
// Scattered grass tufts (batched into one mesh)
// ============================================================

MeshData MeshGen::scatteredGrass(int count, float area_size, float blade_height, float blade_width, unsigned int seed) {
    MeshData result;
    MeshRNG rng(seed);

    float ground_y = -1.0f;

    for (int tuft = 0; tuft < count; tuft++) {
        // Random position, avoid center
        float px, pz, dist2;
        do {
            px = (rng.next() - 0.5f) * area_size;
            pz = (rng.next() - 0.5f) * area_size;
            dist2 = px * px + pz * pz;
        } while (dist2 < 0.49f);

        // Random variation per tuft
        float bh = blade_height * (0.7f + 0.6f * rng.next());
        float bw = blade_width * (0.8f + 0.4f * rng.next());

        // 3 crossed blades rotated 60 degrees apart
        for (int q = 0; q < 3; q++) {
            float angle = static_cast<float>(q) * PI / 3.0f + rng.next() * 0.5f;
            float cs = cosf(angle);
            float sn = sinf(angle);

            float dx = cs * bw * 0.5f;
            float dz = sn * bw * 0.5f;

            // Tapered blade: wide at base, pointed at tip
            float tip_taper = 0.1f; // tip is 10% of base width

            unsigned int base = static_cast<unsigned int>(result.vertices.size());

            Vertex v0, v1, v2, v3;
            // Normal perpendicular to blade face
            Vec3 blade_normal = Vec3(-sn, 0.0f, cs).normalized();

            // Bottom-left (full width)
            v0.pos = Vec3(px - dx, ground_y, pz - dz);
            v0.normal = blade_normal;
            v0.uv = Vec2(0.0f, 0.0f);

            // Bottom-right (full width)
            v1.pos = Vec3(px + dx, ground_y, pz + dz);
            v1.normal = blade_normal;
            v1.uv = Vec2(1.0f, 0.0f);

            // Top-right (tapered)
            v2.pos = Vec3(px + dx * tip_taper, ground_y + bh, pz + dz * tip_taper);
            v2.normal = blade_normal;
            v2.uv = Vec2(1.0f, 1.0f);

            // Top-left (tapered)
            v3.pos = Vec3(px - dx * tip_taper, ground_y + bh, pz - dz * tip_taper);
            v3.normal = blade_normal;
            v3.uv = Vec2(0.0f, 1.0f);

            result.vertices.push_back(v0);
            result.vertices.push_back(v1);
            result.vertices.push_back(v2);
            result.vertices.push_back(v3);

            result.indices.push_back(base + 0);
            result.indices.push_back(base + 1);
            result.indices.push_back(base + 2);
            result.indices.push_back(base + 0);
            result.indices.push_back(base + 2);
            result.indices.push_back(base + 3);
        }
    }

    return result;
}

// ============================================================
// Billboard particle quads (batched into one mesh)
// ============================================================

MeshData MeshGen::particleQuads(int count, float area_size, float height_range, unsigned int seed) {
    MeshData result;
    MeshRNG rng(seed);

    for (int i = 0; i < count; i++) {
        // Random seed position in a volume, avoid center (min dist ~0.5)
        float sx, sy, sz, dist2;
        do {
            sx = (rng.next() - 0.5f) * area_size;
            sz = (rng.next() - 0.5f) * area_size;
            dist2 = sx * sx + sz * sz;
        } while (dist2 < 0.25f); // 0.5^2

        sy = -0.5f + rng.next() * height_range;

        Vec3 seed_pos(sx, sy, sz);
        float particle_id = static_cast<float>(i) / static_cast<float>(count > 1 ? count - 1 : 1);

        unsigned int base = static_cast<unsigned int>(result.vertices.size());

        // Unit quad corners: the vertex shader will billboard these
        // a_pos.xy = quad corner offset, a_normal = seed position, a_uv.x = particle ID
        Vertex v0, v1, v2, v3;

        v0.pos = Vec3(-1.0f, -1.0f, 0.0f);
        v0.normal = seed_pos;
        v0.uv = Vec2(particle_id, 0.0f);

        v1.pos = Vec3(1.0f, -1.0f, 0.0f);
        v1.normal = seed_pos;
        v1.uv = Vec2(particle_id, 0.0f);

        v2.pos = Vec3(1.0f, 1.0f, 0.0f);
        v2.normal = seed_pos;
        v2.uv = Vec2(particle_id, 1.0f);

        v3.pos = Vec3(-1.0f, 1.0f, 0.0f);
        v3.normal = seed_pos;
        v3.uv = Vec2(particle_id, 1.0f);

        result.vertices.push_back(v0);
        result.vertices.push_back(v1);
        result.vertices.push_back(v2);
        result.vertices.push_back(v3);

        result.indices.push_back(base + 0);
        result.indices.push_back(base + 1);
        result.indices.push_back(base + 2);
        result.indices.push_back(base + 0);
        result.indices.push_back(base + 2);
        result.indices.push_back(base + 3);
    }

    return result;
}

// ============================================================
// Forsyth vertex cache optimization
// ============================================================

void MeshGen::optimizeVertexCache(MeshData& m) {
    if (m.indices.size() < 3 || m.vertices.empty()) return;

    const int CACHE_SIZE = 32;
    const size_t num_verts = m.vertices.size();
    const size_t num_tris = m.indices.size() / 3;

    // Per-vertex data
    struct VertData {
        float score;
        int cache_pos;          // -1 = not in cache
        int remaining_tris;     // number of un-emitted triangles using this vertex
        std::vector<unsigned int> tri_list; // triangles using this vertex
    };
    std::vector<VertData> vdata(num_verts);
    for (size_t i = 0; i < num_verts; i++) {
        vdata[i].score = 0.0f;
        vdata[i].cache_pos = -1;
        vdata[i].remaining_tris = 0;
    }

    // Build adjacency: which triangles reference each vertex
    for (size_t t = 0; t < num_tris; t++) {
        for (int j = 0; j < 3; j++) {
            unsigned int vi = m.indices[t * 3 + j];
            if (vi < num_verts) {
                vdata[vi].tri_list.push_back(static_cast<unsigned int>(t));
                vdata[vi].remaining_tris++;
            }
        }
    }

    // Scoring function for a single vertex
    auto computeVertexScore = [&](unsigned int vi) -> float {
        if (vi >= num_verts) return 0.0f;
        VertData& vd = vdata[vi];
        if (vd.remaining_tris <= 0) {
            vd.score = 0.0f;
            return 0.0f;
        }
        float score = 0.0f;
        // Cache position score
        if (vd.cache_pos >= 0 && vd.cache_pos < CACHE_SIZE) {
            float normalized = static_cast<float>(vd.cache_pos) / static_cast<float>(CACHE_SIZE);
            score += powf(1.0f - normalized, 1.5f);
        }
        // Valence score
        score += powf(static_cast<float>(vd.remaining_tris), -0.5f) * 2.0f;
        vd.score = score;
        return score;
    };

    // Initial scores
    for (size_t i = 0; i < num_verts; i++) {
        computeVertexScore(static_cast<unsigned int>(i));
    }

    // Per-triangle: emitted flag and score
    std::vector<bool> tri_emitted(num_tris, false);

    // Compute initial triangle scores
    auto triScore = [&](unsigned int t) -> float {
        if (t >= num_tris || tri_emitted[t]) return 0.0f;
        return vdata[m.indices[t * 3 + 0]].score
             + vdata[m.indices[t * 3 + 1]].score
             + vdata[m.indices[t * 3 + 2]].score;
    };

    // LRU cache (indices of vertices, front = most recent)
    std::vector<unsigned int> cache;
    cache.reserve(CACHE_SIZE + 3);

    // Output index buffer
    std::vector<unsigned int> new_indices;
    new_indices.reserve(m.indices.size());

    size_t emitted_count = 0;

    while (emitted_count < num_tris) {
        // Find the best triangle to emit.
        // First, check triangles adjacent to cache entries (fast path).
        unsigned int best_tri = static_cast<unsigned int>(num_tris); // invalid
        float best_score = -1.0f;

        for (size_t ci = 0; ci < cache.size(); ci++) {
            unsigned int vi = cache[ci];
            const VertData& vd = vdata[vi];
            for (size_t ti = 0; ti < vd.tri_list.size(); ti++) {
                unsigned int t = vd.tri_list[ti];
                if (tri_emitted[t]) continue;
                float s = triScore(t);
                if (s > best_score) {
                    best_score = s;
                    best_tri = t;
                }
            }
        }

        // If no triangle found from cache (cold start or cache miss), scan all
        if (best_tri >= num_tris) {
            for (size_t t = 0; t < num_tris; t++) {
                if (tri_emitted[t]) continue;
                float s = triScore(static_cast<unsigned int>(t));
                if (s > best_score) {
                    best_score = s;
                    best_tri = static_cast<unsigned int>(t);
                }
            }
        }

        if (best_tri >= num_tris) break; // should not happen

        // Emit the triangle
        tri_emitted[best_tri] = true;
        emitted_count++;
        for (int j = 0; j < 3; j++) {
            unsigned int vi = m.indices[best_tri * 3 + j];
            new_indices.push_back(vi);
            vdata[vi].remaining_tris--;

            // Update cache: move vi to front (LRU)
            // Remove from current position if present
            for (size_t ci = 0; ci < cache.size(); ci++) {
                if (cache[ci] == vi) {
                    cache.erase(cache.begin() + static_cast<long>(ci));
                    break;
                }
            }
            cache.insert(cache.begin(), vi);
        }

        // Trim cache to CACHE_SIZE
        if (cache.size() > static_cast<size_t>(CACHE_SIZE)) {
            cache.resize(CACHE_SIZE);
        }

        // Update cache positions and recompute scores for affected vertices
        for (size_t ci = 0; ci < cache.size(); ci++) {
            vdata[cache[ci]].cache_pos = static_cast<int>(ci);
        }
        // Vertices that fell out of the cache
        // (We don't track them individually, but since we only process
        // cache entries, evicted vertices keep their old cache_pos until
        // recalculated. We fix this by setting cache_pos = -1 for vertices
        // not in the cache before scoring.)
        // Actually, let's just recompute scores for all cache vertices
        // and the 3 triangle vertices (which are now in cache).
        for (size_t ci = 0; ci < cache.size(); ci++) {
            computeVertexScore(cache[ci]);
        }
    }

    // Reset cache positions for all verts (cleanup)
    for (size_t i = 0; i < num_verts; i++) {
        vdata[i].cache_pos = -1;
    }

    // --- Reorder vertices for sequential access ---
    // Build a remap table: new_index[old_index] = new vertex index
    std::vector<unsigned int> remap(num_verts, static_cast<unsigned int>(num_verts));
    unsigned int next_vert = 0;
    for (size_t i = 0; i < new_indices.size(); i++) {
        unsigned int old_idx = new_indices[i];
        if (remap[old_idx] >= static_cast<unsigned int>(num_verts)) {
            remap[old_idx] = next_vert++;
        }
        new_indices[i] = remap[old_idx];
    }

    // Build reordered vertex array
    std::vector<Vertex> new_verts(next_vert);
    for (size_t i = 0; i < num_verts; i++) {
        if (remap[i] < next_vert) {
            new_verts[remap[i]] = m.vertices[i];
        }
    }

    m.vertices.swap(new_verts);
    m.indices.swap(new_indices);
}
