#include "tests/tests.h"
#include "geometry/mesh_gen.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include "platform/shader_compat.h"
#include <cstdio>
#include <cmath>
#include <string>

static std::string makeFragShader(const Renderer* r, int variant) {
    bool legacy = testShaderUsesLegacy(r);
    char buf[512];
    float rv = 0.5f + 0.3f * sinf(static_cast<float>(variant) * 1.1f);
    float g  = 0.5f + 0.3f * sinf(static_cast<float>(variant) * 2.3f);
    float b  = 0.5f + 0.3f * sinf(static_cast<float>(variant) * 3.7f);
    snprintf(buf, sizeof(buf),
        "%s"
        "uniform vec4 u_color;\n"
        "%s"
        "void main() {\n"
        "    %s = u_color * vec4(%.3f, %.3f, %.3f, 1.0);\n"
        "}\n",
        testShaderPreamble120(r),
        legacy ? "" : "out vec4 fragColor;\n",
        legacy ? "gl_FragColor" : "fragColor",
        static_cast<double>(rv), static_cast<double>(g), static_cast<double>(b));
    return std::string(buf);
}

static const char* STATECHANGE_VS_LEGACY = R"(
attribute vec2 a_pos;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

static const char* STATECHANGE_VS_MODERN = R"(
in vec2 a_pos;
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
    bool legacy = testShaderUsesLegacy(r);
    std::string vs_src = std::string(testShaderPreamble120(r))
        + (legacy ? STATECHANGE_VS_LEGACY : STATECHANGE_VS_MODERN);
    shaders_.resize(static_cast<size_t>(params_.shader_count));
    for (int i = 0; i < params_.shader_count; i++) {
        std::string fs = makeFragShader(r, i);
        shaders_[static_cast<size_t>(i)] = r->createCustomShader(vs_src.c_str(), fs.c_str());
    }

    // Create multiple textures
    textures_.resize(static_cast<size_t>(params_.tex_count));
    for (int i = 0; i < params_.tex_count; i++) {
        int sz = 64; // small textures, the point is switching not texturing
        auto pix = genColorNoise(sz, static_cast<unsigned int>(100 + i));
        textures_[static_cast<size_t>(i)] = r->createTexture(sz, sz, 3, pix.data());
    }
    LOG_DBG("Test '%s': setup complete (%d switches, %d shaders, %d textures)",
             name(), params_.switches, params_.shader_count, params_.tex_count);
}

void StateChangeTest::render(Renderer* r) {
    r->setDepthTest(false);

    int shader_count = static_cast<int>(shaders_.size());
    int tex_count = static_cast<int>(textures_.size());

    for (int i = 0; i < params_.switches; i++) {
        // Switch shader
        int si = i % shader_count;
        r->useCustomShader(shaders_[static_cast<size_t>(si)]);

        int u_color = r->getCustomUniformLoc(shaders_[static_cast<size_t>(si)], "u_color");
        float t = static_cast<float>(i) / static_cast<float>(params_.switches);
        float c = 0.5f + 0.5f * sinf(t * 6.28f);
        r->setUniform4f(u_color, c, c, c, 1.0f);

        // Switch texture
        int ti = i % tex_count;
        r->bindTexture(textures_[static_cast<size_t>(ti)]);

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
    if (avg_ms <= 0.0) return 0.0;
    return static_cast<double>(params_.switches) / (avg_ms / 1000.0) / 1e3; // Kcalls/s
}
