#pragma once

// Demo quality tier levels matching GL capability.
// This is a demo-specific concept — the engine layer uses int tier levels.
enum class DemoTier {
    Basic    = 1,  // GL 2.1: forward Blinn-Phong, basic fog
    Enhanced = 2,  // GL 3.0+: shadow map, SSAO, bloom
    Quality  = 3,  // GL 3.3+: PCF, point lights, particles, DoF
    Ultra    = 4   // GL 4.3+: PBR, compute particles, tessellation, vol fog
};
