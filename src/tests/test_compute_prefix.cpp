#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "platform/logger.h"

// GLSL 4.30 compute shader: parallel prefix sum (Hillis-Steele scan).
// Measures compute shader throughput with shared memory synchronization.
static const char* PREFIX_SOURCE = R"(
#version 430
layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer DataBuf {
    uint data[];
};

uniform int u_count;

shared uint temp[256];

void main() {
    uint gid = gl_GlobalInvocationID.x;
    uint lid = gl_LocalInvocationID.x;

    // Load
    temp[lid] = (gid < uint(u_count)) ? data[gid] : 0u;
    barrier();

    // Hillis-Steele inclusive scan within workgroup
    for (uint stride = 1u; stride < 256u; stride <<= 1u) {
        uint val = 0u;
        if (lid >= stride) {
            val = temp[lid - stride];
        }
        barrier();
        temp[lid] += val;
        barrier();
    }

    // Store result
    if (gid < uint(u_count)) {
        data[gid] = temp[lid];
    }
}
)";

static const int PREFIX_LOCAL_SIZE = 256;

ComputePrefixTest::ComputePrefixTest(const ComputePrefixParams& params)
    : params_(params), shader_(INVALID_SHADER),
      ssbo_in_(INVALID_BUFFER), ssbo_out_(INVALID_BUFFER), u_count_loc_(-1) {}

const char* ComputePrefixTest::name() const { return "ComputePrefix"; }
const char* ComputePrefixTest::scoreUnit() const { return "GB/s"; }
const char* ComputePrefixTest::description() const {
    return "Parallel prefix sum (scan) (GL 4.3+).\n"
           "Hillis-Steele algorithm with shared memory.\n"
           "Measures compute + shared memory throughput.";
}

void ComputePrefixTest::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(PREFIX_SOURCE);
    if (shader_ == INVALID_SHADER) return;

    u_count_loc_ = r.getCustomUniformLoc(shader_, "u_count");

    int ssbo_size = params_.element_count * 4; // uint = 4 bytes
    ssbo_in_ = comp.createSSBO(ssbo_size);
    // ssbo_out_ unused in this scan variant but kept for API consistency
    (void)ssbo_out_;
    LOG_DBG("Test '%s': setup complete (%d elements, %d workgroups)", name(), params_.element_count, params_.work_groups);
}

void ComputePrefixTest::renderCompute(Renderer& r, ComputeFeatures& comp) {
    if (shader_ == INVALID_SHADER || ssbo_in_ == INVALID_BUFFER) return;

    r.useCustomShader(shader_);
    r.setUniform1i(u_count_loc_, params_.element_count);
    comp.bindSSBO(ssbo_in_, 0);

    int dispatch_groups = (params_.element_count + PREFIX_LOCAL_SIZE - 1) / PREFIX_LOCAL_SIZE;
    comp.dispatchCompute(dispatch_groups, 1, 1);
    comp.computeMemoryBarrier();
}

void ComputePrefixTest::cleanupCompute(Renderer& r, ComputeFeatures& comp) {
    if (ssbo_in_ != INVALID_BUFFER) { comp.destroySSBO(ssbo_in_); ssbo_in_ = INVALID_BUFFER; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
}

double ComputePrefixTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Read + write: 2 * element_count * 4 bytes
    double bytes = 2.0 * params_.element_count * 4.0;
    return bytes / (avg_ms / 1000.0) / 1e9; // GB/s
}
