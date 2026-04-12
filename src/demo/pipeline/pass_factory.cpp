#include "demo/pipeline/pass_factory.h"
#include "demo/scene/demo_scene.h"
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
#include "demo/passes/ssr_copy_pass.h"
#include "demo/passes/dof_pass.h"
#include "demo/passes/torch_pass.h"
#include <cstring>

// Helper: create, init, push to vector.
// Safe: vector is pre-reserved so push_back never invalidates prior elements.
template<typename T>
static void make(std::vector<std::unique_ptr<RenderPassBase>>& out,
                 const TierResourceView& res) {
    out.push_back(std::unique_ptr<RenderPassBase>(new T()));
    static_cast<T*>(out.back().get())->init(res);
}

static RenderPassBase* findByName(const std::vector<std::unique_ptr<RenderPassBase>>& passes,
                                   const char* name) {
    for (size_t i = 0; i < passes.size(); i++)
        if (strcmp(passes[i]->name(), name) == 0) return passes[i].get();
    return nullptr;
}

void createPasses(std::vector<std::unique_ptr<RenderPassBase>>& out,
                  const TierResourceView& res,
                  const DemoTierConfig& cfg) {
    out.clear();
    out.reserve(24);
    (void)cfg;

    // Core passes (all tiers)
    make<SkyPass>(out, res);
    make<ShadowPass>(out, res);
    make<OpaquePass>(out, res);
    make<GrassInstancedPass>(out, res);
    make<FurPass>(out, res);
    make<ParticlePass>(out, res);

    // T3+ torch flames at point lights
    make<TorchPass>(out, res);

    // T2+ post-processing
    make<SSAOPass>(out, res);
    make<BloomPass>(out, res);
    make<CompositePass>(out, res);

    // T4 passes
    make<HDRCompositePass>(out, res);
    make<ComputeParticlesPass>(out, res);
    make<ComputeParticlesDrawPass>(out, res);
    make<GTAOPass>(out, res);
    make<BloomComputePass>(out, res);
    make<AutoExposurePass>(out, res);
    make<VolumetricFogPass>(out, res);
    make<WaterPass>(out, res);
    make<SSRPass>(out, res);
    make<SSRCopyPass>(out, res);
    make<DoFPass>(out, res);

    // SceneToFBOPass (needs sub-pass wiring, no init)
    {
        out.push_back(std::unique_ptr<RenderPassBase>(new SceneToFBOPass()));
        SceneToFBOPass* fbo = static_cast<SceneToFBOPass*>(out.back().get());
        fbo->setSubPasses(
            findByName(out, "sky"),
            findByName(out, "opaque"),
            findByName(out, "grass_instanced"),
            findByName(out, "fur"),
            findByName(out, "particle"),
            findByName(out, "torch"));
    }

    // TessellatedModelPass (needs init + setFurPass)
    {
        out.push_back(std::unique_ptr<RenderPassBase>(new TessellatedModelPass()));
        TessellatedModelPass* tess = static_cast<TessellatedModelPass*>(out.back().get());
        tess->init(res);
        tess->setFurPass(findByName(out, "fur"));
    }
}
