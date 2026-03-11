#include "tests.h"
#include "mesh_gen.h"
#include <cmath>

// ---- Shared texture generation helpers ----

std::vector<unsigned char> genCheckerboard(int size, int check_size) {
    std::vector<unsigned char> pixels(size * size * 3);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool white = ((x / check_size) + (y / check_size)) % 2 == 0;
            unsigned char c = white ? 220 : 40;
            int idx = (y * size + x) * 3;
            pixels[idx] = c;
            pixels[idx + 1] = c;
            pixels[idx + 2] = c;
        }
    }
    return pixels;
}

std::vector<unsigned char> genColorNoise(int size, unsigned int seed) {
    std::vector<unsigned char> pixels(size * size * 3);
    unsigned int s = seed;
    for (int i = 0; i < size * size * 3; i++) {
        s = s * 1664525u + 1013904223u;
        pixels[i] = static_cast<unsigned char>(s >> 24);
    }
    return pixels;
}

int clampTexSize(int requested, int max_size) {
    int s = requested;
    if (s > max_size) s = max_size;
    int pot = 1;
    while (pot < s) pot *= 2;
    if (pot > max_size) pot /= 2;
    return pot > 0 ? pot : 1;
}

// ===== FillrateTest =====

FillrateTest::FillrateTest(const FillrateParams& params)
    : layers_(params.layers), vw_(0), vh_(0), quad_(INVALID_MESH) {}

const char* FillrateTest::name() const { return "Fillrate"; }
const char* FillrateTest::scoreUnit() const { return "Mpix/s"; }
const char* FillrateTest::description() const {
    return "Renders overlapping opaque fullscreen quads.\n"
           "Measures raw pixel output rate without blend overhead.";
}

void FillrateTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r->createMesh(MeshGen::quad());
}

void FillrateTest::render(Renderer* r) {
    r->setDepthTest(false);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Color2D);

    for (int i = 0; i < layers_; i++) {
        float t = static_cast<float>(i) / layers_;
        r->setColor(
            0.5f + 0.5f * sinf(t * 6.28f),
            0.5f + 0.5f * sinf(t * 6.28f + 2.09f),
            0.5f + 0.5f * sinf(t * 6.28f + 4.19f),
            1.0f
        );
        r->drawMesh(quad_);
    }

    r->setDepthTest(true);
}

void FillrateTest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    quad_ = INVALID_MESH;
}

double FillrateTest::computeScore(const std::vector<double>& times, int vw, int vh) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double pixels_per_frame = static_cast<double>(vw) * vh * layers_;
    return pixels_per_frame / (avg_ms / 1000.0) / 1e6;
}
