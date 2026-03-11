#include "tests.h"
#include "mesh_gen.h"
#include <cstdio>

GeometryTest::GeometryTest(const GeometryParams& params)
    : grid_size_(params.grid_size), tri_count_(0), vw_(0), vh_(0), mesh_(INVALID_MESH) {}

const char* GeometryTest::name() const { return "Geometry"; }
const char* GeometryTest::scoreUnit() const { return "Mtris/s"; }
const char* GeometryTest::description() const {
    return "Renders a dense grid of cubes with basic lighting.\n"
           "Measures triangle throughput.";
}

void GeometryTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    MeshData data = MeshGen::cubeGrid(grid_size_);
    tri_count_ = static_cast<int>(data.indices.size()) / 3;
    mesh_ = r->createMesh(data);
    if (mesh_ == INVALID_MESH) {
        fprintf(stderr, "GeometryTest: failed to create mesh (grid=%d)\n", grid_size_);
        tri_count_ = 0;
    }
}

void GeometryTest::render(Renderer* r) {
    r->setDepthTest(true);
    r->setBlending(false);
    r->useShader(Renderer::SHADER_3D);

    float aspect = static_cast<float>(vw_) / vh_;
    r->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 500.0f));

    float dist = grid_size_ * 2.0f;
    r->setView(Mat4::lookAt(
        Vec3(dist * 0.7f, dist * 0.5f, dist * 0.7f),
        Vec3(0, 0, 0),
        Vec3(0, 1, 0)
    ));
    r->setModel(Mat4());
    r->setColor(0.7f, 0.75f, 0.8f, 1.0f);
    r->setUseTexture(false);
    r->setLightDir(0.5f, 0.8f, 0.3f);

    r->drawMesh(mesh_);
}

void GeometryTest::cleanup(Renderer* r) {
    r->destroyMesh(mesh_);
    mesh_ = INVALID_MESH;
}

double GeometryTest::computeScore(const std::vector<double>& times, int, int) {
    if (times.empty() || tri_count_ == 0) return 0;
    double total_ms = 0;
    for (size_t i = 0; i < times.size(); i++) total_ms += times[i];
    double avg_ms = total_ms / times.size();
    if (avg_ms <= 0.0) return 0;
    return static_cast<double>(tri_count_) / (avg_ms / 1000.0) / 1e6;
}
