#pragma once
#include "demo/demo_frame_data.h"
#include "demo/scene_data.h"

class Renderer;
struct TierResourceView;
struct DemoTierConfig;

// Abstract base for composable demo render passes.
// Each pass implements execute() with a uniform signature.
// Passes communicate via FrameData (read/write shared state).
class DemoRenderPass {
public:
    virtual ~DemoRenderPass() {}
    DemoRenderPass(const DemoRenderPass&) = delete;
    DemoRenderPass& operator=(const DemoRenderPass&) = delete;

    virtual const char* name() const = 0;
    virtual void execute(Renderer* r, FrameData& fd,
                         const TierResourceView& res,
                         const DemoTierConfig& cfg,
                         const SceneData& scene) = 0;

protected:
    DemoRenderPass() = default;
};
