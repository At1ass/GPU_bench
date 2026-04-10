#pragma once

#include "engine/uniform_id.h"
#include "demo/shader_program.h"
#include "geometry/math_types.h"

// Per-shader cached uniform location block.
// Resolves locations lazily on first use, then direct GL calls.
// Zero string operations in hot path.
//
// Usage:
//   UniformBlock ub;
//   ub.init(&my_shader);
//   ub.set(U::Proj, proj_matrix);
//   ub.set(U::Time, 1.5f);

class UniformBlock {
public:
    UniformBlock() : shader_(nullptr) {
        for (int i = 0; i < U::COUNT; i++) locs_[i] = UNRESOLVED;
    }

    // Non-copyable (shader_ is non-owning observer, but shallow copy
    // would share mutable location cache — confusing semantics).
    UniformBlock(const UniformBlock&) = delete;
    UniformBlock& operator=(const UniformBlock&) = delete;
    UniformBlock(UniformBlock&&) = default;
    UniformBlock& operator=(UniformBlock&&) = default;

    void init(ShaderProgram* shader) {
        shader_ = shader;
        for (int i = 0; i < U::COUNT; i++) locs_[i] = UNRESOLVED;
    }

    // Resolve a uniform location (lazy, cached)
    int loc(U::Id id) {
        if (locs_[id] == UNRESOLVED)
            locs_[id] = shader_ ? shader_->loc(uniformName(id)) : -1;
        return locs_[id];
    }

    // Type-safe setters
    void set(U::Id id, float v) {
        int l = loc(id); if (l >= 0 && shader_) shader_->set1f_raw(l, v);
    }
    void set(U::Id id, int v) {
        int l = loc(id); if (l >= 0 && shader_) shader_->set1i_raw(l, v);
    }
    void set(U::Id id, float x, float y) {
        int l = loc(id); if (l >= 0 && shader_) shader_->set2f_raw(l, x, y);
    }
    void set(U::Id id, float x, float y, float z) {
        int l = loc(id); if (l >= 0 && shader_) shader_->set3f_raw(l, x, y, z);
    }
    void set(U::Id id, const Vec3& v) {
        set(id, v.x, v.y, v.z);
    }
    void set(U::Id id, const Mat4& m) {
        int l = loc(id); if (l >= 0 && shader_) shader_->setMat4_raw(l, m);
    }

    // Bind the shader and return this block for chaining
    UniformBlock& use() {
        if (shader_) shader_->use();
        return *this;
    }

    ShaderProgram* shader() const { return shader_; }
    explicit operator bool() const { return shader_ != nullptr && static_cast<bool>(*shader_); }

private:
    static const int UNRESOLVED = -2;
    ShaderProgram* shader_;
    int locs_[U::COUNT];
};
