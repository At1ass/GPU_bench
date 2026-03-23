#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "platform/logger.h"
#include <cstdio>

// GLSL 4.30 compute shader: pure FMA throughput on SSBO data.
// Two interleaved vec4 accumulator chains with data dependency.
// Each iteration: 3x vec4 mul + 3x vec4 add = 24 FLOPs.
// Local size 64 is a good default for most GPUs.
static const char* COMPUTE_FMA_SOURCE = R"(
#version 430
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer OutputBuf {
    vec4 data[];
};

uniform int u_iterations;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    float seed = float(gid) * 0.00001;
    vec4 acc0 = vec4(seed + 0.5, seed + 0.3, seed + 0.7, 0.5);
    vec4 acc1 = vec4(seed + 0.3, seed + 0.5, seed + 0.4, 0.6);

    for (int i = 0; i < u_iterations; i++) {
        acc0 = acc0 * acc1 + acc0;
        acc1 = acc1 * acc0 + acc1;
        acc0 = acc0 * vec4(0.5) + vec4(0.25);
    }

    data[gid] = acc0 + acc1;
}
)";

static const int LOCAL_SIZE_X = 64;

ComputeFMATest::ComputeFMATest(const ComputeFMAParams& params)
    : params_(params), shader_(INVALID_SHADER), ssbo_(INVALID_BUFFER),
      u_iterations_loc_(-1) {}

const char* ComputeFMATest::name() const { return "ComputeFMA"; }
const char* ComputeFMATest::scoreUnit() const { return "GFLOP/s"; }
const char* ComputeFMATest::description() const {
    return "Compute shader FMA throughput (GL 4.3+).\n"
           "Dispatches compute work groups doing pure multiply-add.\n"
           "Measures raw GPU compute performance via SSBO.";
}

void ComputeFMATest::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(COMPUTE_FMA_SOURCE);
    if (shader_ == INVALID_SHADER) return;

    u_iterations_loc_ = r.getCustomUniformLoc(shader_, "u_iterations");

    // Create SSBO: work_groups * LOCAL_SIZE_X invocations, each writes vec4 (16 bytes)
    int total_invocations = params_.work_groups * LOCAL_SIZE_X;
    int ssbo_size = total_invocations * 16; // sizeof(vec4)
    ssbo_ = comp.createSSBO(ssbo_size);
    Log::dbg("Test '%s': setup complete (%d workgroups, %d iterations)", name(), params_.work_groups, params_.iterations);
}

void ComputeFMATest::renderCompute(Renderer& r, ComputeFeatures& comp) {
    if (shader_ == INVALID_SHADER || ssbo_ == INVALID_BUFFER) return;

    r.useCustomShader(shader_);
    r.setUniform1i(u_iterations_loc_, params_.iterations);
    comp.bindSSBO(ssbo_, 0);
    comp.dispatchCompute(params_.work_groups, 1, 1);
    comp.computeMemoryBarrier();
}

void ComputeFMATest::cleanupCompute(Renderer& r, ComputeFeatures& comp) {
    if (ssbo_ != INVALID_BUFFER) {
        comp.destroySSBO(ssbo_);
        ssbo_ = INVALID_BUFFER;
    }
    if (shader_ != INVALID_SHADER) {
        r.destroyCustomShader(shader_);
        shader_ = INVALID_SHADER;
    }
}

double ComputeFMATest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Per iteration: 3x vec4 mul (12) + 3x vec4 add (12) = 24 FLOPs
    double total_invocations = static_cast<double>(params_.work_groups) * LOCAL_SIZE_X;
    double flops_per_invocation = 24.0 * params_.iterations;
    double total_flops = total_invocations * flops_per_invocation;

    return total_flops / (avg_ms / 1000.0) / 1e9; // GFLOP/s
}
