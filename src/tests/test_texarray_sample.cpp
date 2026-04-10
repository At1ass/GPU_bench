#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include "platform/shader_compat.h"
#include <string>

// Shader bodies WITHOUT #version (injected via testShaderPreamble140)
static const char* TEXARRAY_VS_BODY = R"(
in vec3 a_pos;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

static const char* TEXARRAY_FS_BODY = R"(
in vec2 v_uv;
uniform sampler2DArray u_tex_array;
uniform float u_layer;
out vec4 frag_color;
void main() {
    frag_color = texture(u_tex_array, vec3(v_uv, u_layer));
}
)";

TexArraySampleTest::TexArraySampleTest(const TexArraySampleParams& params)
    : params_(params), actual_tex_size_(0), vw_(0), vh_(0),
      quad_(INVALID_MESH), tex_array_(INVALID_TEXTURE), shader_(INVALID_SHADER),
      u_layer_loc_(-1) {}

const char* TexArraySampleTest::name() const { return "TexArraySample"; }
const char* TexArraySampleTest::scoreUnit() const { return "MTex/s"; }
const char* TexArraySampleTest::description() const {
    return "Texture array sampling throughput (GL3+).\n"
           "Samples from a 2D texture array (sampler2DArray).\n"
           "Tests layer indexing impact on texture cache.";
}

void TexArraySampleTest::setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    actual_tex_size_ = clampTexSize(params_.tex_size, r.getCaps().max_texture_size);

    quad_ = r.createMesh(MeshGen::quad());
    std::string vs = std::string(testShaderPreamble140(&r)) + TEXARRAY_VS_BODY;
    std::string fs = std::string(testShaderPreamble140(&r)) + TEXARRAY_FS_BODY;
    shader_ = r.createCustomShader(vs.c_str(), fs.c_str());
    if (shader_ != INVALID_SHADER) {
        u_layer_loc_ = r.getCustomUniformLoc(shader_, "u_layer");
    }

    // Generate texture data for all array layers
    size_t layer_size = static_cast<size_t>(actual_tex_size_) * static_cast<size_t>(actual_tex_size_) * 3;
    std::vector<unsigned char> pixels(layer_size * static_cast<size_t>(params_.array_layers));
    unsigned int seed = 42;
    for (size_t i = 0; i < pixels.size(); i++) {
        seed = seed * 1664525u + 1013904223u;
        pixels[i] = static_cast<unsigned char>(seed >> 24);
    }

    tex_array_ = gl3.createTextureArray(actual_tex_size_, actual_tex_size_,
                                        params_.array_layers, 3, pixels.data());
    LOG_DBG("Test '%s': setup complete (tex %dx%d, %d array layers, %d samples)", name(), actual_tex_size_, actual_tex_size_, params_.array_layers, params_.sample_layers);
}

void TexArraySampleTest::renderGL3(Renderer& r, GL3Features& gl3) {
    if (shader_ == INVALID_SHADER || tex_array_ == INVALID_TEXTURE) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);

    // Bind texture array to texture unit 0 as GL_TEXTURE_2D_ARRAY
    int u_tex_loc = r.getCustomUniformLoc(shader_, "u_tex_array");
    r.setUniform1i(u_tex_loc, 0);
    gl3.bindTextureArray(tex_array_);

    for (int i = 0; i < params_.sample_layers; i++) {
        float layer = static_cast<float>(i % params_.array_layers);
        r.setUniform1f(u_layer_loc_, layer);
        r.drawMesh(quad_);
    }

    r.setDepthTest(true);
}

void TexArraySampleTest::cleanupGL3(Renderer& r, GL3Features&) {
    if (tex_array_ != INVALID_TEXTURE) { r.destroyTexture(tex_array_); tex_array_ = INVALID_TEXTURE; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    r.destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double TexArraySampleTest::computeScore(const std::vector<double>& times, int vw, int vh) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double texels_per_frame = static_cast<double>(vw) * vh * params_.sample_layers;
    return texels_per_frame / (avg_ms / 1000.0) / 1e6; // MTex/s
}
