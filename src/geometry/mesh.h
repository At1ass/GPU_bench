#pragma once
#include "geometry/math_types.h"
#include <vector>
#include <cstddef>

struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

// Strongly-typed GPU resource handles.
// Implicit conversion to unsigned int allows use as vector index.
// Explicit constructor prevents accidental cross-type assignment.
template<typename Tag>
struct Handle {
    unsigned int id = 0;

    Handle() = default;
    explicit Handle(unsigned int v) : id(v) {}

    operator unsigned int() const { return id; }

    bool operator==(Handle o) const { return id == o.id; }
    bool operator!=(Handle o) const { return id != o.id; }
};

struct MeshTag {};
struct TextureTag {};

using MeshHandle    = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;

static const MeshHandle    INVALID_MESH;
static const TextureHandle INVALID_TEXTURE;
