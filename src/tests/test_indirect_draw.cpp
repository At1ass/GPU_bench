#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include <cmath>

// Indirect draw command structure (matches GL spec for glMultiDrawElementsIndirect)
struct DrawElementsIndirectCommand {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    unsigned int baseVertex;
    unsigned int baseInstance;
};

// Simple GLSL 430 shader for indirect draw test
static const char* INDIRECT_VS = R"(
#version 430
in vec3 a_pos;
void main() {
    gl_Position = vec4(a_pos * 0.001, 1.0);
}
)";

static const char* INDIRECT_FS = R"(
#version 430
out vec4 frag_color;
void main() {
    frag_color = vec4(0.5, 0.5, 0.5, 1.0);
}
)";

// Multiple passes per frame to ensure stable timing
static const int INDIRECT_PASSES = 20;

IndirectDrawTest::IndirectDrawTest(const IndirectDrawParams& params)
    : params_(params), vw_(0), vh_(0), mesh_(INVALID_MESH),
      shader_(INVALID_SHADER), indirect_buf_(INVALID_BUFFER) {}

const char* IndirectDrawTest::name() const { return "IndirectDraw"; }
const char* IndirectDrawTest::scoreUnit() const { return "calls/s"; }
const char* IndirectDrawTest::description() const {
    return "Multi-draw indirect throughput (GL 4.3+).\n"
           "GPU reads draw parameters from a buffer.\n"
           "Measures indirect draw overhead vs direct.";
}

void IndirectDrawTest::setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) {
    vw_ = vw; vh_ = vh;

    // Create a small mesh
    MeshData md = MeshGen::sphere(4, 3);
    mesh_ = r.createMesh(md);

    shader_ = r.createCustomShader(INDIRECT_VS, INDIRECT_FS);

    // Create indirect buffer with command_count identical commands
    int index_count = static_cast<int>(md.indices.size());
    std::vector<DrawElementsIndirectCommand> commands(static_cast<size_t>(params_.command_count));
    for (int i = 0; i < params_.command_count; i++) {
        commands[i].count = static_cast<unsigned int>(index_count);
        commands[i].instanceCount = 1;
        commands[i].firstIndex = 0;
        commands[i].baseVertex = 0;
        commands[i].baseInstance = 0;
    }

    int buf_size = params_.command_count * static_cast<int>(sizeof(DrawElementsIndirectCommand));
    indirect_buf_ = gl4.createIndirectBuffer(buf_size, commands.data());
}

void IndirectDrawTest::renderGL4(Renderer& r, GL4Features& gl4) {
    if (shader_ == INVALID_SHADER || mesh_ == INVALID_MESH || indirect_buf_ == INVALID_BUFFER) return;

    r.setDepthTest(false);
    r.setBlending(false);
    r.useCustomShader(shader_);
    r.setColorMask(false, false, false, false); // Minimize pixel cost

    // Multiple passes to ensure measurable frame time
    for (int pass = 0; pass < INDIRECT_PASSES; pass++) {
        gl4.multiDrawMeshIndirect(mesh_, indirect_buf_, params_.command_count,
                                  static_cast<int>(sizeof(DrawElementsIndirectCommand)));
    }

    r.setColorMask(true, true, true, true);
    r.setDepthTest(true);
}

void IndirectDrawTest::cleanupGL4(Renderer& r, GL4Features& gl4) {
    if (indirect_buf_ != INVALID_BUFFER) {
        gl4.destroyIndirectBuffer(indirect_buf_);
        indirect_buf_ = INVALID_BUFFER;
    }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
    if (mesh_ != INVALID_MESH) { r.destroyMesh(mesh_); mesh_ = INVALID_MESH; }
}

double IndirectDrawTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double total_commands = static_cast<double>(params_.command_count) * INDIRECT_PASSES;
    return total_commands / (avg_ms / 1000.0); // calls/s
}
