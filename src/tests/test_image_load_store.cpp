#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "platform/logger.h"

// GLSL 4.20 compute shader: image load/store bandwidth test.
// Reads from an image, writes to an SSBO. Compares image vs SSBO access patterns.
static const char* IMAGE_LS_SOURCE = R"(
#version 430
layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) readonly uniform image2D u_input_image;

layout(std430, binding = 1) writeonly buffer OutputBuf {
    vec4 out_data[];
};

uniform int u_image_size;
uniform int u_iterations;

void main() {
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    if (pos.x >= u_image_size || pos.y >= u_image_size) return;

    vec4 acc = vec4(0.0);
    for (int i = 0; i < u_iterations; i++) {
        ivec2 p = ivec2((pos.x + i) % u_image_size, pos.y);
        acc += imageLoad(u_input_image, p);
    }

    uint idx = uint(pos.y) * uint(u_image_size) + uint(pos.x);
    out_data[idx] = acc;
}
)";

static const int IMAGE_LS_LOCAL = 16;

ImageLoadStoreTest::ImageLoadStoreTest(const ImageLoadStoreParams& params)
    : params_(params), shader_(INVALID_SHADER),
      image_tex_(INVALID_TEXTURE), ssbo_(INVALID_BUFFER) {}

const char* ImageLoadStoreTest::name() const { return "ImageLoadStore"; }
const char* ImageLoadStoreTest::scoreUnit() const { return "GB/s"; }
const char* ImageLoadStoreTest::description() const {
    return "Image load/store bandwidth (GL 4.2+).\n"
           "Reads from image2D, writes to SSBO.\n"
           "Measures image unit throughput.";
}

void ImageLoadStoreTest::setupCompute(Renderer& r, ComputeFeatures& comp, int, int) {
    shader_ = comp.createComputeShader(IMAGE_LS_SOURCE);
    if (shader_ == INVALID_SHADER) return;

    int sz = params_.image_size;
    // Create RGBA32F texture for image load
    std::vector<unsigned char> pixels(static_cast<size_t>(sz) * static_cast<size_t>(sz) * 4, 128);
    image_tex_ = r.createTexture(sz, sz, 4, pixels.data());

    // Output SSBO: one vec4 per pixel
    int ssbo_size = sz * sz * 16; // vec4 = 16 bytes
    ssbo_ = comp.createSSBO(ssbo_size);
    LOG_DBG("Test '%s': setup complete (image %dx%d, %d iterations)", name(), sz, sz, params_.iterations);
}

void ImageLoadStoreTest::renderCompute(Renderer& r, ComputeFeatures& comp) {
    if (shader_ == INVALID_SHADER || image_tex_ == INVALID_TEXTURE || ssbo_ == INVALID_BUFFER) return;

    r.useCustomShader(shader_);

    // Bind image texture via GL4Features
    GL4Features* gl4 = r.features<GL4Features>();
    if (gl4) {
        gl4->bindImageTexture(image_tex_, 0, true, false);
    }

    comp.bindSSBO(ssbo_, 1);

    int loc_size = r.getCustomUniformLoc(shader_, "u_image_size");
    int loc_iter = r.getCustomUniformLoc(shader_, "u_iterations");
    r.setUniform1i(loc_size, params_.image_size);
    r.setUniform1i(loc_iter, params_.iterations);

    int groups = (params_.image_size + IMAGE_LS_LOCAL - 1) / IMAGE_LS_LOCAL;
    comp.dispatchCompute(groups, groups, 1);
    comp.computeMemoryBarrier();
}

void ImageLoadStoreTest::cleanupCompute(Renderer& r, ComputeFeatures& comp) {
    if (ssbo_ != INVALID_BUFFER) { comp.destroySSBO(ssbo_); ssbo_ = INVALID_BUFFER; }
    if (image_tex_ != INVALID_TEXTURE) { r.destroyTexture(image_tex_); image_tex_ = INVALID_TEXTURE; }
    if (shader_ != INVALID_SHADER) { r.destroyCustomShader(shader_); shader_ = INVALID_SHADER; }
}

double ImageLoadStoreTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    // Each pixel: iterations image loads (16 bytes each) + 1 SSBO write (16 bytes)
    double pixels = static_cast<double>(params_.image_size) * params_.image_size;
    double bytes = pixels * (params_.iterations * 16.0 + 16.0);
    return bytes / (avg_ms / 1000.0) / 1e9; // GB/s
}
