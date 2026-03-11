#include "tests.h"
#include "mesh_gen.h"
#include <cstdio>
#include <cmath>

static const char* SHADER_FMA_VS =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

// Pure FMA shader with data-dependent operands to prevent driver optimization.
// Two interleaved accumulator chains create a dependency that can't be collapsed.
// Each iteration: 2x vec4 mul + 2x vec4 add + 1x vec4 add (cross-feed) = 20 FLOPs
static const char* SHADER_FMA_FS =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform int u_iterations;\n"
    "uniform float u_time;\n"
    "void main() {\n"
    "    vec4 acc0 = vec4(v_uv.x * 0.01 + 0.5, v_uv.y * 0.01 + 0.5, u_time * 0.001 + 0.5, 0.5);\n"
    "    vec4 acc1 = vec4(v_uv.y * 0.01 + 0.3, v_uv.x * 0.01 + 0.7, u_time * 0.002 + 0.4, 0.6);\n"
    "    for (int i = 0; i < u_iterations; i++) {\n"
    "        acc0 = acc0 * acc1 + acc0;\n"
    "        acc1 = acc1 * acc0 + acc1;\n"
    "        acc0 = acc0 * vec4(0.5) + vec4(0.25);\n"
    "    }\n"
    "    gl_FragColor = vec4((acc0.xyz + acc1.xyz) * 0.5, 1.0);\n"
    "}\n";

ShaderFMATest::ShaderFMATest(const ShaderFMAParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH),
      shader_(INVALID_SHADER), u_iterations_loc_(-1), u_time_loc_(-1), time_(0.0f) {}

const char* ShaderFMATest::name() const { return "ShaderFMA"; }
const char* ShaderFMATest::scoreUnit() const { return "GFLOP/s"; }
const char* ShaderFMATest::description() const {
    return "Fullscreen quad with pure FMA shader (acc = acc * a + b).\n"
           "Measures raw ALU throughput without transcendentals.";
}

void ShaderFMATest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    time_ = 0.0f;
    quad_ = r->createMesh(MeshGen::quad());

    shader_ = r->createCustomShader(SHADER_FMA_VS, SHADER_FMA_FS);
    if (shader_ != INVALID_SHADER) {
        u_iterations_loc_ = r->getCustomUniformLoc(shader_, "u_iterations");
        u_time_loc_ = r->getCustomUniformLoc(shader_, "u_time");
    }
}

void ShaderFMATest::render(Renderer* r) {
    r->setDepthTest(false);
    r->setBlending(false);

    if (shader_ != INVALID_SHADER) {
        r->useCustomShader(shader_);
        r->setUniform1i(u_iterations_loc_, params_.iterations);
        r->setUniform1f(u_time_loc_, time_);
    } else {
        r->useShader(Renderer::ShaderType::Color2D);
        r->setColor(1.0f, 0.0f, 0.0f, 1.0f);
    }

    r->drawMesh(quad_);
    time_ += 0.01f;
}

void ShaderFMATest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    quad_ = INVALID_MESH;
    if (shader_ != INVALID_SHADER) {
        r->destroyCustomShader(shader_);
        shader_ = INVALID_SHADER;
    }
}

double ShaderFMATest::computeScore(const std::vector<double>& times, int vw, int vh) {
    if (times.empty()) return 0;
    double total_ms = 0;
    for (size_t i = 0; i < times.size(); i++) total_ms += times[i];
    double avg_ms = total_ms / times.size();
    if (avg_ms <= 0.0) return 0;

    // Per iteration: 3x vec4 mul (12) + 3x vec4 add (12) = 24 FLOPs
    // (acc0*acc1, +acc0, acc1*acc0, +acc1, acc0*0.5, +0.25)
    double flops_per_pixel = 24.0 * params_.iterations;
    double pixels_per_frame = static_cast<double>(vw) * vh;
    double flops_per_frame = flops_per_pixel * pixels_per_frame;
    return flops_per_frame / (avg_ms / 1000.0) / 1e9; // GFLOP/s
}
