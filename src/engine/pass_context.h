#pragma once
#include "engine/render_state.h"
#include "engine/texture_slots.h"
#include "engine/frame_stats.h"
#include "renderer/renderer.h"
#include "renderer/features.h"

// Barrier flags for compute dispatch (see dispatch()).
enum BarrierFlags : unsigned int {
    Barrier_None    = 0,
    Barrier_Image   = 1 << 0,  // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
    Barrier_Texture = 1 << 1,  // GL_TEXTURE_FETCH_BARRIER_BIT
    Barrier_SSBO    = 1 << 2,  // GL_SHADER_STORAGE_BARRIER_BIT
    Barrier_All     = Barrier_Image | Barrier_Texture | Barrier_SSBO,
};

// Core engine wrapper for pass ↔ renderer interaction.
// Created once per frame in DemoScene::renderFrame(), passed to all passes.
//
// Encapsulates:
//   1. Render target binding + viewport + clear
//   2. Texture binding on fixed slots (TexSlot)
//   3. RenderState application (via Renderer methods)
//   4. Compute dispatch + barriers
//   5. Feature interface caching (gl3/gl4/compute)
//
// Pattern from sokol_gfx: sg_begin_pass() / sg_apply_pipeline() / sg_apply_bindings().

class PassContext {
public:
    explicit PassContext(Renderer* r);

    PassContext(const PassContext&) = delete;
    PassContext& operator=(const PassContext&) = delete;

    // Frame lifecycle — call once at start of frame
    void beginFrame();

    // --- Render Target ---
    void beginRT(RenderTargetHandle rt, int w, int h,
                 const float* clear_color = nullptr);
    void beginScreen(int w, int h, const float* clear_color = nullptr);
    void endRT();

    // --- State ---
    void applyState(const RenderState& state);
    void setCullFace(bool enable);

    // --- Textures ---
    void bindTexture(int slot, TextureHandle tex);
    void bindRTTexture(int slot, RenderTargetHandle rt);

    // --- Compute (GL4+) ---
    void bindImage(int unit, TextureHandle tex, bool read, bool write);
    void dispatch(int groups_x, int groups_y, int groups_z,
                  unsigned int barriers = Barrier_All);
    void bindSSBO(BufferHandle buf, int binding);
    void readSSBO(BufferHandle buf, void* out, int offset, int size);

    // --- Draw ---
    void drawMesh(MeshHandle h) {
        r_->drawMesh(h);
    }

    // --- Accessors ---
    Renderer* renderer() { return r_; }
    GL3Features* gl3();
    GL4Features* gl4();
    ComputeFeatures* compute();
    FrameStats& stats() { return stats_; }
    const FrameStats& stats() const { return stats_; }

private:
    Renderer* r_ = nullptr;           // non-owning: managed by caller
    GL3Features* gl3_ = nullptr;      // non-owning: cached from r_->features<>()
    GL4Features* gl4_ = nullptr;      // non-owning: cached from r_->features<>()
    ComputeFeatures* compute_ = nullptr; // non-owning: cached from r_->features<>()
    bool features_cached_ = false;
    RenderTargetHandle current_rt_;
    FrameStats stats_;

    void cacheFeatures();
};
