#include "engine/state_cache.h"

static constexpr unsigned int INVALID_ID = 0xFFFFFFFF;

void StateCache::reset() {
    depth_test_ = UNKNOWN;
    depth_mask_ = UNKNOWN;
    cull_face_ = UNKNOWN;
    blend_ = UNKNOWN;
    blend_additive_ = UNKNOWN;
    for (int i = 0; i < 4; i++) color_mask_[i] = UNKNOWN;
    current_program_ = INVALID_ID;
    current_fbo_ = INVALID_ID;
    for (int i = 0; i < MAX_TEXTURE_UNITS; i++) bound_textures_[i] = INVALID_ID;
}

bool StateCache::setDepthTest(bool enable) {
    int v = enable ? 1 : 0;
    if (depth_test_ == v) return false;
    depth_test_ = v;
    return true;
}

bool StateCache::setDepthMask(bool write) {
    int v = write ? 1 : 0;
    if (depth_mask_ == v) return false;
    depth_mask_ = v;
    return true;
}

bool StateCache::setCullFace(bool enable) {
    int v = enable ? 1 : 0;
    if (cull_face_ == v) return false;
    cull_face_ = v;
    return true;
}

bool StateCache::setBlending(bool enable) {
    int v = enable ? 1 : 0;
    if (blend_ == v) return false;
    blend_ = v;
    return true;
}

bool StateCache::setBlendingAdditive(bool enable) {
    int v = enable ? 1 : 0;
    if (blend_additive_ == v) return false;
    blend_additive_ = v;
    return true;
}

bool StateCache::setColorMask(bool r, bool g, bool b, bool a) {
    int rv = r ? 1 : 0, gv = g ? 1 : 0, bv = b ? 1 : 0, av = a ? 1 : 0;
    if (color_mask_[0] == rv && color_mask_[1] == gv &&
        color_mask_[2] == bv && color_mask_[3] == av)
        return false;
    color_mask_[0] = rv;
    color_mask_[1] = gv;
    color_mask_[2] = bv;
    color_mask_[3] = av;
    return true;
}

bool StateCache::useProgram(unsigned int program) {
    if (current_program_ == program) return false;
    current_program_ = program;
    return true;
}

bool StateCache::bindFramebuffer(unsigned int fbo) {
    if (current_fbo_ == fbo) return false;
    current_fbo_ = fbo;
    return true;
}

bool StateCache::bindTexture(int unit, unsigned int tex_id) {
    if (unit < 0 || unit >= MAX_TEXTURE_UNITS) return true; // out of range, always bind
    if (bound_textures_[unit] == tex_id) return false;
    bound_textures_[unit] = tex_id;
    return true;
}
