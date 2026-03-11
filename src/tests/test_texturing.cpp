#include "tests/tests.h"
#include "geometry/mesh_gen.h"

TexturingTest::TexturingTest(const TexturingParams& params)
    : tex_size_(params.tex_size), actual_tex_size_(params.tex_size), layers_(params.layers),
      vw_(0), vh_(0), quad_(INVALID_MESH), texture_(INVALID_TEXTURE) {}

const char* TexturingTest::name() const { return "Texturing"; }
const char* TexturingTest::scoreUnit() const { return "Mtex/s"; }
const char* TexturingTest::description() const {
    return "Renders fullscreen textured quads with large textures.\n"
           "Measures texel fetch throughput.";
}

void TexturingTest::setup(Renderer* r, int vw, int vh) {
    vw_ = vw; vh_ = vh;
    quad_ = r->createMesh(MeshGen::quad());

    const RenderCaps& caps = r->getCaps();
    actual_tex_size_ = clampTexSize(tex_size_, caps.max_texture_size);

    auto pixels = genColorNoise(actual_tex_size_, 42);
    texture_ = r->createTexture(actual_tex_size_, actual_tex_size_, 3, pixels.data());
}

void TexturingTest::render(Renderer* r) {
    r->setDepthTest(false);
    r->setBlending(false);
    r->useShader(Renderer::ShaderType::Textured2D);
    r->bindTexture(texture_);

    for (int i = 0; i < layers_; i++) {
        r->drawMesh(quad_);
    }
}

void TexturingTest::cleanup(Renderer* r) {
    r->destroyMesh(quad_);
    r->destroyTexture(texture_);
    quad_ = INVALID_MESH;
    texture_ = INVALID_TEXTURE;
}

double TexturingTest::computeScore(const std::vector<double>& times, int vw, int vh) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;
    double texels_per_frame = static_cast<double>(vw) * vh * layers_;
    return texels_per_frame / (avg_ms / 1000.0) / 1e6;
}
