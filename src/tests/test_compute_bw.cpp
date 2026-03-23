#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "platform/logger.h"
#include <cstdio>

// GLSL 4.30 compute shader: memory bandwidth test.
// Reads from one SSBO, minimal ALU, writes to another SSBO.
// Each invocation reads and writes one vec4 (16 bytes).
static const char* COMPUTE_BW_SOURCE = R"(
#version 430
layout(local_size_x = 256) in;

layout(std430, binding = 0) readonly buffer InputBuf {
    vec4 in_data[];
};

layout(std430, binding = 1) writeonly buffer OutputBuf {
    vec4 out_data[];
};

uniform int u_count;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    if (gid < uint(u_count)) {
        vec4 v = in_data[gid];
        out_data[gid] = v + vec4(1.0);
    }
}
)";

static const int BW_LOCAL_SIZE = 256;

ComputeBandwidthTest::ComputeBandwidthTest(const ComputeBandwidthParams& params)
    : params_(params), shader_(INVALID_SHADER), ssbo_in_(INVALID_BUFFER),
      ssbo_out_(INVALID_BUFFER), u_count_loc_(-1) {}

const char* ComputeBandwidthTest::name() const { return "ComputeBW"; }
const char* ComputeBandwidthTest::scoreUnit() const { return "GB/s"; }
const char* ComputeBandwidthTest::description() const {
    return "Compute shader memory bandwidth (GL 4.3+).\n"
           "Reads from one SSBO, writes to another with minimal ALU.\n"
           "Measures global memory bandwidth.";
}

void ComputeBandwidthTest::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(COMPUTE_BW_SOURCE);
    if (shader_ == INVALID_SHADER) return;

    u_count_loc_ = r.getCustomUniformLoc(shader_, "u_count");

    int ssbo_size = params_.buffer_size_mb * 1024 * 1024;
    ssbo_in_ = comp.createSSBO(ssbo_size);
    ssbo_out_ = comp.createSSBO(ssbo_size);
    LOG_DBG("Test '%s': setup complete (%d MB buffer, %d workgroups)", name(), params_.buffer_size_mb, params_.work_groups);
}

void ComputeBandwidthTest::renderCompute(Renderer& r, ComputeFeatures& comp) {
    if (shader_ == INVALID_SHADER || ssbo_in_ == INVALID_BUFFER || ssbo_out_ == INVALID_BUFFER) return;

    r.useCustomShader(shader_);
    int element_count = (params_.buffer_size_mb * 1024 * 1024) / 16; // vec4 = 16 bytes
    r.setUniform1i(u_count_loc_, element_count);
    comp.bindSSBO(ssbo_in_, 0);
    comp.bindSSBO(ssbo_out_, 1);

    // Dispatch enough work groups to cover the entire buffer
    int dispatch_groups = (element_count + BW_LOCAL_SIZE - 1) / BW_LOCAL_SIZE;
    comp.dispatchCompute(dispatch_groups, 1, 1);
    comp.computeMemoryBarrier();
}

void ComputeBandwidthTest::cleanupCompute(Renderer& r, ComputeFeatures& comp) {
    if (ssbo_in_ != INVALID_BUFFER) { comp.destroySSBO(ssbo_in_); ssbo_in_ = INVALID_BUFFER; }
    if (ssbo_out_ != INVALID_BUFFER) { comp.destroySSBO(ssbo_out_); ssbo_out_ = INVALID_BUFFER; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
}

double ComputeBandwidthTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Read + write: 2 * buffer_size
    double bytes = 2.0 * params_.buffer_size_mb * 1024.0 * 1024.0;
    return bytes / (avg_ms / 1000.0) / 1e9; // GB/s
}
