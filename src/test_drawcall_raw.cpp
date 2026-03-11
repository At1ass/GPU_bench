#include "tests.h"
#include "mesh_gen.h"
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
    meshes_.resize(params_.mesh_count);
    for (int i = 0; i < params_.mesh_count; i++) {
        meshes_[i] = r->createMesh(unit);
    }
}

void DrawCallRawTest::render(Renderer* r) {
    r->setDepthTest(true);
    r->setBlending(false);
    r->useShader(Renderer::SHADER_3D);

    float aspect = (float)vw_ / vh_;
    r->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 500.0f));
    r->setView(Mat4::lookAt(Vec3(0, 20, 40), Vec3(0, 0, 0), Vec3(0, 1, 0)));
    r->setColor(0.6f, 0.7f, 0.8f, 1.0f);
    r->setUseTexture(false);
    r->setLightDir(0.5f, 0.8f, 0.3f);

    // Set model matrix ONCE before the loop
    r->setModel(Mat4::translate(0.0f, 0.0f, 0.0f) * Mat4::scale(0.3f, 0.3f, 0.3f));

    int mesh_count = (int)meshes_.size();
    for (int i = 0; i < params_.draws_per_frame; i++) {
        int idx = i % mesh_count;
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
    if (times.empty()) return 0;
    double total_ms = 0;
    for (size_t i = 0; i < times.size(); i++) total_ms += times[i];
    double avg_ms = total_ms / times.size();
    if (avg_ms <= 0.0) return 0;
    return (double)params_.draws_per_frame / (avg_ms / 1000.0) / 1e3; // Kcalls/s
}
