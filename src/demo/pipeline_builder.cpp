#include "demo/pipeline_builder.h"
#include "demo/demo_scene.h"
#include "demo/demo_utils.h"
#include "demo/resource_id.h"
#include "platform/logger.h"
#include <vector>

// ============================================================
// Topological-sort pipeline builder
// ============================================================
//
// 1. Filter passes by tier and isEnabled()
// 2. Build adjacency graph from resource read/write declarations
// 3. Kahn's algorithm with executionOrder() tie-breaking
// 4. Emit PipelineNodes with RT bindings/barriers

void buildPipeline(DemoPipeline& pipeline,
                   const std::vector<std::unique_ptr<DemoRenderPass>>& passes,
                   const DemoTierConfig& config,
                   const DemoDebugOverrides& debug,
                   const TierResourceView& res,
                   int viewport_w, int viewport_h) {
    pipeline.clear();

    int tier_int = static_cast<int>(config.tier);

    // ----------------------------------------------------------
    // Step 1: collect enabled passes
    // ----------------------------------------------------------
    std::vector<DemoRenderPass*> enabled;
    enabled.reserve(passes.size());
    for (size_t i = 0; i < passes.size(); i++) {
        DemoRenderPass* p = passes[i].get();
        if (static_cast<int>(p->minTier()) > tier_int) continue;
        if (!p->isEnabled(config, debug)) continue;
        enabled.push_back(p);
    }

    int n = static_cast<int>(enabled.size());
    if (n == 0) return;

    // ----------------------------------------------------------
    // Step 2: build adjacency from resource declarations
    // ----------------------------------------------------------
    static const size_t RES_COUNT = static_cast<size_t>(ResourceId::COUNT);

    // For each ResourceId, track which enabled-pass indices write/read it
    std::vector<size_t> writers_buf[RES_COUNT];
    std::vector<size_t> readers_buf[RES_COUNT];

    for (size_t i = 0; i < static_cast<size_t>(n); i++) {
        const ResourceDecl* decls = enabled[i]->resourceDecls();
        int dc = enabled[i]->resourceDeclCount();
        for (int d = 0; d < dc; d++) {
            size_t rid = static_cast<size_t>(decls[d].id);
            if (decls[d].access == ResourceDecl::WRITE)
                writers_buf[rid].push_back(i);
            else
                readers_buf[rid].push_back(i);
        }
    }

    // Adjacency: edge[i] = set of passes that must run after pass i
    // Use a flat bool matrix to deduplicate edges (max 22x22 = 484 entries)
    std::vector<std::vector<bool> > has_edge(static_cast<size_t>(n),
                                              std::vector<bool>(static_cast<size_t>(n), false));
    std::vector<std::vector<size_t> > adj(static_cast<size_t>(n));
    std::vector<int> in_degree(static_cast<size_t>(n), 0);

    for (size_t rid = 0; rid < RES_COUNT; rid++) {
        for (size_t wi = 0; wi < writers_buf[rid].size(); wi++) {
            size_t w = writers_buf[rid][wi];
            for (size_t ri = 0; ri < readers_buf[rid].size(); ri++) {
                size_t r = readers_buf[rid][ri];
                if (w == r) continue;
                // Skip edge if reader is also a writer of the same resource.
                // Co-writers into the same RT are ordered by executionOrder().
                bool reader_also_writes = false;
                for (size_t wj = 0; wj < writers_buf[rid].size(); wj++) {
                    if (writers_buf[rid][wj] == r) { reader_also_writes = true; break; }
                }
                if (reader_also_writes) continue;
                // Deduplicate: only add edge once
                if (!has_edge[w][r]) {
                    has_edge[w][r] = true;
                    adj[w].push_back(r);
                    in_degree[r]++;
                }
            }
        }
    }

    // ----------------------------------------------------------
    // Step 3: Kahn's algorithm with executionOrder() tie-breaking
    // ----------------------------------------------------------
    // Queue: passes with in_degree 0, sorted by executionOrder()
    std::vector<size_t> queue;
    queue.reserve(static_cast<size_t>(n));
    for (size_t i = 0; i < static_cast<size_t>(n); i++)
        if (in_degree[i] == 0) queue.push_back(i);

    std::vector<DemoRenderPass*> sorted;
    sorted.reserve(static_cast<size_t>(n));

    while (!queue.empty()) {
        // Find the queue element with smallest executionOrder (tie-break)
        size_t best_idx = 0;
        int best_order = enabled[queue[0]]->executionOrder();
        for (size_t qi = 1; qi < queue.size(); qi++) {
            int order = enabled[queue[qi]]->executionOrder();
            if (order < best_order) {
                best_order = order;
                best_idx = qi;
            }
        }
        size_t u = queue[best_idx];
        queue.erase(queue.begin() + static_cast<ptrdiff_t>(best_idx));
        sorted.push_back(enabled[u]);

        for (size_t ei = 0; ei < adj[u].size(); ei++) {
            size_t v = adj[u][ei];
            in_degree[v]--;
            if (in_degree[v] == 0)
                queue.push_back(v);
        }
    }

    // Warn if cycle detected (should never happen with valid pass graph)
    if (static_cast<int>(sorted.size()) != n) {
        LOG_ERR("Pipeline: topological sort found cycle! %d of %d passes sorted",
                static_cast<int>(sorted.size()), n);
        for (size_t i = 0; i < static_cast<size_t>(n); i++) {
            if (in_degree[i] > 0)
                LOG_ERR("  stuck: '%s' (in_degree=%d, order=%d)",
                        enabled[i]->name(), in_degree[i], enabled[i]->executionOrder());
        }
    }

    // ----------------------------------------------------------
    // Step 4: determine pipeline path and emit nodes
    // ----------------------------------------------------------
    // Detect rendering path: resource writers + actual RT availability
    bool has_hdr_writer = false;
    bool has_scene_writer = false;
    bool hdr_rt_available = res.t4.hdr_scene_rt != INVALID_RENDER_TARGET;
    bool scene_rt_available = res.bloom.scene_rt != INVALID_RENDER_TARGET;
    for (size_t i = 0; i < sorted.size(); i++) {
        const ResourceDecl* decls = sorted[i]->resourceDecls();
        int dc = sorted[i]->resourceDeclCount();
        for (int d = 0; d < dc; d++) {
            if (decls[d].access == ResourceDecl::WRITE) {
                if (decls[d].id == ResourceId::HDRColor) has_hdr_writer = true;
                if (decls[d].id == ResourceId::SceneColor) has_scene_writer = true;
            }
        }
    }

    // Path selection: resource writers + RT availability
    has_hdr_writer = has_hdr_writer && hdr_rt_available;
    has_scene_writer = has_scene_writer && scene_rt_available;

    float fogR = FOG_COLOR.x, fogG = FOG_COLOR.y, fogB = FOG_COLOR.z;
    int shadow_size = res.shadow.map_size > 0 ? res.shadow.map_size : 1024;

    if (has_hdr_writer) {
        // ============================================================
        // T4 HDR pipeline path
        // ============================================================
        // Passes are sorted by dependencies. We need to:
        // - Bind shadow RT for ShadowPass
        // - Bind HDR RT before first HDRColor writer (with clear)
        // - Unbind HDR RT before first post-processing pass
        // - Add barrier before final composite
        // - Composite renders to dest

        bool hdr_rt_bound = false;
        bool hdr_rt_unbound = false;

        for (size_t i = 0; i < sorted.size(); i++) {
            DemoRenderPass* pass = sorted[i];


            bool writes_shadow = false;
            bool writes_hdr = false;
            bool reads_hdr_post = false; // reads HDR but doesn't write it (post-proc)

            const ResourceDecl* decls = pass->resourceDecls();
            int dc = pass->resourceDeclCount();
            bool w_hdr = false, r_hdr = false, r_hdrd = false;
            for (int d = 0; d < dc; d++) {
                if (decls[d].id == ResourceId::ShadowMap && decls[d].access == ResourceDecl::WRITE)
                    writes_shadow = true;
                if (decls[d].id == ResourceId::HDRColor && decls[d].access == ResourceDecl::WRITE)
                    w_hdr = true;
                if (decls[d].id == ResourceId::HDRColor && decls[d].access == ResourceDecl::READ)
                    r_hdr = true;
                if (decls[d].id == ResourceId::HDRDepth && decls[d].access == ResourceDecl::READ)
                    r_hdrd = true;
            }
            writes_hdr = w_hdr;
            reads_hdr_post = (r_hdr || r_hdrd) && !w_hdr;

            // Shadow pass: bind shadow RT
            if (writes_shadow) {
                pipeline.addPassWithRT(pass, res.shadow.rt, true,
                                       1.0f, 1.0f, 1.0f, 1.0f,
                                       shadow_size, shadow_size);
                continue;
            }

            // Compute passes that don't touch FBOs (e.g., compute_particles)
            if (pass->queueType() == QueueType::Compute && !writes_hdr && !reads_hdr_post) {
                PipelineNode node;
                node.pass = pass;
                node.enabled = true;
                node.barrier = true;
                pipeline.addPass(pass);
                continue;
            }

            // First HDR writer: bind HDR RT with clear
            if (writes_hdr && !hdr_rt_bound) {
                hdr_rt_bound = true;
                pipeline.addPassWithRT(pass, res.t4.hdr_scene_rt, true,
                                       fogR, fogG, fogB, 1.0f,
                                       viewport_w, viewport_h);
                continue;
            }

            // Subsequent HDR writers (scene geometry passes)
            if (writes_hdr && hdr_rt_bound && !hdr_rt_unbound) {
                pipeline.addPass(pass);
                continue;
            }

            // Post-processing reader: unbind HDR RT first time
            if (reads_hdr_post && !hdr_rt_unbound) {
                hdr_rt_unbound = true;
                pipeline.addRTSwitch(INVALID_RENDER_TARGET);
            }

            // SceneToFBO manages its own FBO, just add it
            if (pass->passRole() == PassRole::SceneContainer) {
                pipeline.addPass(pass);
                continue;
            }

            // HDR composite: barrier + render to dest
            if (pass->passRole() == PassRole::FinalComposite) {
                pipeline.addBarrier(4);
                pipeline.addPassToDest(pass, false, 0.0f, 0.0f, 0.0f, 0.0f,
                                       viewport_w, viewport_h);
                continue;
            }

            // Compute post-proc passes get barrier flag
            if (pass->queueType() == QueueType::Compute) {
                pipeline.addPass(pass);
                // Barrier is handled by the pipeline node's compute sync
                continue;
            }

            // Default: just add the pass
            pipeline.addPass(pass);
        }

    } else if (has_scene_writer) {
        // ============================================================
        // T2/T3 scene FBO path
        // ============================================================
        for (size_t i = 0; i < sorted.size(); i++) {
            DemoRenderPass* pass = sorted[i];


            bool writes_shadow = false;
            const ResourceDecl* decls = pass->resourceDecls();
            int dc = pass->resourceDeclCount();
            for (int d = 0; d < dc; d++) {
                if (decls[d].id == ResourceId::ShadowMap && decls[d].access == ResourceDecl::WRITE)
                    writes_shadow = true;
            }

            if (writes_shadow) {
                pipeline.addPassWithRT(pass, res.shadow.rt, true,
                                       1.0f, 1.0f, 1.0f, 1.0f,
                                       shadow_size, shadow_size);
                continue;
            }

            // Composite: render to dest
            if (pass->passRole() == PassRole::FinalComposite) {
                pipeline.addPassToDest(pass, false, 0.0f, 0.0f, 0.0f, 0.0f,
                                       viewport_w, viewport_h);
                continue;
            }

            // Everything else (scene_to_fbo, ssao, bloom): just add
            pipeline.addPass(pass);
        }

    } else {
        // ============================================================
        // T1 default FB path (no FBO)
        // ============================================================
        bool fb_cleared = false;
        for (size_t i = 0; i < sorted.size(); i++) {
            DemoRenderPass* pass = sorted[i];

            bool writes_shadow = false;
            const ResourceDecl* decls = pass->resourceDecls();
            int dc = pass->resourceDeclCount();
            for (int d = 0; d < dc; d++) {
                if (decls[d].id == ResourceId::ShadowMap && decls[d].access == ResourceDecl::WRITE)
                    writes_shadow = true;
            }

            if (writes_shadow) {
                pipeline.addPassWithRT(pass, res.shadow.rt, true,
                                       1.0f, 1.0f, 1.0f, 1.0f,
                                       shadow_size, shadow_size);
                continue;
            }

            // Insert default FB clear before first geometry pass
            if (!fb_cleared) {
                fb_cleared = true;
                pipeline.addDefaultFBWithClear(fogR, fogG, fogB, 1.0f,
                                               viewport_w, viewport_h);
            }

            pipeline.addPass(pass);
        }
    }

    LOG_DBG("Pipeline: %d nodes (%d enabled) for tier %d, path=%s",
            static_cast<int>(pipeline.passCount()),
            static_cast<int>(pipeline.enabledCount()),
            tier_int,
            has_hdr_writer ? "HDR" : (has_scene_writer ? "SceneFBO" : "DefaultFB"));
}
