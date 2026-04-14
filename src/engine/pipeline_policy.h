#pragma once
#include "renderer/renderer.h"

class RenderPassBase;

// Result of RT routing decision for a single pass.
// Returned by PipelinePolicy::routePass().
struct PassRTBinding {
    enum Action : unsigned char {
        None,     // don't change RT (continue with current)
        BindRT,   // bind specific render target
        BindDest, // bind destination (resolved at execute time from FrameData)
        Unbind    // unbind RT (switch to default framebuffer)
    };
    Action action;
    RenderTargetHandle rt;
    bool clear;
    float clear_color[4];
    int viewport_w, viewport_h;  // 0 = use frame default

    PassRTBinding()
        : action(None), rt(), clear(false),
          viewport_w(0), viewport_h(0) {
        clear_color[0] = clear_color[1] = clear_color[2] = 0.0f;
        clear_color[3] = 1.0f;
    }
};

// Sync hints for a pass (barriers, image unbind).
struct PassSyncHint {
    bool barrier_after;
    int unbind_image_count;  // >0 = unbind image units 0..N-1 after execute

    PassSyncHint() : barrier_after(false), unbind_image_count(0) {}
};

// Application-level pipeline policy interface.
// Controls how the engine pipeline builder routes passes to render targets
// and inserts synchronization barriers.
//
// Called during buildPipeline() (once per configuration change, NOT per frame).
// The engine handles topological sort and execution; the policy handles
// application-specific RT routing and sync decisions.
//
// Example (minimal policy — render everything to default framebuffer):
//   class SimplePipelinePolicy : public PipelinePolicy {
//       PassRTBinding routePass(const RenderPassBase&, int, int) const override {
//           return PassRTBinding();  // default FB, no RT binding
//       }
//   };
class PipelinePolicy {
public:
    virtual ~PipelinePolicy() = default;

    // RT routing: where should this pass render?
    // Called once per pass during pipeline build, in topological order.
    virtual PassRTBinding routePass(const RenderPassBase& pass,
                                    int sorted_index, int total_passes) const = 0;

    // Sync: what barriers are needed after this pass?
    // Default: barrier after compute passes.
    virtual PassSyncHint syncHint(const RenderPassBase& pass) const;

    // Called before the first pass (for default FB clear, initial RT bind, etc.)
    virtual PassRTBinding pipelinePrologue() const { return PassRTBinding(); }

protected:
    PipelinePolicy() = default;
};
