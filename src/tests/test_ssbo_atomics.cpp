#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "platform/logger.h"

// GLSL 4.30 compute shader: atomic operation throughput.
// Each invocation does N atomic ops (add, min, max) on SSBO.
static const char* SSBO_ATOMICS_SOURCE = R"(
#version 430
layout(local_size_x = 64) in;

layout(std430, binding = 0) buffer AtomicBuf {
    uint counters[];
};

uniform int u_iterations;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    uint bucket = gid & 255u;

    for (int i = 0; i < u_iterations; i++) {
        atomicAdd(counters[bucket], 1u);
        atomicMin(counters[bucket + 256u], gid);
        atomicMax(counters[bucket + 512u], gid + uint(i));
    }
}
)";

static const int ATOMICS_LOCAL_SIZE = 64;

SSBOAtomicsTest::SSBOAtomicsTest(const SSBOAtomicsParams& params)
    : params_(params), shader_(INVALID_SHADER), ssbo_(INVALID_BUFFER),
      u_iterations_loc_(-1) {}

const char* SSBOAtomicsTest::name() const { return "SSBOAtomics"; }
const char* SSBOAtomicsTest::scoreUnit() const { return "GOps/s"; }
const char* SSBOAtomicsTest::description() const {
    return "SSBO atomic operation throughput (GL 4.3+).\n"
           "Tests atomicAdd, atomicMin, atomicMax on contended counters.\n"
           "Critical for GPU-driven rendering workloads.";
}

void SSBOAtomicsTest::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(SSBO_ATOMICS_SOURCE);
    if (shader_ == INVALID_SHADER) return;

    u_iterations_loc_ = r.getCustomUniformLoc(shader_, "u_iterations");

    // 768 uint counters (3 banks of 256)
    int ssbo_size = 768 * 4;
    ssbo_ = comp.createSSBO(ssbo_size);
    LOG_DBG("Test '%s': setup complete (%d workgroups, %d iterations)", name(), params_.work_groups, params_.iterations);
}

void SSBOAtomicsTest::renderCompute(Renderer& r, ComputeFeatures& comp) {
    if (shader_ == INVALID_SHADER || ssbo_ == INVALID_BUFFER) return;

    r.useCustomShader(shader_);
    r.setUniform1i(u_iterations_loc_, params_.iterations);
    comp.bindSSBO(ssbo_, 0);
    comp.dispatchCompute(params_.work_groups, 1, 1);
    comp.computeMemoryBarrier();
}

void SSBOAtomicsTest::cleanupCompute(Renderer& r, ComputeFeatures& comp) {
    if (ssbo_ != INVALID_BUFFER) { comp.destroySSBO(ssbo_); ssbo_ = INVALID_BUFFER; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
}

double SSBOAtomicsTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // 3 atomic ops per iteration per invocation
    double total_invocations = static_cast<double>(params_.work_groups) * ATOMICS_LOCAL_SIZE;
    double total_ops = total_invocations * params_.iterations * 3.0;
    return total_ops / (avg_ms / 1000.0) / 1e9; // GOps/s
}
