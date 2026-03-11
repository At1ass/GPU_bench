#include "tests.h"
#include "mesh_gen.h"
#include <cmath>
#include <algorithm>

SceneTest::SceneTest(const SceneParams& params)
    : params_(params), vw_(0), vh_(0),
      terrain_(INVALID_MESH), sphere_(INVALID_MESH), cube_(INVALID_MESH),
      terrain_tex_(INVALID_TEXTURE), obj_tex_(INVALID_TEXTURE), angle_(0.0f) {}

const char* SceneTest::name() const { return "Scene"; }
const char* SceneTest::scoreUnit() const { return "FPS"; }
const char* SceneTest::description() const {
    return "Combined test: terrain, spheres, cubes, lighting.\n"
           "Measures overall rendering performance.";
}

void SceneTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    angle_ = 0.0f;

    const RenderCaps& caps = r->getCaps();

    int terrain_tex_size = clampTexSize(params_.terrain_tex, caps.max_texture_size);
    int obj_tex_size = clampTexSize(params_.obj_tex, caps.max_texture_size);

    terrain_ = r->createMesh(MeshGen::terrain(100.0f, params_.terrain_res));
    sphere_  = r->createMesh(MeshGen::sphere(params_.sphere_segs, params_.sphere_rings));
    cube_    = r->createMesh(MeshGen::cubeGrid(params_.cube_grid));

    auto tpix = genCheckerboard(terrain_tex_size, std::max(1, terrain_tex_size / 64));
    terrain_tex_ = r->createTexture(terrain_tex_size, terrain_tex_size, 3, tpix.data());

    auto opix = genColorNoise(obj_tex_size, 1337);
    obj_tex_ = r->createTexture(obj_tex_size, obj_tex_size, 3, opix.data());
}

void SceneTest::render(Renderer* r) {
    r->setDepthTest(true);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Scene3D);

    float aspect = static_cast<float>(vw_) / vh_;
    r->setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 800.0f));
    r->setView(Mat4::lookAt(
        Vec3(50.0f, 30.0f, 50.0f),
        Vec3(0, 0, 0),
        Vec3(0, 1, 0)
    ));
    r->setLightDir(0.4f, 0.7f, 0.5f);

    // Terrain
    r->setModel(Mat4());
    r->setUseTexture(true);
    r->setColor(1.0f, 1.0f, 1.0f, 1.0f);
    r->bindTexture(terrain_tex_);
    r->drawMesh(terrain_);

    // 80+ spheres in 5 rings
    r->bindTexture(obj_tex_);
    for (int ring = 0; ring < 5; ring++) {
        float radius = 10.0f + ring * 7.0f;
        int count = 14 + ring * 4;
        for (int i = 0; i < count; i++) {
            float a = static_cast<float>(i) / count * 6.28318f + angle_ * (1.0f + ring * 0.2f);
            float x = cosf(a) * radius;
            float z = sinf(a) * radius;
            float y = 4.0f + sinf(a * 3.0f + angle_ * 2.0f) * 2.0f;
            r->setModel(Mat4::translate(x, y, z) * Mat4::scale(2.0f, 2.0f, 2.0f));
            r->drawMesh(sphere_);
        }
    }

    // Cube grids
    r->setColor(0.8f, 0.5f, 0.3f, 1.0f);
    r->setUseTexture(false);
    for (int i = 0; i < 6; i++) {
        float a = static_cast<float>(i) / 6.0f * 6.28318f - angle_ * 0.5f;
        float x = cosf(a) * 20.0f;
        float z = sinf(a) * 20.0f;
        r->setModel(Mat4::translate(x, 8.0f, z)
                     * Mat4::rotateY(angle_ * 30.0f + i * 60.0f)
                     * Mat4::scale(0.4f, 0.4f, 0.4f));
        r->drawMesh(cube_);
    }

    // Alpha-blended overlay spheres
    r->setBlending(true);
    r->setDepthTest(false);
    r->setUseTexture(true);
    r->bindTexture(obj_tex_);
    r->setColor(1.0f, 1.0f, 1.0f, 0.1f);
    for (int i = 0; i < 10; i++) {
        float a = angle_ * 0.3f + static_cast<float>(i) * 0.628f;
        r->setModel(Mat4::translate(cosf(a) * 18.0f, 10.0f, sinf(a) * 18.0f)
                     * Mat4::scale(10.0f, 10.0f, 10.0f));
        r->drawMesh(sphere_);
    }
    r->setBlending(false);
    r->setDepthTest(true);

    angle_ += 0.002f;
}

void SceneTest::cleanup(Renderer* r) {
    r->destroyMesh(terrain_);
    r->destroyMesh(sphere_);
    r->destroyMesh(cube_);
    r->destroyTexture(terrain_tex_);
    r->destroyTexture(obj_tex_);
    terrain_ = INVALID_MESH;
    sphere_ = INVALID_MESH;
    cube_ = INVALID_MESH;
    terrain_tex_ = INVALID_TEXTURE;
    obj_tex_ = INVALID_TEXTURE;
}

double SceneTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    return 1000.0 / avg_ms;
}
