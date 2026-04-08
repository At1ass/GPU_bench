#pragma once

// Demo tier levels matching GL capability.
// Shared across pass system, scene, and pipeline.
enum class DemoTier {
    Basic    = 1,  // GL 2.1: forward Blinn-Phong, basic fog
    Enhanced = 2,  // GL 3.0+: shadow map, SSAO, bloom
    Quality  = 3,  // GL 3.3+: PCF, point lights, particles, DoF
    Ultra    = 4   // GL 4.3+: PBR, compute particles, tessellation, vol fog
};

// Logical resource identifiers for inter-pass dependencies.
// Passes declare which resources they read/write.
// Builder uses these to determine execution order via topological sort.
enum class ResourceId {
    ShadowMap,       // shadow depth texture
    SceneColor,      // LDR scene color FBO (T2/T3)
    SceneDepth,      // scene depth from FBO (for SSAO)
    HDRColor,        // HDR scene render target (T4)
    HDRDepth,        // HDR depth texture (T4)
    AOResult,        // ambient occlusion output (SSAO or GTAO)
    BloomResult,     // bloom output (fragment or compute)
    FogResult,       // volumetric fog output
    SSRResult,       // screen-space reflections
    DoFResult,       // depth of field output
    ExposureData,    // auto-exposure SSBO
    ParticleData,    // compute particle SSBO
    COUNT
};

// Resource access mode for inter-pass dependency tracking.
enum class ResourceAccess : unsigned char { Read, Write };

// Resource access declaration for a single pass.
struct ResourceDecl {
    ResourceId id;
    ResourceAccess access;

    // Legacy compatibility aliases (used extensively in pass headers)
    static constexpr ResourceAccess READ  = ResourceAccess::Read;
    static constexpr ResourceAccess WRITE = ResourceAccess::Write;
};

// Queue type hint for future Vulkan async compute scheduling.
// On OpenGL all passes run on Graphics queue (single-threaded).
enum class QueueType : unsigned char {
    Graphics,   // rasterization, fragment shaders
    Compute,    // compute shader dispatch
    Transfer    // data upload/copy (future)
};
