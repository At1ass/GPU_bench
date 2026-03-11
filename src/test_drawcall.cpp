#include "tests.h"
#include "mesh_gen.h"
#include <cmath>

DrawCallTest::DrawCallTest(const DrawCallParams& params)
    : params_(params), vw_(0), vh_(0) {}

const char* DrawCallTest::name() const { return "DrawCall"; }
const char* DrawCallTest::scoreUnit() const { return "Kcalls/s"; }
const char* DrawCallTest::description() const {
    return "Submits many small draw calls with constant state.\n"
           "Measures driver/CPU overhead of draw call submission.";
}

void DrawCallTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    // Create N small cube meshes (separate VBOs)
    MeshData unit = MeshGen::cube();
    meshes_.resize(params_.mesh_count);
    for (int i = 0; i < params_.mesh_count; i++) {
        meshes_[i] = r->createMesh(unit);
    }
}

void DrawCallTest::render(Renderer* r) {
    r->setDepthTest(true);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Scene3D);

    float aspect = static_cast<float>(vw_) / vh_;
    r->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 500.0f));
    r->setView(Mat4::lookAt(Vec3(0, 20, 40), Vec3(0, 0, 0), Vec3(0, 1, 0)));
    r->setColor(0.6f, 0.7f, 0.8f, 1.0f);
    r->setUseTexture(false);
    r->setLightDir(0.5f, 0.8f, 0.3f);

    int mesh_count = static_cast<int>(meshes_.size());
    for (int i = 0; i < params_.draws_per_frame; i++) {
        // Spread cubes in a grid pattern
        int idx = i % mesh_count;
        float x = static_cast<float>(i % 50) * 1.5f - 37.5f;
        float z = static_cast<float>((i / 50) % 50) * 1.5f - 37.5f;
        float y = sinf(static_cast<float>(i) * 0.1f) * 2.0f;
        r->setModel(Mat4::translate(x, y, z) * Mat4::scale(0.3f, 0.3f, 0.3f));
        r->drawMesh(meshes_[idx]);
    }
}

void DrawCallTest::cleanup(Renderer* r) {
    for (size_t i = 0; i < meshes_.size(); i++) {
        r->destroyMesh(meshes_[i]);
    }
    meshes_.clear();
}

double DrawCallTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    return static_cast<double>(params_.draws_per_frame) / (avg_ms / 1000.0) / 1e3; // Kcalls/s
}
