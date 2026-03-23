#include "tests/tests.h"
#include "renderer/features.h"
#include "geometry/mesh_gen.h"
#include "platform/logger.h"
#include <cmath>

// GLSL 1.40 vertex shader using gl_InstanceID for per-instance positioning.
// Instances are spread in a grid pattern with small scale.
static const char* INSTANCED_VS_140 = R"(
#version 140
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform int u_instance_count;
out vec3 v_normal;
void main() {
    int cols = int(sqrt(float(u_instance_count))) + 1;
    float x = float(gl_InstanceID % cols) * 2.5 - float(cols) * 1.25;
    float z = float(gl_InstanceID / cols) * 2.5 - float(cols) * 1.25;
    vec3 world_pos = a_pos * 0.5 + vec3(x, 0.0, z);
    v_normal = a_normal;
    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}
)";

static const char* INSTANCED_FS_140 = R"(
#version 140
in vec3 v_normal;
void main() {
    vec3 light = normalize(vec3(0.5, 0.8, 0.3));
    float d = max(dot(normalize(v_normal), light), 0.2);
    gl_FragColor = vec4(vec3(0.6, 0.7, 0.8) * d, 1.0);
}
)";

// GLSL 1.50 core profile variants
static const char* INSTANCED_VS_150 = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform int u_instance_count;
out vec3 v_normal;
void main() {
    int cols = int(sqrt(float(u_instance_count))) + 1;
    float x = float(gl_InstanceID % cols) * 2.5 - float(cols) * 1.25;
    float z = float(gl_InstanceID / cols) * 2.5 - float(cols) * 1.25;
    vec3 world_pos = a_pos * 0.5 + vec3(x, 0.0, z);
    v_normal = a_normal;
    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}
)";

static const char* INSTANCED_FS_150 = R"(
#version 150
in vec3 v_normal;
out vec4 fragColor;
void main() {
    vec3 light = normalize(vec3(0.5, 0.8, 0.3));
    float d = max(dot(normalize(v_normal), light), 0.2);
    fragColor = vec4(vec3(0.6, 0.7, 0.8) * d, 1.0);
}
)";

InstancedDrawTest::InstancedDrawTest(const InstancedDrawParams& params)
    : params_(params), tri_count_(0), vw_(0), vh_(0),
      mesh_(INVALID_MESH), shader_(INVALID_SHADER),
      u_proj_loc_(-1), u_view_loc_(-1), u_instance_count_loc_(-1) {}

const char* InstancedDrawTest::name() const { return "InstancedDraw"; }
const char* InstancedDrawTest::scoreUnit() const { return "Mtri/s"; }
const char* InstancedDrawTest::description() const {
    return "Draws many instances of a small mesh using GL3+ instancing.\n"
           "Uses gl_InstanceID for per-instance transforms.\n"
           "Measures instanced rendering throughput.";
}

void InstancedDrawTest::setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) {
    (void)gl3;
    vw_ = vw; vh_ = vh;

    // Use a sphere as the instanced mesh (moderate tri count per instance)
    MeshData mesh = MeshGen::sphere(8, 6);
    tri_count_ = static_cast<int>(mesh.indices.size()) / 3;
    mesh_ = r.createMesh(mesh);

    const char* vs = r.isCoreProfile() ? INSTANCED_VS_150 : INSTANCED_VS_140;
    const char* fs = r.isCoreProfile() ? INSTANCED_FS_150 : INSTANCED_FS_140;
    shader_ = r.createCustomShader(vs, fs);
    if (shader_ != INVALID_SHADER) {
        u_proj_loc_ = r.getCustomUniformLoc(shader_, "u_proj");
        u_view_loc_ = r.getCustomUniformLoc(shader_, "u_view");
        u_instance_count_loc_ = r.getCustomUniformLoc(shader_, "u_instance_count");
    }
    Log::dbg("Test '%s': setup complete (%d instances, %d tris/instance)", name(), params_.instance_count, tri_count_);
}

void InstancedDrawTest::renderGL3(Renderer& r, GL3Features& gl3) {
    r.setDepthTest(true);
    r.setBlending(false);

    if (shader_ != INVALID_SHADER) {
        r.useCustomShader(shader_);

        float aspect = static_cast<float>(vw_) / static_cast<float>(vh_);
        Mat4 proj = Mat4::perspective(60.0f, aspect, 0.1f, 5000.0f);
        Mat4 view = Mat4::lookAt(Vec3(0, 80, 120), Vec3(0, 0, 0), Vec3(0, 1, 0));

        r.setUniformMat4(u_proj_loc_, proj);
        r.setUniformMat4(u_view_loc_, view);
        r.setUniform1i(u_instance_count_loc_, params_.instance_count);
    } else {
        // Fallback: use built-in shader
        r.useShader(Renderer::ShaderType::Scene3D);
        float aspect = static_cast<float>(vw_) / static_cast<float>(vh_);
        r.setProjection(Mat4::perspective(60.0f, aspect, 0.1f, 5000.0f));
        r.setView(Mat4::lookAt(Vec3(0, 80, 120), Vec3(0, 0, 0), Vec3(0, 1, 0)));
        r.setColor(0.6f, 0.7f, 0.8f, 1.0f);
        r.setUseTexture(false);
        r.setLightDir(0.5f, 0.8f, 0.3f);
    }

    gl3.drawMeshInstanced(mesh_, params_.instance_count);
}

void InstancedDrawTest::cleanupGL3(Renderer& r, GL3Features&) {
    r.destroyMesh(mesh_);
    mesh_ = INVALID_MESH;
    if (shader_ != INVALID_SHADER) {
        r.destroyCustomShader(shader_);
        shader_ = INVALID_SHADER;
    }
}

double InstancedDrawTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    double total_tris = static_cast<double>(tri_count_) * params_.instance_count;
    return total_tris / (avg_ms / 1000.0) / 1e6; // Mtri/s
}
