#pragma once
// Demo-level adapter layer: bridges engine's generic pass interfaces
// with demo-specific types (TierResourceView, DemoTierConfig, DemoDebugOverrides).
//
// Engine pass base classes use generic signatures:
//   RenderPassBase::execute(PassContext&, FrameData&, const SceneData&)
//   FullscreenPass::onExecute(PassContext&, FrameData&, const SceneData&)
//   GeometryPass::onSceneSetup(UniformBlock&, PassContext&, const FrameData&)
//   ComputePassBase::onBind(PassContext&, UniformBlock&, const FrameData&)
//   ComputePassBase::onWorkgroups(const FrameData&, int&, int&, int&)
//
// Pass filtering (minTier, isEnabled) is demo-level concern, not engine.
// These adapter classes provide minTier()/isEnabled() as regular methods
// that the demo pipeline wrapper (demoBuildPipeline) calls for filtering.
//
// These adapter classes store demo-specific resources via configure() and
// translate generic callbacks into demo-typed virtual methods that concrete
// demo passes implement.

#include "engine/render_pass.h"
#include "engine/fullscreen_pass.h"
#include "engine/geometry_pass.h"
#include "engine/compute_pass.h"
#include "demo/tier/demo_tier.h"
#include "demo/pipeline/demo_pass_meta.h"

struct TierResourceView;
struct DemoTierConfig;
struct DemoDebugOverrides;

// ---------------------------------------------------------------------------
// DemoPassBase: adapter for passes extending RenderPassBase directly.
//
// Covers: SkyPass, SSAOPass, BloomPass, GTAOPass, HDRCompositePass,
//         SceneToFBOPass, TessellatedModelPass, BloomComputePass,
//         AutoExposurePass, ComputeParticlesDrawPass, ParticlePass,
//         FurPass, GrassInstancedPass, TorchPass, SSRCopyPass.
// ---------------------------------------------------------------------------

class DemoPassBase : public RenderPassBase, public DemoPassMeta {
public:
    // Called once at creation to provide typed demo resources.
    // Pointers are non-owning; caller (DemoScene/pass_factory) owns the data.
    void configure(const TierResourceView* res, const DemoTierConfig* cfg,
                   const DemoDebugOverrides* dbg) {
        res_ = res;
        cfg_ = cfg;
        dbg_ = dbg;
    }

    // --- Application-level typed interface (subclasses implement) ---

    // One-time setup with typed resources (called by pass_factory after creation).
    virtual void init(const TierResourceView& res) { (void)res; }

    // Minimum demo tier required for this pass.
    virtual DemoTier demoMinTier() const { return DemoTier::Basic; }

    // Runtime enable/disable check with full demo context.
    virtual bool checkEnabled(const DemoTierConfig& cfg,
                              const DemoDebugOverrides& dbg) const {
        (void)cfg;
        (void)dbg;
        return true;
    }

    // --- Demo-level filtering (DemoPassMeta, called by demoBuildPipeline) ---

    int minTier() const override {
        return static_cast<int>(demoMinTier());
    }

    bool isEnabled() const override {
        return (cfg_ && dbg_) ? checkEnabled(*cfg_, *dbg_) : true;
    }

    // Engine execute: delegates to typed doExecute.
    void execute(PassContext& ctx, FrameData& fd, const SceneData& scene) override {
        doExecute(ctx, fd, scene);
    }

protected:
    // Subclasses implement their rendering logic here.
    virtual void doExecute(PassContext& ctx, FrameData& fd,
                           const SceneData& scene) = 0;

    // Typed resource accessors for subclasses.
    const TierResourceView& res() const { return *res_; }
    const DemoTierConfig& cfg() const { return *cfg_; }
    const DemoDebugOverrides& dbg() const { return *dbg_; }

    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
};

// ---------------------------------------------------------------------------
// DemoFullscreenPass: adapter for passes extending FullscreenPass.
//
// Engine FullscreenPass handles RT binding, viewport, state, quad draw.
// This adapter translates the generic onExecute() callback into three
// demo-typed steps: setup -> inputs -> uniforms.
//
// Covers: CompositePass, VolumetricFogPass, SSRPass.
// ---------------------------------------------------------------------------

class DemoFullscreenPass : public FullscreenPass, public DemoPassMeta {
public:
    void configure(const TierResourceView* res, const DemoTierConfig* cfg,
                   const DemoDebugOverrides* dbg) {
        res_ = res;
        cfg_ = cfg;
        dbg_ = dbg;
    }

    // --- Application-level typed interface ---

    virtual void init(const TierResourceView& res) { (void)res; }
    virtual DemoTier demoMinTier() const { return DemoTier::Basic; }
    virtual bool checkEnabled(const DemoTierConfig& cfg,
                              const DemoDebugOverrides& dbg) const {
        (void)cfg;
        (void)dbg;
        return true;
    }

    // Three-phase fullscreen rendering (subclasses implement):
    //   setup()    — configure output RT, shader, quad (from stored resources)
    //   inputs()   — bind input textures for the current frame
    //   uniforms() — set shader uniforms
    virtual void setup(const TierResourceView& res) = 0;
    virtual void inputs(PassContext& ctx, const TierResourceView& res,
                        const FrameData& fd) = 0;
    virtual void uniforms(UniformBlock& ub, const FrameData& fd,
                          const DemoTierConfig& cfg) {
        (void)ub;
        (void)fd;
        (void)cfg;
    }

    // --- Demo-level filtering (DemoPassMeta, called by demoBuildPipeline) ---

    int minTier() const override {
        return static_cast<int>(demoMinTier());
    }

