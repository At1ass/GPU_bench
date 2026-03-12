#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include <vector>

// GLSL 4.00 textureGather test.
// Compares textureGather (4 texels in one call) vs 4x texelFetch.

static const char* GATHER_VS = R"(
#version 400
in vec3 a_pos;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    gl_Position = vec4(a_pos, 1.0);
    v_uv = a_uv;
}
)";

static const char* GATHER_FS = R"(
#version 400
in vec2 v_uv;
out vec4 frag_color;
uniform sampler2D u_texture;
uniform int u_iterations;

void main() {
    vec4 acc = vec4(0.0);
    for (int i = 0; i < u_iterations; i++) {
        acc += textureGather(u_texture, v_uv + vec2(float(i) * 0.001, 0.0), 0);
    }
    frag_color = acc * 0.001;
}
)";

TextureGatherTest::TextureGatherTest(const TextureGatherParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH),
      texture_(INVALID_TEXTURE), shader_(INVALID_SHADER), u_iterations_loc_(-1) {}

const char* TextureGatherTest::name() const { return "TextureGather"; }
const char* TextureGatherTest::scoreUnit() const { return "MTex/s"; }
const char* TextureGatherTest::description() const {
    return "textureGather throughput (GL 4.0+).\n"
           "Fetches 4 texels per gather call.\n"
           "Measures texture gather unit performance.";
}

void TextureGatherTest::setupGL4(Renderer& r, GL4Features&, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    MeshData md = MeshGen::quad();
    quad_ = r.createMesh(md);

    int sz = params_.tex_size;
    int clamped = clampTexSize(sz, r.getCaps().max_texture_size);
    std::vector<unsigned char> pixels = genColorNoise(clamped, 42);
    texture_ = r.createTexture(clamped, clamped, 4, pixels.data());

    shader_ = r.createCustomShader(GATHER_VS, GATHER_FS);
    if (shader_ != INVALID_SHADER) {
        u_iterations_loc_ = r.getCustomUniformLoc(shader_, "u_iterations");
    }
}

void TextureGatherTest::renderGL4(Renderer& r, GL4Features&) {
    if (shader_ == INVALID_SHADER || quad_ == INVALID_MESH) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);
    r.bindTexture(texture_);
    r.setUniform1i(u_iterations_loc_, params_.iterations);

    r.drawMesh(quad_);
}

void TextureGatherTest::cleanupGL4(Renderer& r, GL4Features&) {
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    if (texture_ != INVALID_TEXTURE) { r.destroyTexture(texture_); texture_ = INVALID_TEXTURE; }
    if (quad_ != INVALID_MESH) { r.destroyMesh(quad_); quad_ = INVALID_MESH; }
}

double TextureGatherTest::computeScore(const std::vector<double>& times, int vw, int vh) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Each pixel executes iterations gathers, each gather fetches 4 texels
    double total_pixels = static_cast<double>(vw) * vh;
    double total_texels = total_pixels * params_.iterations * 4.0;
    return total_texels / (avg_ms / 1000.0) / 1e6; // MTex/s
}
