#pragma once

// Fixed texture unit assignments for demo scene.
// Shaders bind samplers to these unit numbers.
// Convention: unit does not change between passes within a frame.
//
// Ref: Khronos best practices — fixed binding points reduce state changes.

namespace TexSlot {
    enum : int {
        Primary      = 0,   // main texture (albedo, scene color, fur texture)
        Secondary    = 1,   // secondary (noise, mask, fur mask, bloom mips)
        AO           = 2,   // ambient occlusion result
        ShadowMap    = 3,   // shadow depth map
        NormalMap    = 4,   // normal map
        SSRColor     = 5,   // SSR color snapshot
        SSRDepth     = 6,   // SSR depth snapshot
        PostSource   = 7,   // post-process source (ping-pong)
        Depth        = 8,   // scene depth (HDR depth for SSAO/GTAO/SSR/DoF/VolumetricFog)
        Fog          = 9,   // volumetric fog output texture
    };
}
