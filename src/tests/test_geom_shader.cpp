#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>

// GLSL 150: VS passes through, GS takes triangles and amplifies
static const char* GS_VS = R"(
#version 150
in vec3 a_pos;
void main() {
    gl_Position = vec4(a_pos, 1.0);
}
)";

// GS: input triangles, emit additional triangle strips per input primitive.
// This tests geometry shader throughput with amplification.
static const char* GS_GS = R"(
#version 150
layout(triangles) in;
layout(triangle_strip, max_vertices = 24) out;
out vec3 v_color;

uniform int u_tris_per_point;

void main() {
    vec4 center = (gl_in[0].gl_Position + gl_in[1].gl_Position + gl_in[2].gl_Position) / 3.0;
    float s = 0.003;
    int pairs = u_tris_per_point / 2;
    if (pairs < 1) pairs = 1;
    if (pairs > 6) pairs = 6;

    for (int p = 0; p < pairs; p++) {
        float angle = float(p) * 1.047;
        float ox = cos(angle) * s * float(p);
        float oy = sin(angle) * s * float(p);
        v_color = vec3(0.3 + float(p) * 0.1, 0.5, 0.7);

        gl_Position = center + vec4(-s + ox, -s + oy, 0.0, 0.0);
        EmitVertex();
        gl_Position = center + vec4( s + ox, -s + oy, 0.0, 0.0);
        EmitVertex();
        gl_Position = center + vec4(-s + ox,  s + oy, 0.0, 0.0);
        EmitVertex();
        gl_Position = center + vec4( s + ox,  s + oy, 0.0, 0.0);
        EmitVertex();
        EndPrimitive();
    }
}
)";

static const char* GS_FS = R"(
#version 150
in vec3 v_color;
out vec4 frag_color;
void main() {
    frag_color = vec4(v_color, 1.0);
}
)";

GeometryShaderTest::GeometryShaderTest(const GeometryShaderParams& params)
    : params_(params), vw_(0), vh_(0), points_mesh_(INVALID_MESH), shader_(INVALID_SHADER) {}

const char* GeometryShaderTest::name() const { return "GeomShader"; }
const char* GeometryShaderTest::scoreUnit() const { return "Mtri/s"; }
const char* GeometryShaderTest::description() const {
    return "Geometry shader amplification (GL 3.2+).\n"
           "Triangles in, amplified triangle strips out.\n"
           "Tests geometry pipeline throughput.";
}

void GeometryShaderTest::setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    shader_ = gl3.createShaderVGF(GS_VS, GS_GS, GS_FS);

    // Create a grid of small triangles as input primitives.
    // input_points parameter = number of input triangles we want.
    int num_tris = params_.input_points;
    MeshData md;
    unsigned int seed = 12345;
    for (int i = 0; i < num_tris; i++) {
        seed = seed * 1664525u + 1013904223u;
        float cx = (static_cast<float>(seed >> 16) / 32768.0f) - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float cy = (static_cast<float>(seed >> 16) / 32768.0f) - 1.0f;
        float s = 0.001f;
        unsigned int base = static_cast<unsigned int>(md.vertices.size());

        Vertex v;
        v.normal = Vec3(0, 0, 1);
        v.uv = Vec2(0, 0);

        v.pos = Vec3(cx - s, cy - s, 0);
        md.vertices.push_back(v);
        v.pos = Vec3(cx + s, cy - s, 0);
        v.uv = Vec2(1, 0);
        md.vertices.push_back(v);
        v.pos = Vec3(cx, cy + s, 0);
        v.uv = Vec2(0.5f, 1);
        md.vertices.push_back(v);

        md.indices.push_back(base);
        md.indices.push_back(base + 1);
        md.indices.push_back(base + 2);
    }
    points_mesh_ = r.createMesh(md);
    Log::dbg("Test '%s': setup complete (%d input tris, %d tris/point)", name(), params_.input_points, params_.tris_per_point);
}

void GeometryShaderTest::renderGL3(Renderer& r, GL3Features&) {
    if (shader_ == INVALID_SHADER || points_mesh_ == INVALID_MESH) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);

    int u_tris = r.getCustomUniformLoc(shader_, "u_tris_per_point");
    r.setUniform1i(u_tris, params_.tris_per_point);

    r.drawMesh(points_mesh_);
    r.setDepthTest(true);
}

void GeometryShaderTest::cleanupGL3(Renderer& r, GL3Features&) {
    if (points_mesh_ != INVALID_MESH) { r.destroyMesh(points_mesh_); points_mesh_ = INVALID_MESH; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
}

double GeometryShaderTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    // Each input triangle produces tris_per_point output triangles
    double total_tris = static_cast<double>(params_.input_points) * params_.tris_per_point;
    return total_tris / (avg_ms / 1000.0) / 1e6; // Mtri/s
}
