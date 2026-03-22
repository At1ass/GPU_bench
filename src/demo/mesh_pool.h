#pragma once
#include "renderer/renderer.h"
#include <vector>

// RAII mesh lifetime manager for scene geometry.
// Owns all MeshHandles; SceneObjects reference them by value (trivially copyable).
// Destructor destroys all meshes.
class MeshPool {
public:
    MeshPool() : renderer_(nullptr) {}

    ~MeshPool() { destroyAll(); }

    MeshPool(const MeshPool&) = delete;
    MeshPool& operator=(const MeshPool&) = delete;
    MeshPool(MeshPool&&) = delete;
    MeshPool& operator=(MeshPool&&) = delete;

    void init(Renderer* r) { renderer_ = r; }

    // Upload mesh data to GPU and return the handle.
    MeshHandle add(const MeshData& data) {
        MeshHandle h = renderer_->createMesh(data);
        handles_.push_back(h);
        return h;
    }

    void destroyAll() {
        if (!renderer_) return;
        for (size_t i = 0; i < handles_.size(); i++) {
            if (handles_[i] != MeshHandle()) {
                renderer_->destroyMesh(handles_[i]);
            }
        }
        handles_.clear();
    }

private:
    Renderer* renderer_;
    std::vector<MeshHandle> handles_;
};
