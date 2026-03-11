#include "tests/tests.h"
#include "geometry/mesh_gen.h"
#include <cmath>

OverdrawTest::OverdrawTest(const OverdrawParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH) {}

const char* OverdrawTest::name() const { return "Overdraw"; }
const char* OverdrawTest::scoreUnit() const { return "Mpix/s"; }
const char* OverdrawTest::description() const {
    return "Fullscreen quads with alpha blending.\n"
           "Measures pure blend bandwidth (isolated from Fillrate).";
}

void OverdrawTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r->createMesh(MeshGen::quad());
}

void OverdrawTest::render(Renderer* r) {
    r->setDepthTest(false);
    r->setBlending(true);
    r->useShader(Renderer::ShaderType::Color2D);

    for (int i = 0; i < params_.layers; i++) {
        // Uniform color with configurable alpha
        float t = static_cast<float>(i) / static_cast<float>(params_.layers);
        r->setColor(
            0.5f + 0.3f * sinf(t * 6.28f),
            0.5f + 0.3f * sinf(t * 6.28f + 2.09f),
            0.5f + 0.3f * sinf(t * 6.28f + 4.19f),
            params_.alpha
        );
        r->drawMesh(quad_);
    }

    r->setBlending(false);
}

void OverdrawTest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double OverdrawTest::computeScore(const std::vector<double>& times, int vw, int vh) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double pixels_per_frame = static_cast<double>(vw) * vh * params_.layers;
    return pixels_per_frame / (avg_ms / 1000.0) / 1e6;
}
