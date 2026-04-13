#include <doctest.h>
#include "geometry/mesh_gen.h"
#include <cmath>

TEST_CASE("MeshGen::quad") {
    MeshData m = MeshGen::quad();
    CHECK(m.vertices.size() == 4);
    CHECK(m.indices.size() == 6);
    for (const auto& v : m.vertices) {
        CHECK(v.uv.x >= 0.0f); CHECK(v.uv.x <= 1.0f);
        CHECK(v.uv.y >= 0.0f); CHECK(v.uv.y <= 1.0f);
        CHECK(v.normal.length() == doctest::Approx(1.0f).epsilon(0.01f));
    }
}

TEST_CASE("MeshGen::cube") {
    MeshData m = MeshGen::cube();
    CHECK(m.vertices.size() == 24);  // 4 per face × 6
    CHECK(m.indices.size() == 36);   // 6 per face × 6
    for (const auto& v : m.vertices) {
        CHECK(v.normal.length() == doctest::Approx(1.0f).epsilon(0.01f));
    }
}

TEST_CASE("MeshGen::sphere vertex count") {
    int segs = 16, rings = 8;
    MeshData m = MeshGen::sphere(segs, rings);
    size_t expected_verts = static_cast<size_t>((segs + 1) * (rings + 1));
    CHECK(m.vertices.size() == expected_verts);
    CHECK(m.indices.size() > 0);
    // All normals approximately unit length
    for (const auto& v : m.vertices) {
        CHECK(v.normal.length() == doctest::Approx(1.0f).epsilon(0.05f));
    }
}

TEST_CASE("MeshGen::recomputeNormals produces unit normals") {
    MeshData m = MeshGen::cube();
    // Zero out normals
    for (auto& v : m.vertices) v.normal = Vec3(0, 0, 0);
    MeshGen::recomputeNormals(m);
    for (const auto& v : m.vertices) {
        CHECK(v.normal.length() == doctest::Approx(1.0f).epsilon(0.01f));
    }
}

TEST_CASE("MeshGen::boundingRadius") {
    MeshData m = MeshGen::sphere(8, 4);
    float r = MeshGen::boundingRadius(m);
    // Radius must be >= max vertex distance from origin
    float max_dist = 0.0f;
    for (const auto& v : m.vertices) {
        float d = v.pos.length();
        if (d > max_dist) max_dist = d;
    }
    CHECK(r >= max_dist - 0.001f);
}

TEST_CASE("MeshGen::optimizeVertexCache preserves mesh") {
    MeshData m = MeshGen::cube();
    size_t orig_verts = m.vertices.size();
    size_t orig_indices = m.indices.size();
    MeshGen::optimizeVertexCache(m);
    CHECK(m.vertices.size() == orig_verts);
    CHECK(m.indices.size() == orig_indices);
}
