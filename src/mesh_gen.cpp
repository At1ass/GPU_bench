#include "mesh_gen.h"
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
