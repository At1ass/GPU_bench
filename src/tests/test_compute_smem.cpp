#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"

// GLSL 4.30 compute shader: shared memory (LDS) bandwidth.
// Each workgroup fills shared memory, does many read-modify-write passes, writes sum to SSBO.
static const char* COMPUTE_SMEM_SOURCE = R"(
#version 430
layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer OutputBuf {
    float data[];
};

uniform int u_iterations;

shared float smem[256];

void main() {
    uint lid = gl_LocalInvocationID.x;
    uint gid = gl_GlobalInvocationID.x;

    smem[lid] = float(gid) * 0.001;
    barrier();

    float acc = 0.0;
    for (int i = 0; i < u_iterations; i++) {
        uint idx = (lid + uint(i)) & 255u;
        acc += smem[idx];
        smem[lid] = acc * 0.5;
        barrier();
    }

    data[gid] = acc;
}
)";

static const int SMEM_LOCAL_SIZE = 256;
static const int SMEM_SIZE_BYTES = 256 * 4; // 256 floats

ComputeSharedMemTest::ComputeSharedMemTest(const ComputeSharedMemParams& params)
    : params_(params), shader_(INVALID_SHADER), ssbo_(INVALID_BUFFER),
      u_iterations_loc_(-1) {}

const char* ComputeSharedMemTest::name() const { return "ComputeSMem"; }
const char* ComputeSharedMemTest::scoreUnit() const { return "GB/s"; }
const char* ComputeSharedMemTest::description() const {
    return "Compute shared memory bandwidth (GL 4.3+).\n"
           "Workgroups read/write shared memory in tight loops.\n"
           "Measures on-chip LDS throughput.";
}

void ComputeSharedMemTest::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(COMPUTE_SMEM_SOURCE);
    if (shader_ == INVALID_SHADER) return;

    u_iterations_loc_ = r.getCustomUniformLoc(shader_, "u_iterations");

    int total_invocations = params_.work_groups * SMEM_LOCAL_SIZE;
    int ssbo_size = total_invocations * 4; // 1 float per invocation
    ssbo_ = comp.createSSBO(ssbo_size);
}

void ComputeSharedMemTest::renderCompute(Renderer& r, ComputeFeatures& comp) {
    if (shader_ == INVALID_SHADER || ssbo_ == INVALID_BUFFER) return;

    r.useCustomShader(shader_);
    r.setUniform1i(u_iterations_loc_, params_.iterations);
    comp.bindSSBO(ssbo_, 0);
    comp.dispatchCompute(params_.work_groups, 1, 1);
    comp.computeMemoryBarrier();
}

void ComputeSharedMemTest::cleanupCompute(Renderer& r, ComputeFeatures& comp) {
    if (ssbo_ != INVALID_BUFFER) { comp.destroySSBO(ssbo_); ssbo_ = INVALID_BUFFER; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
}

double ComputeSharedMemTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Each iteration: read 1 float + write 1 float = 8 bytes per invocation
    double bytes_per_dispatch = 8.0 * params_.iterations * params_.work_groups * SMEM_LOCAL_SIZE;
    return bytes_per_dispatch / (avg_ms / 1000.0) / 1e9; // GB/s
}
