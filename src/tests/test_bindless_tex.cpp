#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include <vector>

// GLSL 4.30 + ARB_bindless_texture shader.
// Samples textures using bindless handles (no glBindTexture overhead).

static const char* BINDLESS_VS = R"(
#version 430
#extension GL_ARB_bindless_texture : require
in vec3 a_pos;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    gl_Position = vec4(a_pos, 1.0);
    v_uv = a_uv;
}
)";

static const char* BINDLESS_FS = R"(
#version 430
#extension GL_ARB_bindless_texture : require
in vec2 v_uv;
out vec4 frag_color;
layout(bindless_sampler) uniform sampler2D u_texture;

void main() {
    frag_color = texture(u_texture, v_uv);
}
)";

BindlessTexTest::BindlessTexTest(const BindlessTexParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH),
      shader_(INVALID_SHADER), u_tex_loc_(-1) {}

const char* BindlessTexTest::name() const { return "BindlessTex"; }
const char* BindlessTexTest::scoreUnit() const { return "ops/s"; }
const char* BindlessTexTest::description() const {
    return "Bindless texture throughput (ARB ext).\n"
           "Renders with bindless texture handles.\n"
           "Measures texture switching overhead elimination.";
}

void BindlessTexTest::setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    MeshData md = MeshGen::quad();
    quad_ = r.createMesh(md);

    shader_ = r.createCustomShader(BINDLESS_VS, BINDLESS_FS);
    if (shader_ == INVALID_SHADER) return;

    u_tex_loc_ = r.getCustomUniformLoc(shader_, "u_texture");

    // Create multiple small textures
    for (int i = 0; i < params_.tex_count; i++) {
        std::vector<unsigned char> pixels = genColorNoise(64, static_cast<unsigned int>(i + 1));
        TextureHandle tex = r.createTexture(64, 64, 4, pixels.data());
        textures_.push_back(tex);

        uint64_t handle = gl4.getBindlessHandle(tex);
        gl4.makeTextureResident(handle);
        handles_.push_back(handle);
    }
}

void BindlessTexTest::renderGL4(Renderer& r, GL4Features& gl4) {
    if (shader_ == INVALID_SHADER || quad_ == INVALID_MESH || handles_.empty()) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);
    r.setColorMask(false, false, false, false);

    int tex_count = static_cast<int>(handles_.size());
    for (int i = 0; i < params_.draws_per_frame; i++) {
        gl4.setUniformHandle(u_tex_loc_, handles_[i % tex_count]);
        r.drawMesh(quad_);
    }

    r.setColorMask(true, true, true, true);
    r.setDepthTest(true);
}

void BindlessTexTest::cleanupGL4(Renderer& r, GL4Features& gl4) {
    // Make all handles non-resident before destroying textures
    for (size_t i = 0; i < handles_.size(); i++) {
        gl4.makeTextureNonResident(handles_[i]);
    }
    handles_.clear();

    for (size_t i = 0; i < textures_.size(); i++) {
        r.destroyTexture(textures_[i]);
    }
    textures_.clear();

    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    if (quad_ != INVALID_MESH) { r.destroyMesh(quad_); quad_ = INVALID_MESH; }
}

double BindlessTexTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    double total_draws = static_cast<double>(params_.draws_per_frame);
    return total_draws / (avg_ms / 1000.0); // ops/s
}
