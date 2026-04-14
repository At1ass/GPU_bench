#include "engine/pass_context.h"

PassContext::PassContext(Renderer* r, MeshHandle fsquad)
    : r_(r), fsquad_(fsquad), gl3_(nullptr), gl4_(nullptr), compute_(nullptr),
      features_cached_(false), current_rt_() {
    stats_.reset();
}

void PassContext::drawFullscreen() {
    cacheFeatures();
    if (gl3_) {
        gl3_->drawFullscreenTriangle();
    } else {
        r_->drawMesh(fsquad_);
    }
}

void PassContext::beginFrame() {
    cacheFeatures();
    stats_.reset();
    r_->resetRendererStats();
}

void PassContext::cacheFeatures() {
    if (features_cached_) return;
    gl3_ = r_->features<GL3Features>();
    gl4_ = r_->features<GL4Features>();
    compute_ = r_->features<ComputeFeatures>();
    features_cached_ = true;
}

// --- Render Target ---

void PassContext::beginRT(RenderTargetHandle rt, int w, int h,
                          const float* clear_color) {
    r_->bindRenderTarget(rt);
    r_->setViewport(0, 0, w, h);
    if (clear_color)
        r_->clear(clear_color[0], clear_color[1],
                  clear_color[2], clear_color[3]);
    current_rt_ = rt;
}

void PassContext::beginScreen(int w, int h, const float* clear_color) {
    r_->bindRenderTarget(RenderTargetHandle());
    r_->setViewport(0, 0, w, h);
    if (clear_color)
        r_->clear(clear_color[0], clear_color[1],
                  clear_color[2], clear_color[3]);
    current_rt_ = RenderTargetHandle();
}

void PassContext::endRT() {
    current_rt_ = RenderTargetHandle();
}

// --- State ---

void PassContext::applyState(const RenderState& state) {
    r_->setDepthTest(state.depth_test);
    r_->setDepthMask(state.depth_write);
    r_->setCullFace(state.cull_face);
    if (state.additive_blend)
        r_->setBlendingAdditive(state.blending);
    else
        r_->setBlending(state.blending);
    r_->setColorMask(state.color_mask_r, state.color_mask_g,
                     state.color_mask_b, state.color_mask_a);
    if (state.polygon_offset) {
        r_->setPolygonOffset(true, state.offset_factor, state.offset_units);
    } else {
        r_->setPolygonOffset(false, 0.0f, 0.0f);
    }
}

void PassContext::setCullFace(bool enable) {
    r_->setCullFace(enable);
}

// --- Textures ---

void PassContext::bindTexture(int slot, TextureHandle tex) {
    r_->bindTextureUnit(slot, tex);
}

void PassContext::bindRTTexture(int slot, RenderTargetHandle rt) {
    r_->bindRenderTargetTexture(rt, slot);
}

// --- Compute ---

void PassContext::bindImage(int unit, TextureHandle tex, bool read, bool write) {
    cacheFeatures();
    if (gl4_) gl4_->bindImageTexture(tex, unit, read, write);
}

void PassContext::dispatch(int gx, int gy, int gz, unsigned int barriers) {
    cacheFeatures();
    if (!compute_) return;
    compute_->dispatchCompute(gx, gy, gz);
    if (barriers == Barrier_None) return;
    if ((barriers & Barrier_Image) && gl4_) {
        gl4_->imageMemoryBarrier();
    }
    if (barriers & (Barrier_Texture | Barrier_SSBO)) {
        compute_->computeMemoryBarrier();
    }
}

void PassContext::bindSSBO(BufferHandle buf, int binding) {
    cacheFeatures();
    if (compute_) compute_->bindSSBO(buf, binding);
}

void PassContext::readSSBO(BufferHandle buf, void* out, int offset, int size) {
    cacheFeatures();
    if (compute_) compute_->readSSBO(buf, out, offset, size);
}

// --- Accessors ---

GL3Features* PassContext::gl3() {
    cacheFeatures();
    return gl3_;
}

GL4Features* PassContext::gl4() {
    cacheFeatures();
    return gl4_;
}

ComputeFeatures* PassContext::compute() {
    cacheFeatures();
    return compute_;
}
