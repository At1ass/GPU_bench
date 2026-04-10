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

    MeshData() = default;
    MeshData(const MeshData&) = default;
    MeshData& operator=(const MeshData&) = default;
    MeshData(MeshData&&) noexcept = default;
    MeshData& operator=(MeshData&&) noexcept = default;
};

// Strongly-typed GPU resource handles.
// Implicit conversion to unsigned int allows use as vector index.
// Explicit constructor prevents accidental cross-type assignment.
template<typename Tag>
struct Handle {
    unsigned int id = 0;

    constexpr Handle() = default;
    explicit constexpr Handle(unsigned int v) : id(v) {}

    explicit operator unsigned int() const { return id; }
    explicit operator bool() const { return id != 0; }

    bool operator==(Handle o) const { return id == o.id; }
    bool operator!=(Handle o) const { return id != o.id; }
};

struct MeshTag {};
struct TextureTag {};

using MeshHandle    = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;

static constexpr MeshHandle    INVALID_MESH{};
static constexpr TextureHandle INVALID_TEXTURE{};
