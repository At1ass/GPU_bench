#pragma once

// Per-frame GL call counters. Incremented in PassContext/StateCache.
// Displayed in DemoUI when --debug is active.
// Pattern: Quake r_speeds, DOOM com_showFPS, Godot Performance Monitor.
//
// Usage:
//   FrameStats& stats = ctx.stats();
//   stats.reset();         // at frame start (in beginFrame)
//   stats.draw_calls++;    // in each drawMesh
//   // ... at end of frame, pass to DemoUI for display

struct FrameStats {
    // State management
    int state_applied;      // applyState() calls that changed GL state
    int state_skipped;      // calls skipped by StateCache (redundant)

    // Resources
    int draw_calls;         // drawMesh() + drawMeshInstanced()
    int texture_binds;      // bindTextureUnit() actual GL calls
    int texture_skipped;    // skipped by texture cache
    int rt_switches;        // bindRenderTarget() actual GL calls

    // Compute
    int compute_dispatches;
    int barriers_issued;

    // Shaders
    int shader_switches;    // useCustomShader() actual
    int uniform_calls;      // glUniform* via UniformBlock

    // Scene
    int objects_drawn;      // passed frustum culling
    int objects_culled;     // failed frustum culling

    void reset() {
        state_applied = 0; state_skipped = 0;
        draw_calls = 0;
        texture_binds = 0; texture_skipped = 0;
        rt_switches = 0;
        compute_dispatches = 0; barriers_issued = 0;
        shader_switches = 0; uniform_calls = 0;
        objects_drawn = 0; objects_culled = 0;
    }
};
