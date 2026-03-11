#include "tests.h"
#include "mesh_gen.h"
#include "compat.h"
#include <cstdio>
#include <cmath>

// Generate a simple fragment shader variant with unique color tint
static std::string makeFragShader(int variant) {
    char buf[512];
    float r = 0.5f + 0.3f * sinf(static_cast<float>(variant) * 1.1f);
    float g = 0.5f + 0.3f * sinf(static_cast<float>(variant) * 2.3f);
    float b = 0.5f + 0.3f * sinf(static_cast<float>(variant) * 3.7f);
    snprintf(buf, sizeof(buf),
        "#version 120\n"
        "uniform vec4 u_color;\n"
        "void main() {\n"
        "    gl_FragColor = u_color * vec4(%.3f, %.3f, %.3f, 1.0);\n"
        "}\n", r, g, b);
    return std::string(buf);
}

static const char* STATECHANGE_VS = R"(
#version 120
attribute vec2 a_pos;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

StateChangeTest::StateChangeTest(const StateChangeParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH) {}

const char* StateChangeTest::name() const { return "StateChange"; }
const char* StateChangeTest::scoreUnit() const { return "Kcalls/s"; }
const char* StateChangeTest::description() const {
    return "Alternates shader/texture/blend state between draws.\n"
           "Measures state-switching overhead.";
}

void StateChangeTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r->createMesh(MeshGen::quad());

    // Create multiple custom shaders
    shaders_.resize(params_.shader_count);
    for (int i = 0; i < params_.shader_count; i++) {
        std::string fs = makeFragShader(i);
        shaders_[i] = r->createCustomShader(STATECHANGE_VS, fs.c_str());
    }

    // Create multiple textures
    textures_.resize(params_.tex_count);
    for (int i = 0; i < params_.tex_count; i++) {
        int sz = 64; // small textures, the point is switching not texturing
        std::vector<unsigned char> pix = genColorNoise(sz, 100 + i);
        textures_[i] = r->createTexture(sz, sz, 3, pix.data());
    }
}

void StateChangeTest::render(Renderer* r) {
    r->setDepthTest(false);

    int shader_count = static_cast<int>(shaders_.size());
    int tex_count = static_cast<int>(textures_.size());

    for (int i = 0; i < params_.switches; i++) {
        // Switch shader
        int si = i % shader_count;
        r->useCustomShader(shaders_[si]);

        int u_color = r->getCustomUniformLoc(shaders_[si], "u_color");
        float t = static_cast<float>(i) / params_.switches;
        float c = 0.5f + 0.5f * sinf(t * 6.28f);
        r->setUniform4f(u_color, c, c, c, 1.0f);

        // Switch texture
        int ti = i % tex_count;
        r->bindTexture(textures_[ti]);

        // Toggle blending
        r->setBlending((i % 2) == 0);

        r->drawMesh(quad_);
    }

    r->setBlending(false);
}

void StateChangeTest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    quad_ = INVALID_MESH;

    for (size_t i = 0; i < shaders_.size(); i++)
        r->destroyCustomShader(shaders_[i]);
    shaders_.clear();

    for (size_t i = 0; i < textures_.size(); i++)
        r->destroyTexture(textures_[i]);
    textures_.clear();
}

double StateChangeTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    return static_cast<double>(params_.switches) / (avg_ms / 1000.0) / 1e3; // Kcalls/s
}
