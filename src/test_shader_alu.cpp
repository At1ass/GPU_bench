#include "tests.h"
#include "mesh_gen.h"
#include <cstdio>
#include <cmath>

static const char* SHADER_ALU_VS =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

// Heavy fragment shader with sin/cos/pow/sqrt loop
// The loop count is controlled by a uniform u_iterations
// We unroll in groups of 4 operations per iteration to give the compiler less room to optimize away
static const char* SHADER_ALU_FS =
    "#version 120\n"
    "varying vec2 v_uv;\n"
    "uniform int u_iterations;\n"
    "uniform float u_time;\n"
    "void main() {\n"
    "    vec4 acc = vec4(v_uv.x, v_uv.y, u_time * 0.01, 1.0);\n"
    "    for (int i = 0; i < u_iterations; i++) {\n"
    "        acc.x = sin(acc.x * 1.1 + acc.y);\n"
    "        acc.y = cos(acc.y * 1.2 + acc.z);\n"
    "        acc.z = sqrt(abs(acc.z * acc.x + 0.5));\n"
    "        acc.w = pow(abs(acc.w), 0.99) + acc.x * 0.01;\n"
    "    }\n"
    "    gl_FragColor = vec4(acc.xyz * 0.5 + 0.5, 1.0);\n"
    "}\n";

ShaderALUTest::ShaderALUTest(const ShaderALUParams& params)
    : params_(params), vw_(0), vh_(0), quad_(INVALID_MESH),
      shader_(INVALID_SHADER), u_iterations_loc_(-1), u_time_loc_(-1), time_(0.0f) {}

const char* ShaderALUTest::name() const { return "ShaderALU"; }
const char* ShaderALUTest::scoreUnit() const { return "Gops"; }
const char* ShaderALUTest::description() const {
    return "Fullscreen quad with heavy fragment shader (sin/cos/pow/sqrt).\n"
           "Measures fragment shader ALU throughput.";
}

void ShaderALUTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    time_ = 0.0f;
    quad_ = r->createMesh(MeshGen::quad());

    shader_ = r->createCustomShader(SHADER_ALU_VS, SHADER_ALU_FS);
    if (shader_ != INVALID_SHADER) {
        u_iterations_loc_ = r->getCustomUniformLoc(shader_, "u_iterations");
        u_time_loc_ = r->getCustomUniformLoc(shader_, "u_time");
    }
}

void ShaderALUTest::render(Renderer* r) {
    r->setDepthTest(false);
    r->setBlending(false);

    if (shader_ != INVALID_SHADER) {
        r->useCustomShader(shader_);
        r->setUniform1i(u_iterations_loc_, params_.iterations);
        r->setUniform1f(u_time_loc_, time_);
    } else {
        // Fallback: use built-in shader
        r->useShader(Renderer::ShaderType::Color2D);
        r->setColor(1.0f, 0.0f, 0.0f, 1.0f);
    }

    r->drawMesh(quad_);
    time_ += 0.01f;
}

void ShaderALUTest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    quad_ = INVALID_MESH;
    if (shader_ != INVALID_SHADER) {
        r->destroyCustomShader(shader_);
        shader_ = INVALID_SHADER;
    }
}

double ShaderALUTest::computeScore(const std::vector<double>& times, int vw, int vh) {
    if (times.empty()) return 0;
    double total_ms = 0;
    for (size_t i = 0; i < times.size(); i++) total_ms += times[i];
    double avg_ms = total_ms / times.size();
    if (avg_ms <= 0.0) return 0;

    // 4 transcendental ops + 4 add/mul ops per iteration per pixel.
    // Note: transcendentals (sin/cos/sqrt/pow) are NOT single FLOPs —
    // actual cost varies by GPU architecture. Score is for relative comparison only.
    double flops_per_pixel = 8.0 * params_.iterations;
    double pixels_per_frame = static_cast<double>(vw) * vh;
    double flops_per_frame = flops_per_pixel * pixels_per_frame;
    return flops_per_frame / (avg_ms / 1000.0) / 1e9; // Giga-ops/s
}