    bool isEnabled() const override {
        return (cfg_ && dbg_) ? checkEnabled(*cfg_, *dbg_) : true;
    }

    // FullscreenPass::onExecute -> demo typed three-phase rendering.
    void onExecute(PassContext& ctx, FrameData& fd,
                   const SceneData& scene) override {
        (void)scene;
        setup(*res_);
        inputs(ctx, *res_, fd);
        uniforms(ub(), fd, *cfg_);
    }

protected:
    const TierResourceView& res() const { return *res_; }
    const DemoTierConfig& cfg() const { return *cfg_; }
    const DemoDebugOverrides& dbg() const { return *dbg_; }

    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
};

// ---------------------------------------------------------------------------
// DemoGeometryPass: adapter for passes extending GeometryPass.
//
// Engine GeometryPass handles RT, state, object iteration, frustum culling,
// per-object transform + material uniforms, DrawList sorting.
// This adapter translates onSceneSetup() into a demo-typed callback.
//
// Covers: OpaquePass, ShadowPass, WaterPass, TorchPass.
// ---------------------------------------------------------------------------

class DemoGeometryPass : public GeometryPass, public DemoPassMeta {
public:
    void configure(const TierResourceView* res, const DemoTierConfig* cfg,
                   const DemoDebugOverrides* dbg) {
        res_ = res;
        cfg_ = cfg;
        dbg_ = dbg;
    }

    // --- Application-level typed interface ---

    virtual void init(const TierResourceView& res) { (void)res; }
    virtual DemoTier demoMinTier() const { return DemoTier::Basic; }
    virtual bool checkEnabled(const DemoTierConfig& cfg,
                              const DemoDebugOverrides& dbg) const {
        (void)cfg;
        (void)dbg;
        return true;
    }

    // Demo-typed scene setup: camera uniforms, light params, textures.
    virtual void sceneSetup(UniformBlock& ub, PassContext& ctx,
                            const FrameData& fd,
                            const TierResourceView& res,
                            const DemoTierConfig& cfg) = 0;

    // --- Demo-level filtering (DemoPassMeta, called by demoBuildPipeline) ---

    int minTier() const override {
        return static_cast<int>(demoMinTier());
    }

    bool isEnabled() const override {
        return (cfg_ && dbg_) ? checkEnabled(*cfg_, *dbg_) : true;
    }

    // GeometryPass::onSceneSetup -> demo typed sceneSetup.
    void onSceneSetup(UniformBlock& ub, PassContext& ctx,
                      const FrameData& fd) override {
        sceneSetup(ub, ctx, fd, *res_, *cfg_);
    }

protected:
    const TierResourceView& res() const { return *res_; }
    const DemoTierConfig& cfg() const { return *cfg_; }
    const DemoDebugOverrides& dbg() const { return *dbg_; }

    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
};

// ---------------------------------------------------------------------------
// DemoComputePass: adapter for passes extending ComputePassBase.
//
// Engine ComputePassBase handles feature check, shader bind, dispatch, barrier.
// This adapter translates onBind()/onWorkgroups() into demo-typed callbacks.
//
// Covers: DoFPass, GTAOPass (compute variant), BloomComputePass,
//         AutoExposurePass, SSRPass (compute), ComputeParticlesPass.
// ---------------------------------------------------------------------------

class DemoComputePass : public ComputePassBase, public DemoPassMeta {
public:
    void configure(const TierResourceView* res, const DemoTierConfig* cfg,
                   const DemoDebugOverrides* dbg) {
        res_ = res;
        cfg_ = cfg;
        dbg_ = dbg;
    }

    // --- Application-level typed interface ---

    virtual void init(const TierResourceView& res) { (void)res; }
    virtual DemoTier demoMinTier() const { return DemoTier::Basic; }
    virtual bool checkEnabled(const DemoTierConfig& cfg,
                              const DemoDebugOverrides& dbg) const {
        (void)cfg;
        (void)dbg;
        return true;
    }

    // Bind input/output resources and set uniforms (demo-typed).
    virtual void bind(PassContext& ctx, UniformBlock& ub,
                      const TierResourceView& res,
                      const FrameData& fd,
                      const DemoTierConfig& cfg) = 0;

    // Calculate dispatch workgroup count (demo-typed).
    virtual void workgroups(const FrameData& fd,
                            const DemoTierConfig& cfg,
                            int& gx, int& gy, int& gz) = 0;

    // --- Demo-level filtering (DemoPassMeta, called by demoBuildPipeline) ---

    int minTier() const override {
        return static_cast<int>(demoMinTier());
    }

    bool isEnabled() const override {
        return (cfg_ && dbg_) ? checkEnabled(*cfg_, *dbg_) : true;
    }

    // ComputePassBase::onBind -> demo typed bind.
    void onBind(PassContext& ctx, UniformBlock& ub,
                const FrameData& fd) override {
        bind(ctx, ub, *res_, fd, *cfg_);
    }

    // ComputePassBase::onWorkgroups -> demo typed workgroups.
    void onWorkgroups(const FrameData& fd,
                      int& gx, int& gy, int& gz) override {
        workgroups(fd, *cfg_, gx, gy, gz);
    }

protected:
    const TierResourceView& res() const { return *res_; }
    const DemoTierConfig& cfg() const { return *cfg_; }
    const DemoDebugOverrides& dbg() const { return *dbg_; }

    const TierResourceView* res_ = nullptr;
    const DemoTierConfig* cfg_ = nullptr;
    const DemoDebugOverrides* dbg_ = nullptr;
};
