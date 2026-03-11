#pragma once
#include "math_types.h"
#include <vector>

struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

// Opaque handles for GPU resources
typedef unsigned int MeshHandle;
typedef unsigned int TextureHandle;

static const MeshHandle   INVALID_MESH    = 0;
static const TextureHandle INVALID_TEXTURE = 0;
