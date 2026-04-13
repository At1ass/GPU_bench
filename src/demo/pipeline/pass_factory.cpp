#include "demo/pipeline/pass_factory.h"
#include "demo/scene/demo_scene.h"
#include "demo/passes/all_passes.h"
// Special passes (need sub-pass wiring, not in registry)
#include "demo/passes/scene_to_fbo_pass.h"
#include "demo/passes/tessellated_model_pass.h"
#include <cstring>

// Helper: create, init, push to vector.
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

    // --- All standard passes from registry ---
    #define X(Class) make<Class>(out, res);
    #include "demo/pipeline/pass_registry.def"
    #undef X

    // --- Special passes (need sub-pass wiring) ---
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
    {
        out.push_back(std::unique_ptr<RenderPassBase>(new TessellatedModelPass()));
        TessellatedModelPass* tess = static_cast<TessellatedModelPass*>(out.back().get());
        tess->init(res);
        tess->setFurPass(findByName(out, "fur"));
    }
}
