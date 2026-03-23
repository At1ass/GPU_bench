#include "demo/pass_factory.h"
#include "demo/demo_scene.h"
#include "demo/passes/sky_pass.h"
#include "demo/passes/shadow_pass.h"
#include "demo/passes/opaque_pass.h"
#include "demo/passes/grass_instanced_pass.h"
#include "demo/passes/fur_pass.h"
#include "demo/passes/particle_pass.h"
#include "demo/passes/ssao_pass.h"
#include "demo/passes/bloom_pass.h"
#include "demo/passes/composite_pass.h"
#include "demo/passes/scene_to_fbo_pass.h"
#include "demo/passes/hdr_composite_pass.h"
#include "demo/passes/compute_particles_pass.h"
#include "demo/passes/compute_particles_draw_pass.h"
#include "demo/passes/tessellated_model_pass.h"
#include "demo/passes/gtao_pass.h"
#include "demo/passes/bloom_compute_pass.h"
#include "demo/passes/auto_exposure_pass.h"
#include "demo/passes/volumetric_fog_pass.h"
#include "demo/passes/water_pass.h"
#include "demo/passes/ssr_pass.h"
#include "demo/passes/dof_pass.h"

// Helper: create, init, push to vector, return raw pointer
template<typename T>
static T* make(std::vector<std::unique_ptr<DemoRenderPass>>& out,
               const TierResourceView& res) {
    std::unique_ptr<T> p(new T());
    p->init(res);
    T* raw = p.get();
    out.push_back(std::move(p));
    return raw;
}

DemoPassSet createPasses(std::vector<std::unique_ptr<DemoRenderPass>>& out,
                         const TierResourceView& res,
                         const DemoTierConfig& cfg) {
    out.clear();
    out.reserve(21);

    DemoPassSet s;

    // Core passes (all tiers)
    s.sky      = make<SkyPass>(out, res);
    s.shadow   = make<ShadowPass>(out, res);
    s.opaque   = make<OpaquePass>(out, res);
    s.grass    = make<GrassInstancedPass>(out, res);
    s.fur      = make<FurPass>(out, res);
    s.particle = make<ParticlePass>(out, res);

    // T2+ post-processing
    s.ssao       = make<SSAOPass>(out, res);
    s.bloom      = make<BloomPass>(out, res);
    s.composite  = make<CompositePass>(out, res);

    // T2/T3 scene-to-FBO wrapper
    {
        auto p = std::unique_ptr<SceneToFBOPass>(new SceneToFBOPass());
        p->setSubPasses(s.sky, s.opaque, s.grass, s.fur, s.particle);
        s.scene_to_fbo = p.get();
        out.push_back(std::move(p));
    }

    // T4 passes
    s.hdr_composite        = make<HDRCompositePass>(out, res);
    s.compute_particles    = make<ComputeParticlesPass>(out, res);
    s.compute_particles_draw = make<ComputeParticlesDrawPass>(out, res);

    {
        auto p = std::unique_ptr<TessellatedModelPass>(new TessellatedModelPass());
        p->init(res);
        p->setFurPass(s.fur);
        s.tess_model = p.get();
        out.push_back(std::move(p));
    }

    s.gtao           = make<GTAOPass>(out, res);
    s.bloom_compute  = make<BloomComputePass>(out, res);
    s.auto_exposure  = make<AutoExposurePass>(out, res);
    s.vol_fog        = make<VolumetricFogPass>(out, res);
    s.water          = make<WaterPass>(out, res);
    s.ssr            = make<SSRPass>(out, res);
    s.dof            = make<DoFPass>(out, res);

    (void)cfg; // cfg available for future conditional pass creation
    return s;
}
