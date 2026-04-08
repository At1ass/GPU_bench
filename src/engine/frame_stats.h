#pragma once

// Engine-level per-frame counters (scene/object management).
// GL call counters are now in RendererStats (renderer.h), counted where
// GL calls actually happen — follows bgfx/Godot/Ogre3D pattern.
//
// Usage:
//   FrameStats& stats = ctx.stats();
//   stats.reset();             // at frame start (in beginFrame)
//   stats.objects_drawn++;     // in geometry pass
//   // ... at end of frame, pass to DemoUI for display

struct FrameStats {
    // Scene
    int objects_drawn;      // passed frustum culling
    int objects_culled;     // failed frustum culling

    void reset() { objects_drawn = 0; objects_culled = 0; }
};
