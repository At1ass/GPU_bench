#pragma once
#include "geometry/mesh.h"

namespace ObjLoader {
    // Load Wavefront OBJ file. Returns empty MeshData on failure.
    MeshData load(const char* filepath);

    // Parse OBJ data from memory buffer.
    MeshData loadFromMemory(const char* data, size_t len, const char* debug_name = "memory");

    // Center at origin and uniform-scale to fit in [-1,1] cube.
    void normalize(MeshData& mesh);
}
