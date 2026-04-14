#pragma once
#include "geometry/mesh.h"
#include "renderer/renderer.h"

// RAII wrapper for GPU resource handles.
// Automatically calls the appropriate destroy method when going out of scope.
//
// Usage:
//   ScopedMesh quad(r, r->createMesh(data));
//   r->drawMesh(quad);  // implicit conversion to MeshHandle
//   // auto-destroyed when quad goes out of scope
//
// Move-only. Use release() to detach ownership.

// Traits: maps handle type to its destroy method.
template<typename H> struct HandleTraits;

template<> struct HandleTraits<MeshHandle> {
    static void destroy(Renderer* r, MeshHandle h) { r->destroyMesh(h); }
};
template<> struct HandleTraits<TextureHandle> {
    static void destroy(Renderer* r, TextureHandle h) { r->destroyTexture(h); }
};
template<> struct HandleTraits<ShaderHandle> {
    static void destroy(Renderer* r, ShaderHandle h) { r->destroyCustomShader(h); }
};
template<> struct HandleTraits<RenderTargetHandle> {
    static void destroy(Renderer* r, RenderTargetHandle h) { r->destroyRenderTarget(h); }
};
template<> struct HandleTraits<BufferHandle> {
    static void destroy(Renderer* r, BufferHandle h) { r->destroyBuffer(h); }
};

template<typename H>
class ScopedHandle {
public:
    ScopedHandle() : handle_(), renderer_(nullptr) {}
    ScopedHandle(Renderer* r, H h) : handle_(h), renderer_(r) {}

    ~ScopedHandle() { reset(); }

    // Move-only
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& o) noexcept
        : handle_(o.handle_), renderer_(o.renderer_) {
        o.handle_ = H();
        o.renderer_ = nullptr;
    }

    ScopedHandle& operator=(ScopedHandle&& o) noexcept {
        if (this != &o) {
            reset();
            handle_ = o.handle_;
            renderer_ = o.renderer_;
            o.handle_ = H();
            o.renderer_ = nullptr;
        }
        return *this;
    }

    // Destroy the held resource (if any) and clear.
    void reset() {
        if (renderer_ && handle_ != H()) {
            HandleTraits<H>::destroy(renderer_, handle_);
        }
        handle_ = H();
        renderer_ = nullptr;
    }

    // Assign a new handle (destroys the previous one if any).
    void assign(Renderer* r, H h) {
        reset();
        renderer_ = r;
        handle_ = h;
    }

    // Release ownership without destroying. Returns the raw handle.
    H release() {
        H h = handle_;
        handle_ = H();
        renderer_ = nullptr;
        return h;
    }

    H get() const { return handle_; }
    operator H() const { return handle_; }
    explicit operator bool() const { return handle_ != H(); }

private:
    H handle_;
    Renderer* renderer_;
};

using ScopedMesh          = ScopedHandle<MeshHandle>;
using ScopedTexture       = ScopedHandle<TextureHandle>;
using ScopedShader        = ScopedHandle<ShaderHandle>;
using ScopedRenderTarget  = ScopedHandle<RenderTargetHandle>;
using ScopedBuffer        = ScopedHandle<BufferHandle>;
