#pragma once
#include "demo/render_pass.h"
#include "engine/pass_context.h"
#include "engine/render_state.h"
#include "engine/draw_list.h"
#include "demo/uniform_block.h"
#include "demo/scene_data.h"
#include "demo/demo_utils.h"

// Template for "geometry draw" passes: camera + objects -> shader -> RT.
// Automatically handles: RT binding, state, object iteration, frustum culling,
//                        per-object transform + material uniforms.
// Author implements: setup(), sceneSetup().
// Optionally overrides: objectFilter(), perObject(), objectList().
//
// Covers: opaque, shadow, water, torch, particle (~5 passes).

class GeometryPass : public DemoRenderPass {
public:
    virtual void setup(const TierResourceView& res) = 0;

    // Called once before object iteration.
    // Set camera uniforms, light params, textures here.
    virtual void sceneSetup(UniformBlock& ub, PassContext& ctx,
                            const FrameData& fd,
                            const TierResourceView& res,
                            const DemoTierConfig& cfg) = 0;

    // Return false to skip object. Default: frustum culling.
    virtual bool objectFilter(const SceneObject& obj,
                              const FrameData& fd) {
        return sphereInFrustum(fd.frustum,
                               obj.bounds_center, obj.bounds_radius);
    }

    // Per-object uniforms. Default: Model + full material properties.
    virtual void perObject(UniformBlock& ub, PassContext& ctx,
                           const SceneObject& obj) {
        ub.set(U::Model, obj.transform);
        ub.set(U::MatColor, obj.mat.color_a);
        ub.set(U::MatColorB, obj.mat.color_b);
        ub.set(U::Metallic, obj.mat.metallic);
        ub.set(U::Roughness, obj.mat.roughness);
        ub.set(U::NoiseScale, obj.mat.noise_scale);
        ub.set(U::NoiseIntensity, obj.mat.noise_intensity);
        ub.set(U::WarpStrength, obj.mat.warp_strength);
        ub.set(U::DetailScale, obj.mat.detail_scale);
        ub.set(U::ProcTex, static_cast<float>(
                   static_cast<int>(obj.mat.proc_type)));
        ub.set(U::MatSpec, obj.mat.specular);
    }

    // Which object list to draw. Default: opaque_objects.
    virtual const std::vector<SceneObject>* objectList(
            const SceneData& scene) {
        return scene.opaque_objects;
    }

    // Opt-in DrawList sorting (default: false = iterate in construction order).
    // When enabled, objects are sorted by sort key before drawing to minimize
    // shader/material/depth state changes. Override shaderIdFor/materialIdFor
    // to provide sort key components.
    virtual bool useSortedDraw() const { return false; }
    virtual uint8_t shaderIdFor(const SceneObject& obj) const { (void)obj; return 0; }
    virtual uint8_t materialIdFor(const SceneObject& obj) const { (void)obj; return 0; }

    void execute(PassContext& ctx, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader) { shader_ = shader; }
    void setOutputRT(RenderTargetHandle rt, int w, int h) {
        output_rt_ = rt; out_w_ = w; out_h_ = h;
    }
    void setState(const RenderState& state) { state_ = state; }
    void setClearColor(float r, float g, float b, float a) {
        has_clear_ = true;
        clear_[0] = r; clear_[1] = g; clear_[2] = b; clear_[3] = a;
    }
    // Skip RT binding in execute() — for passes where pipeline manages RT
    // (e.g., OpaquePass/WaterPass where SceneToFBO already binds the target)
    void setPipelineManagedRT() { pipeline_managed_rt_ = true; }
    UniformBlock& ub() { return ub_; }

private:
    UniformBlock ub_;
    ShaderProgram* shader_ = nullptr;
    RenderTargetHandle output_rt_;
    int out_w_ = 0, out_h_ = 0;
    RenderState state_ = RenderState::opaque();
    bool has_clear_ = false;
    float clear_[4] = {};
    bool pipeline_managed_rt_ = false;
};
