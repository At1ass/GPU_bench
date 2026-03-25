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

    // Recompute normals from triangle faces (smooth, area-weighted).
    void recomputeNormals(MeshData& m);

    // Smooth normals by position: vertices sharing the same position get
    // averaged normals. Eliminates faceted look on models with split vertices
    // (e.g. OBJ files with per-face normals).
    void smoothNormals(MeshData& m);

    // Compute bounding sphere radius from mesh data (assumes center at origin).
    float boundingRadius(const MeshData& md);

    // Forsyth vertex cache optimization: reorder indices for better
    // post-transform vertex cache utilization, then reorder vertices
    // for sequential access (pre-transform cache).
    void optimizeVertexCache(MeshData& m);

    // Generate a batch of scattered rocks as a single mesh.
    // Each rock is a deformed sphere placed at a random XZ position.
    MeshData scatteredRocks(int count, float area_size, float min_scale, float max_scale, unsigned int seed);

    // Generate scattered grass tufts as a single batched mesh.
    // Each tuft is 3 crossed quads (billboard cross pattern).
    // Vertex Y position encodes height for wind animation.
    MeshData scatteredGrass(int count, float area_size, float blade_height, float blade_width, unsigned int seed);

    // Generate billboard particle quads as a single batched mesh.
    // Each particle is a quad (4 verts, 6 indices).
    // Seed positions stored in the normal attribute.
    MeshData particleQuads(int count, float area_size, float height_range, unsigned int seed);

    // Single grass blade template for instanced rendering.
    // A tapered quad: wide at base (y=0), pointed at tip (y=1).
    // Centered at origin, unit height. Instance shader scales and places.
    MeshData grassBlade();

    // Flat disc on the XZ plane (for puddles/reflective surfaces).
    // Centered at origin, y=0, with organic irregular edges.
    // seed varies the shape per instance.
    MeshData disc(float radius, int segments, unsigned int seed = 0);

    // Half torus (arch shape): only the top half (u in [0, PI]).
    // Generated in XY plane: base endpoints at (±R, 0, 0), apex at (0, R, 0).
    MeshData halfTorus(int ring_segments, int tube_segments, float ring_radius, float tube_radius);

    // Procedural tree: tapered trunk + branching + sphere foliage clusters.
    // Generates a realistic-looking tree with organic non-symmetric crown.
    // seed varies the shape so each tree instance looks different.
    MeshData proceduralTree(float trunk_height, float trunk_radius, float crown_radius, int segments = 10, unsigned int seed = 0);

    // Simple procedural tree: cylinder trunk + 2 overlapping cones for crown (legacy).
    MeshData simpleTree(float trunk_height, float trunk_radius, float crown_height, float crown_radius, int segments = 12);

    // Append src mesh into dst, transforming all vertices by the given matrix.
    // Indices are offset by the current dst vertex count.
    // Useful for merging multi-part objects (e.g. column + base ring) into a single draw call.
    void appendMesh(MeshData& dst, const MeshData& src, const Mat4& transform);
}
