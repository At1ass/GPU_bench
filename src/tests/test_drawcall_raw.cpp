#include "tests/tests.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>

DrawCallRawTest::DrawCallRawTest(const DrawCallParams& params)
    : params_(params), vw_(0), vh_(0) {}

const char* DrawCallRawTest::name() const { return "DrawCallRaw"; }
const char* DrawCallRawTest::scoreUnit() const { return "Kcalls/s"; }
const char* DrawCallRawTest::description() const {
    return "Submits many small draw calls without per-draw uniform updates.\n"
           "Measures pure draw call submission overhead.";
}

void DrawCallRawTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    MeshData unit = MeshGen::cube();
    meshes_.resize(static_cast<size_t>(params_.mesh_count));
    for (size_t i = 0; i < static_cast<size_t>(params_.mesh_count); i++) {
        meshes_[i] = r->createMesh(unit);
    }
    Log::dbg("Test '%s': setup complete (%d meshes, %d draws/frame)", name(), params_.mesh_count, params_.draws_per_frame);
}

void DrawCallRawTest::render(Renderer* r) {
    r->setDepthTest(true);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Scene3D);

    float aspect = static_cast<float>(vw_) / static_cast<float>(vh_);
    r->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 500.0f));
    r->setView(Mat4::lookAt(Vec3(0, 20, 40), Vec3(0, 0, 0), Vec3(0, 1, 0)));
    r->setColor(0.6f, 0.7f, 0.8f, 1.0f);
    r->setUseTexture(false);
    r->setLightDir(0.5f, 0.8f, 0.3f);

    // Set model matrix ONCE before the loop
    r->setModel(Mat4::translate(0.0f, 0.0f, 0.0f) * Mat4::scale(0.3f, 0.3f, 0.3f));

    size_t mesh_count = meshes_.size();
    for (int i = 0; i < params_.draws_per_frame; i++) {
        size_t idx = static_cast<size_t>(i) % mesh_count;
        r->drawMesh(meshes_[idx]);
    }
}

void DrawCallRawTest::cleanup(Renderer* r) {
    for (size_t i = 0; i < meshes_.size(); i++) {
        r->destroyMesh(meshes_[i]);
    }
    meshes_.clear();
}

double DrawCallRawTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    return static_cast<double>(params_.draws_per_frame) / (avg_ms / 1000.0) / 1e3; // Kcalls/s
}
