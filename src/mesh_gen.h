#pragma once
#include "mesh.h"

namespace MeshGen {
    // Fullscreen quad in NDC (-1..1). Uses a_pos(xy) and a_uv.
    MeshData quad();

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
}
