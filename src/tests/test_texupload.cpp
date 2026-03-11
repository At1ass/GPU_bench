#include "tests/tests.h"
#include "geometry/mesh_gen.h"

TexUploadTest::TexUploadTest(const TexUploadParams& params)
    : params_(params), actual_tex_size_(params.tex_size),
      vw_(0), vh_(0), quad_(INVALID_MESH), texture_(INVALID_TEXTURE) {}

const char* TexUploadTest::name() const { return "TexUpload"; }
const char* TexUploadTest::scoreUnit() const { return "MB/s"; }
const char* TexUploadTest::description() const {
    return "Uploads texture data CPU->GPU each frame.\n"
           "Measures texture upload bandwidth via glTexSubImage2D.";
}

void TexUploadTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r->createMesh(MeshGen::quad());

    const RenderCaps& caps = r->getCaps();
    actual_tex_size_ = clampTexSize(params_.tex_size, caps.max_texture_size);

    // Create initial texture
    upload_data_ = genColorNoise(actual_tex_size_, 12345);
    texture_ = r->createTexture(actual_tex_size_, actual_tex_size_, 3, upload_data_.data());
}

void TexUploadTest::render(Renderer* r) {
    // Re-upload texture data N times per frame
    for (int i = 0; i < params_.uploads_per_frame; i++) {
        // Modify a few bytes to prevent driver optimization
        int offset = (i * 1024) % static_cast<int>(upload_data_.size());
        upload_data_[offset] = static_cast<unsigned char>(upload_data_[offset] + 1);
        r->uploadTextureData(texture_, actual_tex_size_, actual_tex_size_, 3, upload_data_.data());
    }

    // Draw a quad to ensure the upload is used
    r->setDepthTest(false);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Textured2D);
    r->bindTexture(texture_);
    r->drawMesh(quad_);
}

void TexUploadTest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    r->destroyTexture(texture_);
    quad_ = INVALID_MESH;
    texture_ = INVALID_TEXTURE;
    upload_data_.clear();
}

double TexUploadTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    // bytes per frame = tex_size^2 * 3 channels * uploads_per_frame
    double bytes_per_frame = static_cast<double>(actual_tex_size_) * actual_tex_size_ * 3.0 * params_.uploads_per_frame;
    return bytes_per_frame / (avg_ms / 1000.0) / (1024.0 * 1024.0); // MB/s
}
