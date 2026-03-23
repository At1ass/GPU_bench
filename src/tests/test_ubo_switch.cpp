#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>
#include <vector>

// GLSL 140 shader with a uniform block
static const char* UBO_VS_140 = R"(
#version 140
in vec3 a_pos;
void main() {
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

static const char* UBO_FS_140 = R"(
#version 140
layout(std140) uniform ColorBlock {
    vec4 u_color;
};
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)";

// GLSL 150 core profile variants
static const char* UBO_VS_150 = R"(
#version 150
in vec3 a_pos;
void main() {
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

static const char* UBO_FS_150 = R"(
#version 150
layout(std140) uniform ColorBlock {
    vec4 u_color;
};
out vec4 frag_color;
void main() {
    frag_color = u_color;
}
)";

UBOSwitchTest::UBOSwitchTest(const UBOSwitchParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH), shader_(INVALID_SHADER) {}

const char* UBOSwitchTest::name() const { return "UBOSwitch"; }
const char* UBOSwitchTest::scoreUnit() const { return "ops/s"; }
const char* UBOSwitchTest::description() const {
    return "UBO binding switch overhead (GL3+).\n"
           "Rapidly switches uniform buffer bindings between draws.\n"
           "Measures driver constant buffer rebind efficiency.";
}

void UBOSwitchTest::setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r.createMesh(MeshGen::quad());
    const char* vs = r.isCoreProfile() ? UBO_VS_150 : UBO_VS_140;
    const char* fs = r.isCoreProfile() ? UBO_FS_150 : UBO_FS_140;
    shader_ = r.createCustomShader(vs, fs);

    // Create UBOs with different colors
    for (int i = 0; i < params_.ubo_count; i++) {
        float t = static_cast<float>(i) / static_cast<float>(params_.ubo_count);
        float color[4] = {
            0.5f + 0.5f * sinf(t * 6.28f),
            0.5f + 0.5f * sinf(t * 6.28f + 2.09f),
            0.5f + 0.5f * sinf(t * 6.28f + 4.19f),
            1.0f
        };
        BufferHandle ubo = gl3.createUBO(sizeof(color));
        if (ubo != INVALID_BUFFER) {
            gl3.updateUBO(ubo, color, sizeof(color));
            ubos_.push_back(ubo);
        }
    }
    Log::dbg("Test '%s': setup complete (%d UBOs, %d switches/frame)", name(), static_cast<int>(ubos_.size()), params_.switches_per_frame);
}

void UBOSwitchTest::renderGL3(Renderer& r, GL3Features& gl3) {
    if (shader_ == INVALID_SHADER || ubos_.empty()) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);
    r.setColorMask(false, false, false, false); // Minimize fill cost

    int num_ubos = static_cast<int>(ubos_.size());
    for (int i = 0; i < params_.switches_per_frame; i++) {
        gl3.bindUBO(ubos_[static_cast<size_t>(i % num_ubos)], 0);
        r.drawMesh(quad_);
    }

    r.setColorMask(true, true, true, true);
    r.setDepthTest(true);
}

void UBOSwitchTest::cleanupGL3(Renderer& r, GL3Features& gl3) {
    for (auto ubo : ubos_) {
        gl3.destroyUBO(ubo);
    }
    ubos_.clear();
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    r.destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double UBOSwitchTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    return params_.switches_per_frame / (avg_ms / 1000.0); // ops/s
}
