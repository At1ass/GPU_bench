#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include <cmath>

// GLSL 140: VS does vertex transforms, output captured by transform feedback.
// Rasterizer is disabled — pure vertex processing + streaming write to TF buffer.
static const char* TF_VS_140 = R"(
#version 140
in vec3 a_pos;
in vec3 a_normal;
out vec3 tf_position;
out vec3 tf_normal;

void main() {
    // Non-trivial vertex work to avoid being optimized away
    vec3 p = a_pos;
    p.x += sin(a_normal.x * 3.14) * 0.01;
    p.y += cos(a_normal.y * 3.14) * 0.01;
    p.z += sin(a_normal.z * 2.0 + a_pos.x) * 0.01;
    tf_position = p;
    tf_normal = normalize(a_normal + p * 0.001);
    gl_Position = vec4(p, 1.0);
}
)";

static const char* TF_FS_140 = R"(
#version 140
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
)";

// GLSL 150 core profile variants
static const char* TF_VS_150 = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
out vec3 tf_position;
out vec3 tf_normal;

void main() {
    vec3 p = a_pos;
    p.x += sin(a_normal.x * 3.14) * 0.01;
    p.y += cos(a_normal.y * 3.14) * 0.01;
    p.z += sin(a_normal.z * 2.0 + a_pos.x) * 0.01;
    tf_position = p;
    tf_normal = normalize(a_normal + p * 0.001);
    gl_Position = vec4(p, 1.0);
}
)";

static const char* TF_FS_150 = R"(
#version 150
out vec4 frag_color;
void main() {
    frag_color = vec4(1.0);
}
)";

// Multiple passes per frame to ensure stable timing
static const int TF_PASSES = 10;

// Varyings captured by transform feedback (must match VS outputs)
static const char* TF_VARYINGS[] = { "tf_position", "tf_normal" };
static const int TF_VARYING_COUNT = 2;

// Bytes per vertex written: 2 * vec3 = 24 bytes
static const int TF_BYTES_PER_VERTEX = 24;

TransformFeedbackTest::TransformFeedbackTest(const TransformFeedbackParams& params)
    : params_(params), actual_vertex_count_(0), mesh_(INVALID_MESH),
      shader_(INVALID_SHADER), tf_buffer_(INVALID_BUFFER) {}

const char* TransformFeedbackTest::name() const { return "TransformFeedback"; }
const char* TransformFeedbackTest::scoreUnit() const { return "Mverts/s"; }
const char* TransformFeedbackTest::description() const {
    return "Transform feedback vertex throughput (GL3+).\n"
           "Rasterization disabled, vertex outputs streamed to buffer.\n"
           "Measures vertex processing + TF write bandwidth.";
}

void TransformFeedbackTest::setupGL3(Renderer& r, GL3Features& gl3, int, int) {
    // Create a large mesh for vertex processing.
    // sphere generates (segs+1)*(rings+1) vertices.
    int target = params_.vertex_count;
    int segs = static_cast<int>(sqrt(static_cast<double>(target))) + 1;
    int rings = segs;
    if (segs > 1000) segs = 1000;
    if (rings > 1000) rings = 1000;

    MeshData md = MeshGen::sphere(segs, rings);
    actual_vertex_count_ = static_cast<int>(md.vertices.size());
    mesh_ = r.createMesh(md);

    // Create shader with transform feedback varyings set before link
    const char* vs = r.isCoreProfile() ? TF_VS_150 : TF_VS_140;
    const char* fs = r.isCoreProfile() ? TF_FS_150 : TF_FS_140;
    shader_ = gl3.createTransformFeedbackShader(vs, fs,
                                                TF_VARYINGS, TF_VARYING_COUNT);

    // TF buffer must hold all output: indices drawn * bytes_per_vertex.
    // drawMesh draws index_count vertices (with potential reuse via post-transform cache).
    // GL writes one output per vertex invocation, so buffer must hold index_count outputs.
    int index_count = static_cast<int>(md.indices.size());
    int tf_size = index_count * TF_BYTES_PER_VERTEX;
    tf_buffer_ = gl3.createTransformFeedbackBuffer(tf_size);
}

void TransformFeedbackTest::renderGL3(Renderer& r, GL3Features& gl3) {
    if (shader_ == INVALID_SHADER || mesh_ == INVALID_MESH || tf_buffer_ == INVALID_BUFFER) return;

    r.setDepthTest(false);
    r.setBlending(false);
    gl3.setRasterizerDiscard(true);
    r.useCustomShader(shader_);

    // Multiple passes for stable timing
    for (int pass = 0; pass < TF_PASSES; pass++) {
        gl3.beginTransformFeedback(tf_buffer_);
        r.drawMesh(mesh_);
        gl3.endTransformFeedback();
    }

    gl3.setRasterizerDiscard(false);
    r.setDepthTest(true);
}

void TransformFeedbackTest::cleanupGL3(Renderer& r, GL3Features& gl3) {
    if (tf_buffer_ != INVALID_BUFFER) {
        gl3.destroyTransformFeedbackBuffer(tf_buffer_);
        tf_buffer_ = INVALID_BUFFER;
    }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    if (mesh_ != INVALID_MESH) { r.destroyMesh(mesh_); mesh_ = INVALID_MESH; }
}

double TransformFeedbackTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    // Use actual vertex count, multiplied by passes
    double total_verts = static_cast<double>(actual_vertex_count_) * TF_PASSES;
    return total_verts / (avg_ms / 1000.0) / 1e6; // Mverts/s
}
