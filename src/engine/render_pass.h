#pragma once
#include "engine/frame_data.h"
#include "engine/resource_id.h"
#include "engine/scene_data.h"

class Renderer;
class PassContext;

// Typed role for pipeline builder routing (replaces strcmp on pass name).
enum class PassRole {
    Default,        // ordinary pass, no special pipeline handling
    SceneContainer, // manages own FBO (scene_to_fbo)
    FinalComposite, // renders to dest framebuffer (composite, hdr_composite)
};

// Abstract base for composable render passes.
//
// Engine-generic: no application-specific types in the interface.
// Application-specific resources (shader configs, tier views, etc.)
// are stored by application-level adapter classes, not passed as parameters.
//
// Each pass declares:
//   - name()           — unique identifier
//   - execute()        — rendering logic
//   - resourceDecls()  — what resources it reads/writes (for dependency graph)
//   - executionOrder() — tie-breaker for passes at same topological depth
//   - queueType()      — Graphics/Compute hint (for future Vulkan async)
//   - passRole()       — pipeline RT routing hint
//
// Pass filtering (tier gating, enable/disable) is application-level concern.
// Application adapters (e.g., demo_pass_base.h) provide their own methods.

class RenderPassBase {
public:
    virtual ~RenderPassBase() = default;
    RenderPassBase(const RenderPassBase&) = delete;
    RenderPassBase& operator=(const RenderPassBase&) = delete;

    // Identity
    virtual const char* name() const = 0;

    // Rendering — engine-generic signature
    virtual void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) = 0;

    // Resource dependency declarations (static, zero-cost)
    virtual const ResourceDecl* resourceDecls() const { return nullptr; }
    virtual int resourceDeclCount() const { return 0; }

    // Ordering hint: passes at same topological depth sorted by this value
    virtual int executionOrder() const { return 100; }

    // Queue type hint (OpenGL: always Graphics; Vulkan: async compute possible)
    virtual QueueType queueType() const { return QueueType::Graphics; }

    // Pipeline role: determines special RT handling in PipelineBuilder.
    virtual PassRole passRole() const { return PassRole::Default; }

protected:
    RenderPassBase() = default;
};
