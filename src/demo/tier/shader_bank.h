#pragma once
#include "engine/shader_cache.h"
#include "demo/tier/shader_feature.h"
#include "engine/shader_program.h"

class Renderer;

// Centralized shader compilation and lookup.
// Single source of truth: shader_registry.def declares all shaders.
// ShaderBank compiles them via ShaderCache and provides O(1) enum-indexed access.
//
// Usage:
//   ShaderBank bank;
//   bank.compile(renderer, max_tier);
//   ShaderProgram* sky = bank.get(ShaderBank::Sky, DemoTier::Enhanced);

class ShaderBank {
public:
    // Shader IDs — generated from shader_registry.def
    enum Id {
        #define SHADER(id, ...) id,
        #include "demo/tier/shader_registry.def"
        #undef SHADER
        COUNT
    };

    // Shader compilation type
    enum Type { Uber, GL4, Compute, Tess };

    ShaderBank();
    ~ShaderBank();

    ShaderBank(const ShaderBank&) = delete;
    ShaderBank& operator=(const ShaderBank&) = delete;

    // Compile all shaders for tiers up to max_tier.
    // Returns false if critical T1 shaders fail.
    bool compile(Renderer* r, int max_tier);

    // Get compiled shader for a specific tier.
    // Uber: returns tier-specific variant with automatic fallback to lower tier.
    // GL4/Compute/Tess: tier is ignored, returns the single compiled program.
    // Returns nullptr if shader requires higher tier or compilation failed.
    ShaderProgram* get(Id id, DemoTier tier) const;

    // Convenience: get shader without tier (returns T4/Ultra variant for Uber,
    // or the single compiled program for GL4/Compute/Tess).
    ShaderProgram* get(Id id) const;

    // Static metadata from registry
    static int minTier(Id id);
    static Type type(Id id);
    static const char* baseName(Id id);

    // Validate that critical shaders for each tier are present.
    // Returns the highest valid tier <= max_tier.
    int validateTier(int max_tier) const;

    void destroy();

    // Stats passthrough
    int compiledCount() const { return cache_.compiledCount(); }
    int cacheHitCount() const { return cache_.cacheHitCount(); }

private:
    static const int MAX_TIERS = 4;
    ShaderProgram* programs_[COUNT][MAX_TIERS];

    ShaderCache cache_;
    Renderer* renderer_;
};
