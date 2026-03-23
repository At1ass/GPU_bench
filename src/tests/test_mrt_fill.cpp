#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>

// GLSL 150 shader that writes to multiple render targets
static const char* MRT_VS = R"(
#version 150
in vec3 a_pos;
void main() {
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

// Fragment shader for 2 targets
static const char* MRT_FS_2 = R"(
#version 150
uniform vec4 u_color;
out vec4 out_color0;
out vec4 out_color1;
void main() {
    out_color0 = u_color;
    out_color1 = vec4(1.0) - u_color;
}
)";

// Fragment shader for 3 targets
static const char* MRT_FS_3 = R"(
#version 150
uniform vec4 u_color;
out vec4 out_color0;
out vec4 out_color1;
out vec4 out_color2;
void main() {
    out_color0 = u_color;
    out_color1 = vec4(1.0) - u_color;
    out_color2 = u_color * 0.5 + vec4(0.25);
}
)";

// Fragment shader for 4 targets
static const char* MRT_FS_4 = R"(
#version 150
uniform vec4 u_color;
out vec4 out_color0;
out vec4 out_color1;
out vec4 out_color2;
out vec4 out_color3;
void main() {
    out_color0 = u_color;
    out_color1 = vec4(1.0) - u_color;
    out_color2 = u_color * 0.5 + vec4(0.25);
    out_color3 = u_color.bgra;
}
)";

MRTFillTest::MRTFillTest(const MRTFillParams& params)
    : params_(params), rt_size_(0), quad_(INVALID_MESH),
      rt_(INVALID_RENDER_TARGET), shader_(INVALID_SHADER) {}

const char* MRTFillTest::name() const { return "MRTFill"; }
const char* MRTFillTest::scoreUnit() const { return "MPix/s"; }
const char* MRTFillTest::description() const {
    return "Multiple render target fill throughput (GL3+).\n"
           "Writes to 2-4 color attachments simultaneously.\n"
           "Key metric for deferred rendering (G-buffer) performance.";
}

void MRTFillTest::setupGL3(Renderer& r, GL3Features& gl3, int, int) {
    rt_size_ = clampTexSize(params_.rt_size, r.getCaps().max_texture_size);
    quad_ = r.createMesh(MeshGen::quad());

    // Choose shader based on target count
    const char* fs = MRT_FS_2;
    if (params_.num_targets >= 4) fs = MRT_FS_4;
    else if (params_.num_targets >= 3) fs = MRT_FS_3;

    shader_ = r.createCustomShader(MRT_VS, fs);
    rt_ = gl3.createMRTRenderTarget(rt_size_, rt_size_, params_.num_targets);
    LOG_DBG("Test '%s': setup complete (RT %dx%d, %d targets, %d layers)", name(), rt_size_, rt_size_, params_.num_targets, params_.layers);
}

void MRTFillTest::renderGL3(Renderer& r, GL3Features&) {
    if (rt_ == INVALID_RENDER_TARGET || shader_ == INVALID_SHADER) return;

    r.bindRenderTarget(rt_);
    r.setViewport(0, 0, rt_size_, rt_size_);
    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);

    int u_color_loc = r.getCustomUniformLoc(shader_, "u_color");

    for (int i = 0; i < params_.layers; i++) {
        float t = static_cast<float>(i) / static_cast<float>(params_.layers);
        r.setUniform4f(u_color_loc,
            0.5f + 0.5f * sinf(t * 6.28f),
            0.5f + 0.5f * sinf(t * 6.28f + 2.09f),
            0.5f + 0.5f * sinf(t * 6.28f + 4.19f),
            1.0f
        );
        r.drawMesh(quad_);
    }

    r.bindRenderTarget(RenderTargetHandle(0));
    r.setDepthTest(true);
}

void MRTFillTest::cleanupGL3(Renderer& r, GL3Features&) {
    if (rt_ != INVALID_RENDER_TARGET) { r.destroyRenderTarget(rt_); rt_ = INVALID_RENDER_TARGET; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    r.destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double MRTFillTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double pixels_per_frame = static_cast<double>(rt_size_) * rt_size_ * params_.num_targets * params_.layers;
    return pixels_per_frame / (avg_ms / 1000.0) / 1e6;
}
