#pragma once
#include "renderer/renderer.h"
#include "renderer/features.h"

// RAII wrapper for compute SSBOs. Automatically destroys the buffer
// via ComputeFeatures::destroySSBO when it goes out of scope.
class ScopedSSBO {
public:
    ScopedSSBO() : handle_(), renderer_(nullptr) {}
    ~ScopedSSBO() { reset(); }

    ScopedSSBO(const ScopedSSBO&) = delete;
    ScopedSSBO& operator=(const ScopedSSBO&) = delete;

    ScopedSSBO(ScopedSSBO&& o) : handle_(o.handle_), renderer_(o.renderer_) {
        o.handle_ = BufferHandle();
        o.renderer_ = nullptr;
    }

    ScopedSSBO& operator=(ScopedSSBO&& o) {
        if (this != &o) {
            reset();
            handle_ = o.handle_;
            renderer_ = o.renderer_;
            o.handle_ = BufferHandle();
            o.renderer_ = nullptr;
        }
        return *this;
    }

    void assign(Renderer* r, BufferHandle h) {
        reset();
        renderer_ = r;
        handle_ = h;
    }

    void reset() {
        if (handle_ != INVALID_BUFFER && renderer_) {
            ComputeFeatures* cf = renderer_->features<ComputeFeatures>();
            if (cf) cf->destroySSBO(handle_);
        }
        handle_ = BufferHandle();
        renderer_ = nullptr;
    }

    BufferHandle get() const { return handle_; }
    operator BufferHandle() const { return handle_; }
    explicit operator bool() const { return handle_ != INVALID_BUFFER; }

private:
    BufferHandle handle_;
    Renderer* renderer_;
};
