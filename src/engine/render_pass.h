#pragma once
#include "engine/frame_data.h"
#include "engine/resource_id.h"
#include "engine/scene_data.h"

class Renderer;
class PassContext;
struct TierResourceView;
struct DemoTierConfig;
struct DemoDebugOverrides;

// Typed role for pipeline builder routing (replaces strcmp on pass name).
enum class PassRole {
    Default,        // ordinary pass, no special pipeline handling
    SceneContainer, // manages own FBO (scene_to_fbo)
    FinalComposite, // renders to dest framebuffer (composite, hdr_composite)
};

// Abstract base for composable demo render passes.
//
// Each pass declares:
//   - name()           — unique identifier
//   - execute()        — rendering logic
//   - resourceDecls()  — what resources it reads/writes (for dependency graph)
//   - minTier()        — minimum tier required to run
//   - isEnabled()      — runtime condition (config flags, debug overrides)
//   - executionOrder() — tie-breaker for passes at same topological depth
//   - queueType()      — Graphics/Compute hint (for future Vulkan async)
//
// Passes communicate via FrameData (shared mutable state) and
// TierResourceView (read-only GPU resources).

class RenderPassBase {
public:
    virtual ~RenderPassBase() = default;
    RenderPassBase(const RenderPassBase&) = delete;
    RenderPassBase& operator=(const RenderPassBase&) = delete;

    // Identity
    virtual const char* name() const = 0;

    // Rendering
    virtual void execute(PassContext& ctx, FrameData& fd,
                         const TierResourceView& res,
                         const DemoTierConfig& cfg,
                         const SceneData& scene) = 0;

    // Resource dependency declarations (static, zero-cost)
    virtual const ResourceDecl* resourceDecls() const { return nullptr; }
    virtual int resourceDeclCount() const { return 0; }

    // Tier and feature gating
    virtual DemoTier minTier() const { return DemoTier::Basic; }
    virtual bool isEnabled(const DemoTierConfig& cfg,
                           const DemoDebugOverrides& dbg) const {
        (void)cfg; (void)dbg; return true;
    }

    // Ordering hint: passes at same topological depth sorted by this value
    virtual int executionOrder() const { return 100; }

    // Queue type hint (OpenGL: always Graphics; Vulkan: async compute possible)
    virtual QueueType queueType() const { return QueueType::Graphics; }

    // Pipeline role: determines special RT handling in PipelineBuilder.
    virtual PassRole passRole() const { return PassRole::Default; }

protected:
    RenderPassBase() = default;
};
