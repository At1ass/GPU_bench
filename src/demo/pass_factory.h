#pragma once
#include "demo/render_pass.h"
#include "demo/tier_resource_view.h"
#include <memory>
#include <vector>

// Forward declarations — DemoScene doesn't need to know concrete types
struct DemoTierConfig;
enum class DemoTier;

// Named pass pointers for pipeline wiring (non-owning, into passes_ vector)
struct DemoPassSet {
    DemoRenderPass* sky;
    DemoRenderPass* shadow;
    DemoRenderPass* opaque;
    DemoRenderPass* grass;
    DemoRenderPass* fur;
    DemoRenderPass* particle;
    DemoRenderPass* ssao;
    DemoRenderPass* bloom;
    DemoRenderPass* composite;
    DemoRenderPass* scene_to_fbo;
    DemoRenderPass* hdr_composite;
    DemoRenderPass* compute_particles;
    DemoRenderPass* compute_particles_draw;
    DemoRenderPass* tess_model;
    DemoRenderPass* gtao;
    DemoRenderPass* bloom_compute;
    DemoRenderPass* auto_exposure;
    DemoRenderPass* vol_fog;
    DemoRenderPass* water;
    DemoRenderPass* ssr;
    DemoRenderPass* dof;

    DemoPassSet() : sky(nullptr), shadow(nullptr), opaque(nullptr),
        grass(nullptr), fur(nullptr), particle(nullptr), ssao(nullptr),
        bloom(nullptr), composite(nullptr), scene_to_fbo(nullptr),
        hdr_composite(nullptr), compute_particles(nullptr),
        compute_particles_draw(nullptr), tess_model(nullptr),
        gtao(nullptr), bloom_compute(nullptr), auto_exposure(nullptr),
        vol_fog(nullptr), water(nullptr), ssr(nullptr), dof(nullptr) {}
};

// Create all render passes for a given tier. Returns owned pointers
// in the vector and named non-owning pointers in the pass set.
// DemoScene stores the vector (ownership) and uses the set (wiring).
DemoPassSet createPasses(std::vector<std::unique_ptr<DemoRenderPass>>& out,
                         const TierResourceView& res,
                         const DemoTierConfig& cfg);
