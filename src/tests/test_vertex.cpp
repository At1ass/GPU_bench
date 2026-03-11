#include "tests/tests.h"
#include "geometry/mesh_gen.h"
#include <cstdio>
#include <cmath>

VertexTest::VertexTest(const VertexParams& params)
    : params_(params), actual_vertex_count_(0), vw_(0), vh_(0), mesh_(INVALID_MESH) {}

const char* VertexTest::name() const { return "Vertex"; }
const char* VertexTest::scoreUnit() const { return "Mverts/s"; }
const char* VertexTest::description() const {
    return "Large mesh with color mask disabled.\n"
           "Measures pure vertex processing throughput.";
}

void VertexTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    // Build a large sphere/terrain mesh to reach target vertex count
    // Sphere: (segs+1)*(rings+1) vertices
    // Aim for target vertex_count: solve segs*rings ~ vertex_count
    // Use segs = rings * 2 ratio
    int target = params_.vertex_count;
    int rings = static_cast<int>(sqrtf(static_cast<float>(target) / 2.0f));
    if (rings < 4) rings = 4;
    int segs = rings * 2;

    MeshData data = MeshGen::sphere(segs, rings);
    actual_vertex_count_ = static_cast<int>(data.vertices.size());
    mesh_ = r->createMesh(data);
    if (mesh_ == INVALID_MESH) {
        fprintf(stderr, "VertexTest: failed to create mesh (%d verts)\n", actual_vertex_count_);
        actual_vertex_count_ = 0;
    }
}

void VertexTest::render(Renderer* r) {
    r->setDepthTest(true);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Scene3D);

    float aspect = static_cast<float>(vw_) / vh_;
    r->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 100.0f));
    r->setView(Mat4::lookAt(Vec3(0, 0, 3), Vec3(0, 0, 0), Vec3(0, 1, 0)));
    r->setModel(Mat4());
    r->setColor(0.5f, 0.5f, 0.5f, 1.0f);
    r->setUseTexture(false);
    r->setLightDir(0.5f, 0.8f, 0.3f);

    // Disable color writes: pure vertex processing, no fragment work
    r->setColorMask(false, false, false, false);

    r->drawMesh(mesh_);

    // Restore color writes
    r->setColorMask(true, true, true, true);
}

void VertexTest::cleanup(Renderer* r) {
    r->destroyMesh(mesh_);
    mesh_ = INVALID_MESH;
}

double VertexTest::computeScore(const std::vector<double>& times, int, int) {
    if (times.empty() || actual_vertex_count_ == 0) return 0;
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    return static_cast<double>(actual_vertex_count_) / (avg_ms / 1000.0) / 1e6; // Mverts/s
}
