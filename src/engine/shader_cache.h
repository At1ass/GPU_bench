#pragma once
#include "engine/shader_feature_set.h"
#include "engine/shader_feature_define.h"
#include "engine/shader_program.h"
#include "renderer/renderer.h"
#include <string>
#include <unordered_map>
#include <memory>

// Shader permutation cache.
// Compiles shader variants on first request, returns cached program on repeat.
// Generates GLSL preamble from feature flags (#version, #define, compat macros).
//
// Usage:
//   ShaderCache cache;
//   cache.init(renderer);
//   ShaderProgram* prog = cache.get("island", tier_features, tier_features);
//   // prog is owned by cache, valid until destroy()

class ShaderCache {
public:
    ShaderCache();
    ~ShaderCache();

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    ShaderCache(ShaderCache&&) = delete;
    ShaderCache& operator=(ShaderCache&&) = delete;

    void init(Renderer* r, const FeatureDefine* defines = nullptr, int define_count = 0);
    void destroy();

    // Get or compile a vertex+fragment shader variant.
    // base_name: shader name (e.g., "island", "sky") → loads uber/{base_name}.vert/frag
    // Returns nullptr on compilation failure.
    ShaderProgram* get(const char* base_name,
                       ShaderFeatureSet vert_features,
                       ShaderFeatureSet frag_features);

    // Get or compile a GL4 vertex+fragment shader from gl4/ directory.
    // No preamble injection — GL4 shaders contain their own #version 430.
    ShaderProgram* getGL4(const char* base_name);

    // Get or compile a compute shader variant.
    ShaderProgram* getCompute(const char* base_name,
                              ShaderFeatureSet features);

    // Get or compile a tessellation shader (4 stages: vert+tcs+tes+frag).
    ShaderProgram* getTess(const char* base_name,
                           ShaderFeatureSet features);

    // Compile inline vertex+fragment source with uber preamble injection.
    // Same preamble as get() (version, feature defines, compat macros).
    // Source should NOT contain #version — preamble provides it.
    // The key is used for caching; pass a unique name per shader variant.
    ShaderProgram* compileInline(const char* key_name,
                                 const char* vs_source, const char* fs_source,
                                 ShaderFeatureSet features);

    int compiledCount() const { return compiles_; }
    int cacheHitCount() const { return hits_; }

private:
    Renderer* renderer_;

    // Cache key: base name + feature sets for each stage
    struct ProgramKey {
        std::string base_name;
        ShaderFeatureSet vert_features;
        ShaderFeatureSet frag_features;

        bool operator==(const ProgramKey& o) const {
            return base_name == o.base_name
                && vert_features == o.vert_features
                && frag_features == o.frag_features;
        }
    };

    struct ProgramKeyHash {
        size_t operator()(const ProgramKey& k) const {
            size_t h = std::hash<std::string>()(k.base_name);
            h ^= std::hash<uint32_t>()(k.vert_features) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>()(k.frag_features) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<ProgramKey, std::unique_ptr<ShaderProgram>, ProgramKeyHash> cache_;
    const FeatureDefine* defines_;
    int define_count_;
    int compiles_;
    int hits_;

    // Build GLSL preamble: #version + #defines + compat macros + FragColor decl
    std::string buildPreamble(ShaderFeatureSet features, bool is_fragment) const;

    // Load shader file, strip original #version, prepend preamble, process includes
    std::string loadSource(const char* base_name, const char* extension,
                           ShaderFeatureSet features, bool is_fragment) const;

    // Prepend preamble to raw source (strips existing #version if present)
    std::string prepareSource(const char* raw, ShaderFeatureSet features, bool is_fragment) const;

    // GLSL version string from feature flags
    static const char* versionString(ShaderFeatureSet features);
};
