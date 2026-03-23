#include "tests/tests.h"
#include "renderer/features.h"
#include "geometry/mesh_gen.h"
#include <cmath>

FBOFillrateTest::FBOFillrateTest(const FBOFillrateParams& params)
    : params_(params), rt_size_(0), quad_(INVALID_MESH), rt_(INVALID_RENDER_TARGET) {}

const char* FBOFillrateTest::name() const { return "FBOFillrate"; }
const char* FBOFillrateTest::scoreUnit() const { return "MPix/s"; }
const char* FBOFillrateTest::description() const {
    return "Fillrate rendering to FBO (render-to-texture).\n"
           "Compares with Fillrate test to show FBO overhead.\n"
           "Uses existing createRenderTarget/bindRenderTarget.";
}

void FBOFillrateTest::setupGL3(Renderer& r, GL3Features&, int, int) {
    rt_size_ = clampTexSize(params_.rt_size, r.getCaps().max_texture_size);
    quad_ = r.createMesh(MeshGen::quad());
    rt_ = r.createRenderTarget(rt_size_, rt_size_);
}

void FBOFillrateTest::renderGL3(Renderer& r, GL3Features&) {
    if (rt_ == INVALID_RENDER_TARGET) return;

    r.bindRenderTarget(rt_);
    r.setViewport(0, 0, rt_size_, rt_size_);
    r.setDepthTest(false);
    r.setBlending(false);
    r.useShader(Renderer::ShaderType::Color2D);

    for (int i = 0; i < params_.layers; i++) {
        float t = static_cast<float>(i) / static_cast<float>(params_.layers);
        r.setColor(
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

void FBOFillrateTest::cleanupGL3(Renderer& r, GL3Features&) {
    if (rt_ != INVALID_RENDER_TARGET) { r.destroyRenderTarget(rt_); rt_ = INVALID_RENDER_TARGET; }
    r.destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double FBOFillrateTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double pixels_per_frame = static_cast<double>(rt_size_) * rt_size_ * params_.layers;
    return pixels_per_frame / (avg_ms / 1000.0) / 1e6;
}
