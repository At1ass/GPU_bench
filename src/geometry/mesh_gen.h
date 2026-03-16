#pragma once
#include "geometry/mesh.h"

namespace MeshGen {
    // Fullscreen quad in NDC (-1..1). Uses a_pos(xy) and a_uv.
    MeshData quad();

    // Parametric torus. ring_segments = around the ring, tube_segments = around the tube.
    MeshData torus(int ring_segments, int tube_segments, float ring_radius, float tube_radius);

    // 3D unit cube centered at origin.
    MeshData cube();

    // UV sphere. segments/rings control tessellation.
    MeshData sphere(int segments, int rings);

    // Flat terrain grid on XZ plane, centered at origin.
    // size: world units, resolution: vertices per side.
    // Uses a simple sine-based heightmap.
    MeshData terrain(float size, int resolution);

    // Grid of cubes for geometry stress test.
    // Returns a single mesh with count^3 cubes (total tris = count^3 * 12).
    MeshData cubeGrid(int count);

    // Cone (apex at top, base at Y=0). segments = slices around axis.
    MeshData cone(int segments, float height, float radius);

    // Cylinder (centered at origin, along Y axis). segments = slices.
    MeshData cylinder(int segments, float height, float radius);

    // Frustum (truncated cone, along Y axis). Different top/bottom radii.
    MeshData frustum(int segments, float height, float r_bottom, float r_top);

    // Append src mesh transformed by mat into dst (positions + normals).
    void appendTransformed(MeshData& dst, const MeshData& src, const Mat4& mat);

    // Recompute normals from triangle faces (smooth, area-weighted).
    void recomputeNormals(MeshData& m);

    // --- V1 generators (kept for backward compat) ---
    MeshData rock(int segments, int rings, float radius, unsigned int seed);
    MeshData building_detailed(float width, float height, float depth, unsigned int seed);
    MeshData tree_branching(float height, float trunk_radius, int depth, unsigned int seed);
    MeshData arch(float width, float height, float thickness, int segments);

    // --- V2 generators: higher quality, seed selects variant ---
    MeshData rock_v2(int detail, float radius, unsigned int seed);
    MeshData building_v2(float width, float height, float depth, unsigned int seed);
    MeshData tree_v2(float height, float trunk_radius, int max_depth, unsigned int seed);
    MeshData arch_v2(float width, float height, float thickness, int segments, unsigned int seed);

    // --- Props ---
    MeshData street_lamp(float height, unsigned int seed);
    MeshData fence_segment(float length, float height, unsigned int seed);
}
