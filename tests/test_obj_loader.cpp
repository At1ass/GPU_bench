#include <doctest.h>
#include "geometry/obj_loader.h"
#include <cmath>
#include <cstring>

static const float EPS = 1e-4f;

TEST_SUITE("ObjLoader") {

TEST_CASE("OBJ: empty input") {
    MeshData m = ObjLoader::loadFromMemory("", 0);
    CHECK(m.vertices.empty());
    CHECK(m.indices.empty());
}

TEST_CASE("OBJ: single triangle") {
    const char* obj =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    CHECK(m.vertices.size() == 3);
    CHECK(m.indices.size() == 3);
    CHECK(m.vertices[0].pos.x == doctest::Approx(0.0f));
    CHECK(m.vertices[1].pos.x == doctest::Approx(1.0f));
    CHECK(m.vertices[2].pos.y == doctest::Approx(1.0f));
}

TEST_CASE("OBJ: quad → 2 triangles") {
    const char* obj =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "f 1 2 3 4\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    CHECK(m.vertices.size() == 4);
    CHECK(m.indices.size() == 6);  // fan: (0,1,2), (0,2,3)
}

TEST_CASE("OBJ: v/t/n format") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    CHECK(m.vertices.size() == 3);
    CHECK(m.indices.size() == 3);
    CHECK(m.vertices[0].uv.x == doctest::Approx(0.0f));
    CHECK(m.vertices[1].uv.x == doctest::Approx(1.0f));
    CHECK(m.vertices[0].normal.z == doctest::Approx(1.0f));
}

TEST_CASE("OBJ: v//n format") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\n"
        "f 1//1 2//1 3//1\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    CHECK(m.vertices.size() == 3);
    CHECK(m.vertices[0].normal.z == doctest::Approx(1.0f));
}

TEST_CASE("OBJ: negative indices") {
    const char* obj =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f -3 -2 -1\n";  // same as f 1 2 3
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    CHECK(m.vertices.size() == 3);
    CHECK(m.indices.size() == 3);
    CHECK(m.vertices[0].pos.x == doctest::Approx(0.0f));
    CHECK(m.vertices[1].pos.x == doctest::Approx(1.0f));
}

TEST_CASE("OBJ: comments and blank lines") {
    const char* obj =
        "# this is a comment\n"
        "\n"
        "v 0 0 0\n"
        "# another comment\n"
        "v 1 0 0\n"
        "\n"
        "v 0 1 0\n"
        "f 1 2 3\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    CHECK(m.vertices.size() == 3);
}

TEST_CASE("OBJ: normals absent → recomputed") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "f 1 2 3\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    // Normals should be recomputed and unit-length
    for (const auto& v : m.vertices) {
        float len = v.normal.length();
        CHECK(len == doctest::Approx(1.0f).epsilon(0.01f));
    }
}

TEST_CASE("OBJ: normalize centers and scales") {
    const char* obj =
        "v 10 10 10\nv 20 10 10\nv 10 20 10\n"
        "f 1 2 3\n";
    MeshData m = ObjLoader::loadFromMemory(obj, strlen(obj));
    ObjLoader::normalize(m);
    // All vertices should be in [-1, 1]
    for (const auto& v : m.vertices) {
        CHECK(v.pos.x >= -1.0f - EPS);
        CHECK(v.pos.x <= 1.0f + EPS);
        CHECK(v.pos.y >= -1.0f - EPS);
        CHECK(v.pos.y <= 1.0f + EPS);
    }
}

} // TEST_SUITE("ObjLoader")
