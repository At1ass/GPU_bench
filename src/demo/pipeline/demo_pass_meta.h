#pragma once

// Demo-level pass metadata interface for pipeline filtering.
//
// Engine's RenderPassBase is intentionally free of filtering concerns
// (tier gating, enable/disable). This interface provides those methods
// at the demo level. Concrete demo passes inherit from this alongside
// their engine base (RenderPassBase/GeometryPass/FullscreenPass/ComputePassBase).
//
// The demo pipeline wrapper (demoBuildPipeline) uses dynamic_cast<DemoPassMeta*>
// to extract filtering info. Passes that don't implement DemoPassMeta are
// always included (minTier=1, isEnabled=true).

class DemoPassMeta {
public:
    virtual ~DemoPassMeta() = default;

    // Minimum demo tier level required for this pass.
    virtual int minTier() const = 0;

    // Runtime enable/disable (based on stored config/debug state).
    virtual bool isEnabled() const = 0;

protected:
    DemoPassMeta() = default;
};
