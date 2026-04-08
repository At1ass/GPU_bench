#pragma once

// Shadow state cache for GL.
// Eliminates redundant glEnable/glDisable/glBind* calls by tracking
// CPU-side shadow of GPU state. Each setter returns true if the value
// actually changed (caller should issue the GL call).
//
// Rules:
//   - CPU shadow is always synchronized with GPU state
//   - Never use glGet* to query state (causes pipeline stall)
//   - reset() must be called at init and after resetState()
//
// Pattern from sokol_gfx (shadow state), Godot (GL state cache).
// Ref: Khronos OpenGL Insights, Chapter 28.

class StateCache {
public:
    StateCache() { reset(); }

    StateCache(const StateCache&) = delete;
    StateCache& operator=(const StateCache&) = delete;

    void reset();

    // Returns true if state changed (GL call needed)
    bool setDepthTest(bool enable);
    bool setDepthMask(bool write);
    bool setCullFace(bool enable);
    bool setBlending(bool enable);
    bool setBlendingAdditive(bool enable);
    bool setColorMask(bool r, bool g, bool b, bool a);
    bool useProgram(unsigned int program);
    bool bindFramebuffer(unsigned int fbo);

    // Texture unit cache: prevents duplicate glActiveTexture + glBindTexture
    bool bindTexture(int unit, unsigned int tex_id);

private:
    static constexpr int UNKNOWN = -1;

    int depth_test_;
    int depth_mask_;
    int cull_face_;
    int blend_;
    int blend_additive_;
    int color_mask_[4];
    unsigned int current_program_;
    unsigned int current_fbo_;

    static constexpr int MAX_TEXTURE_UNITS = 10;
    unsigned int bound_textures_[MAX_TEXTURE_UNITS];
};
