#pragma once

class Renderer;
class RenderContext;
class BenchCallbacks;

// Encapsulates the combined GPU stress test.
// Stresses all GPU units: shader cores (FMA), SFU (sin/cos), TMU, ROP.
// Self-calibrating: adjusts pass count to saturate any GPU.
class StressRunner {
public:
    // Run the stress test.
    // shader_iterations: ALU loop count per pixel (from preset)
    // duration_sec: how long to run (seconds)
    void run(Renderer* r, RenderContext* ctx,
             int shader_iterations,
             int render_w, int render_h,
             int duration_sec,
             BenchCallbacks* cb);
};
