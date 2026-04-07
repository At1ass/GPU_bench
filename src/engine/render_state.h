#pragma once

// Declarative GL render state description.
// Applied atomically via PassContext::applyState().
// Pattern from sokol_gfx (sg_pipeline) adapted for GL 2.1+.
//
// Usage:
//   ctx.applyState(RenderState::opaque());   // depth + cull, no blend
//   ctx.applyState(RenderState::fullscreen()); // no depth/cull/blend

struct RenderState {
    bool depth_test;       // glEnable(GL_DEPTH_TEST)
    bool depth_write;      // glDepthMask()
    bool cull_face;        // glEnable(GL_CULL_FACE)
    bool blending;         // glEnable(GL_BLEND)
    bool additive_blend;   // GL_ONE + GL_ONE (vs SRC_ALPHA + ONE_MINUS_SRC_ALPHA)

    bool color_mask_r;     // glColorMask() components
    bool color_mask_g;
    bool color_mask_b;
    bool color_mask_a;

    bool polygon_offset;   // glEnable(GL_POLYGON_OFFSET_FILL)
    float offset_factor;   // glPolygonOffset(factor, units)
    float offset_units;

    static RenderState opaque() {
        return { true, true, true, false, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState transparent() {
        return { true, false, false, true, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState additive() {
        return { true, false, false, true, true,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState fullscreen() {
        return { false, false, false, false, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState shadow() {
        return { true, true, true, false, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState depth_only() {
        return { true, true, true, false, false,
                 false, false, false, false,
                 true, 1.1f, 4.0f };
    }
};
