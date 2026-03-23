#include "geometry/obj_loader.h"
#include "geometry/mesh_gen.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>

MeshData ObjLoader::load(const char* filepath) {
    MeshData result;

    FILE* f = fopen(filepath, "r");
    if (!f) return result;

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;

    // Temp storage for face indices
    struct FaceVert { int vi, ti, ni; };

    // Vertex deduplication map: pack (vi, ti, ni) into uint64_t key
    std::unordered_map<uint64_t, unsigned int> vert_map;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        if (line[0] == 'v' && line[1] == ' ') {
            Vec3 v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3)
                positions.push_back(v);
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            Vec3 n;
            if (sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z) == 3)
                normals.push_back(n);
        }
        else if (line[0] == 'v' && line[1] == 't') {
            Vec2 t;
            if (sscanf(line + 3, "%f %f", &t.x, &t.y) >= 2)
                texcoords.push_back(t);
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            // Parse face vertices (triangles and quads)
            FaceVert verts[4];
            int vert_count = 0;
            const char* p = line + 2;

            while (*p && vert_count < 4) {
                // Skip whitespace
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0' || *p == '\n' || *p == '\r') break;

                FaceVert fv = { 0, 0, 0 };
                int consumed = 0;
                // Try v/t/n
                if (sscanf(p, "%d/%d/%d%n", &fv.vi, &fv.ti, &fv.ni, &consumed) >= 3 && consumed > 0) {
                    p += consumed;
                }
                // Try v//n
                else if (consumed = 0, sscanf(p, "%d//%d%n", &fv.vi, &fv.ni, &consumed) >= 2 && consumed > 0) {
                    p += consumed;
                }
                // Try v/t
                else if (consumed = 0, sscanf(p, "%d/%d%n", &fv.vi, &fv.ti, &consumed) >= 2 && consumed > 0) {
                    p += consumed;
                }
                // Just v
                else if (consumed = 0, sscanf(p, "%d%n", &fv.vi, &consumed) >= 1 && consumed > 0) {
                    p += consumed;
                }
                else {
                    break;
                }

                // Handle negative (relative) indices
                if (fv.vi < 0) fv.vi = static_cast<int>(positions.size()) + fv.vi + 1;
                if (fv.ti < 0) fv.ti = static_cast<int>(texcoords.size()) + fv.ti + 1;
                if (fv.ni < 0) fv.ni = static_cast<int>(normals.size()) + fv.ni + 1;

                verts[vert_count++] = fv;
            }

            // Emit triangles (with vertex deduplication)
            for (int i = 1; i + 1 < vert_count; i++) {
                int tri[3] = { 0, i, i + 1 };
                for (int j = 0; j < 3; j++) {
                    const FaceVert& fv = verts[tri[j]];

                    // Pack (vi, ti, ni) into a single uint64_t key
                    uint64_t key = (static_cast<uint64_t>(static_cast<unsigned int>(fv.vi)) << 40)
                                 | (static_cast<uint64_t>(static_cast<unsigned int>(fv.ti) & 0xFFFFFu) << 20)
                                 | static_cast<uint64_t>(static_cast<unsigned int>(fv.ni) & 0xFFFFFu);

                    std::unordered_map<uint64_t, unsigned int>::iterator it = vert_map.find(key);
                    if (it != vert_map.end()) {
                        result.indices.push_back(it->second);
                    } else {
                        Vertex vtx;
                        vtx.pos = (fv.vi > 0 && fv.vi <= static_cast<int>(positions.size()))
                                  ? positions[static_cast<size_t>(fv.vi - 1)] : Vec3(0, 0, 0);
                        vtx.normal = (fv.ni > 0 && fv.ni <= static_cast<int>(normals.size()))
                                     ? normals[static_cast<size_t>(fv.ni - 1)] : Vec3(0, 1, 0);
                        vtx.uv = (fv.ti > 0 && fv.ti <= static_cast<int>(texcoords.size()))
                                 ? texcoords[static_cast<size_t>(fv.ti - 1)] : Vec2(0, 0);

                        unsigned int idx = static_cast<unsigned int>(result.vertices.size());
                        result.vertices.push_back(vtx);
                        vert_map[key] = idx;
                        result.indices.push_back(idx);
                    }
                }
            }
        }
        // Skip: mtllib, usemtl, o, g, s, etc.
    }

    fclose(f);

    // Recompute normals if none were provided
    if (normals.empty() && !result.vertices.empty()) {
        MeshGen::recomputeNormals(result);
    }

    return result;
}

void ObjLoader::normalize(MeshData& mesh) {
    if (mesh.vertices.empty()) return;

    // Compute AABB
    Vec3 mn = mesh.vertices[0].pos;
    Vec3 mx = mesh.vertices[0].pos;
    for (size_t i = 1; i < mesh.vertices.size(); i++) {
        const Vec3& p = mesh.vertices[i].pos;
        if (p.x < mn.x) mn.x = p.x;
        if (p.y < mn.y) mn.y = p.y;
        if (p.z < mn.z) mn.z = p.z;
        if (p.x > mx.x) mx.x = p.x;
        if (p.y > mx.y) mx.y = p.y;
        if (p.z > mx.z) mx.z = p.z;
    }

    Vec3 center((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
    float extent = mx.x - mn.x;
    if (mx.y - mn.y > extent) extent = mx.y - mn.y;
    if (mx.z - mn.z > extent) extent = mx.z - mn.z;
    float scale = (extent > 1e-6f) ? 2.0f / extent : 1.0f;

    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        mesh.vertices[i].pos.x = (mesh.vertices[i].pos.x - center.x) * scale;
        mesh.vertices[i].pos.y = (mesh.vertices[i].pos.y - center.y) * scale;
        mesh.vertices[i].pos.z = (mesh.vertices[i].pos.z - center.z) * scale;
    }
}
