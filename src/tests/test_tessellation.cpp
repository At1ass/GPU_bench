#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>

// GLSL 4.00 tessellation throughput test.
// Measures tessellation shader throughput by subdividing patches.

static const char* TESS_VS = R"(
#version 400
in vec3 a_pos;
void main() {
    gl_Position = vec4(a_pos * 0.001, 1.0);
}
)";

static const char* TESS_TCS = R"(
#version 400
layout(vertices = 3) out;
uniform int u_tess_level;
void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    if (gl_InvocationID == 0) {
        gl_TessLevelInner[0] = float(u_tess_level);
        gl_TessLevelOuter[0] = float(u_tess_level);
        gl_TessLevelOuter[1] = float(u_tess_level);
        gl_TessLevelOuter[2] = float(u_tess_level);
    }
}
)";

static const char* TESS_TES = R"(
#version 400
layout(triangles, equal_spacing) in;
void main() {
    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position
                + gl_TessCoord.y * gl_in[1].gl_Position
                + gl_TessCoord.z * gl_in[2].gl_Position;
}
)";

static const char* TESS_FS = R"(
#version 400
out vec4 frag_color;
void main() {
    frag_color = vec4(0.5, 0.5, 0.5, 1.0);
}
)";

static const int TESS_PASSES = 10;

TessellationTPTest::TessellationTPTest(const TessellationTPParams& params)
    : params_(params), vw_(0), vh_(0), mesh_(INVALID_MESH), shader_(INVALID_SHADER) {}

const char* TessellationTPTest::name() const { return "TessellationTP"; }
const char* TessellationTPTest::scoreUnit() const { return "Mtri/s"; }
const char* TessellationTPTest::description() const {
    return "Tessellation shader throughput (GL 4.0+).\n"
           "Subdivides triangle patches on the GPU.\n"
           "Measures tessellation unit performance.";
}

void TessellationTPTest::setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    // Create a sphere as patch input (provides triangle patches)
    MeshData md = MeshGen::sphere(32, 16);
    mesh_ = r.createMesh(md);

    shader_ = gl4.createTessShader(TESS_VS, TESS_TCS, TESS_TES, TESS_FS);
    if (shader_ != INVALID_SHADER) {
        int loc = r.getCustomUniformLoc(shader_, "u_tess_level");
        r.useCustomShader(shader_);
        r.setUniform1i(loc, params_.patch_subdivisions);
    }
    LOG_DBG("Test '%s': setup complete (%d subdivisions, %d patches)", name(), params_.patch_subdivisions, params_.patches);
}

void TessellationTPTest::renderGL4(Renderer& r, GL4Features& gl4) {
    if (shader_ == INVALID_SHADER || mesh_ == INVALID_MESH) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);
    r.setColorMask(false, false, false, false);

    gl4.setPatchVertices(3);

    for (int pass = 0; pass < TESS_PASSES; pass++) {
        r.drawMesh(mesh_);
    }

    r.setColorMask(true, true, true, true);
    r.setDepthTest(true);
}

void TessellationTPTest::cleanupGL4(Renderer& r, GL4Features&) {
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    if (mesh_ != INVALID_MESH) { r.destroyMesh(mesh_); mesh_ = INVALID_MESH; }
}

double TessellationTPTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Each input triangle generates approximately (tess_level^2) output triangles
    int tess = params_.patch_subdivisions;
    double tris_per_patch = static_cast<double>(tess) * tess;
    double total_tris = static_cast<double>(params_.patches) * tris_per_patch * TESS_PASSES;
    return total_tris / (avg_ms / 1000.0) / 1e6; // Mtri/s
}
