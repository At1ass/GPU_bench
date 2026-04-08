#pragma once
#include "renderer/renderer.h"
#include "renderer/scoped_handle.h"
#include <string>
#include <unordered_map>

// RAII shader wrapper with lazy uniform location caching.
// Eliminates manual int u_* fields and repeated getCustomUniformLoc calls.
class ShaderProgram {
public:
    ShaderProgram() : renderer_(nullptr) {}
    ~ShaderProgram() = default;

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) = default;
    ShaderProgram& operator=(ShaderProgram&&) = default;

    // Create from vertex + fragment source. Returns false on failure.
    bool create(Renderer* r, const char* vs, const char* fs) {
        ShaderHandle h = r->createCustomShader(vs, fs);
        if (!h) return false;
        shader_.assign(r, h);
        renderer_ = r;
        cache_.clear();
        return true;
    }

    // Adopt an externally-created shader (compute, tessellation, geometry).
    void adopt(Renderer* r, ShaderHandle h) {
        shader_.assign(r, h);
        renderer_ = r;
        cache_.clear();
    }

    void reset() {
        shader_.reset();
        renderer_ = nullptr;
        cache_.clear();
    }

    // Bind this shader for rendering.
    void use() {
        renderer_->useCustomShader(shader_);
    }

    // Get cached uniform location.
    int loc(const char* name) {
        std::unordered_map<std::string, int>::iterator it = cache_.find(name);
        if (it != cache_.end()) return it->second;
        int l = renderer_->getCustomUniformLoc(shader_, name);
        cache_[name] = l;
        return l;
    }

    // Uniform setters (use cached locations).
    void set1i(const char* name, int v) {
        renderer_->setUniform1i(loc(name), v);
    }
    void set1f(const char* name, float v) {
        renderer_->setUniform1f(loc(name), v);
    }
    void set2f(const char* name, float x, float y) {
        renderer_->setUniform2f(loc(name), x, y);
    }
    void set3f(const char* name, float x, float y, float z) {
        renderer_->setUniform3f(loc(name), x, y, z);
    }
    void set4f(const char* name, float r, float g, float b, float a) {
        renderer_->setUniform4f(loc(name), r, g, b, a);
    }
    void setMat4(const char* name, const Mat4& m) {
        renderer_->setUniformMat4(loc(name), m);
    }

    // Direct setters by location (used by UniformBlock, no string lookup)
    void set1i_raw(int l, int v) { renderer_->setUniform1i(l, v); }
    void set1f_raw(int l, float v) { renderer_->setUniform1f(l, v); }
    void set2f_raw(int l, float x, float y) { renderer_->setUniform2f(l, x, y); }
    void set3f_raw(int l, float x, float y, float z) { renderer_->setUniform3f(l, x, y, z); }
    void set4f_raw(int l, float r, float g, float b, float a) { renderer_->setUniform4f(l, r, g, b, a); }
    void setMat4_raw(int l, const Mat4& m) { renderer_->setUniformMat4(l, m); }

    // Access underlying handle.
    ShaderHandle handle() const { return shader_.get(); }
    explicit operator bool() const { return static_cast<bool>(shader_); }
    explicit operator ShaderHandle() const { return shader_.get(); }

private:
    ScopedShader shader_;
    Renderer* renderer_ = nullptr;  // non-owning: valid after create()/adopt()
    std::unordered_map<std::string, int> cache_;
};
