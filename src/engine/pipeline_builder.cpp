#include "engine/pipeline_builder.h"
#include "engine/render_pass.h"
#include "engine/resource_id.h"
#include "platform/logger.h"
#include <vector>
#include <cstring>

// ============================================================
// Engine-level topological-sort pipeline builder
// ============================================================
//
// 1. Build adjacency graph from resource read/write declarations
// 2. Kahn's algorithm with executionOrder() tie-breaking
// 3. Emit PipelineNodes via policy-driven RT routing and barriers

static PipelineNode::RTAction mapAction(PassRTBinding::Action a) {
    switch (a) {
    case PassRTBinding::BindRT:   return PipelineNode::RTAction::Bind;
    case PassRTBinding::BindDest: return PipelineNode::RTAction::BindDest;
    case PassRTBinding::Unbind:   return PipelineNode::RTAction::Unbind;
    default:                      return PipelineNode::RTAction::None;
    }
}

void buildPipeline(RenderPipeline& out,
                   const std::vector<RenderPassBase*>& passes,
                   const PipelinePolicy& policy,
                   int viewport_w, int viewport_h) {
    out.clear();

    int n = static_cast<int>(passes.size());
    if (n == 0) return;

    // ----------------------------------------------------------
    // Step 1: build adjacency from resource declarations
    // ----------------------------------------------------------
    static const size_t RES_COUNT = static_cast<size_t>(ResourceId::COUNT);

    // For each ResourceId, track which pass indices write/read it
    std::vector<size_t> writers_buf[RES_COUNT];
    std::vector<size_t> readers_buf[RES_COUNT];

    for (size_t i = 0; i < static_cast<size_t>(n); i++) {
        const ResourceDecl* decls = passes[i]->resourceDecls();
        int dc = passes[i]->resourceDeclCount();
        for (int d = 0; d < dc; d++) {
            size_t rid = static_cast<size_t>(decls[d].id);
            if (decls[d].access == ResourceDecl::WRITE)
                writers_buf[rid].push_back(i);
            else
                readers_buf[rid].push_back(i);
        }
    }

    // Adjacency: edge[i] = set of passes that must run after pass i
    // Use a flat bool matrix to deduplicate edges
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
    // Step 2: Kahn's algorithm with executionOrder() tie-breaking
    // ----------------------------------------------------------
    std::vector<size_t> queue;
    queue.reserve(static_cast<size_t>(n));
    for (size_t i = 0; i < static_cast<size_t>(n); i++)
        if (in_degree[i] == 0) queue.push_back(i);

    std::vector<RenderPassBase*> sorted;
    sorted.reserve(static_cast<size_t>(n));

    while (!queue.empty()) {
        // Find the queue element with smallest executionOrder (tie-break)
        size_t best_idx = 0;
        int best_order = passes[queue[0]]->executionOrder();
        for (size_t qi = 1; qi < queue.size(); qi++) {
            int order = passes[queue[qi]]->executionOrder();
            if (order < best_order) {
                best_order = order;
                best_idx = qi;
            }
        }
        size_t u = queue[best_idx];
        queue.erase(queue.begin() + static_cast<ptrdiff_t>(best_idx));
        sorted.push_back(passes[u]);

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
                        passes[i]->name(), in_degree[i], passes[i]->executionOrder());
        }
    }

    // ----------------------------------------------------------
    // Step 3: emit pipeline nodes via policy
    // ----------------------------------------------------------
    int sorted_count = static_cast<int>(sorted.size());

    // Prologue (e.g., default FB clear)
    PassRTBinding prologue = policy.pipelinePrologue();
    if (prologue.action != PassRTBinding::None) {
        PipelineNode pn;
        pn.rt_action = mapAction(prologue.action);
        pn.bind_rt = prologue.rt;
        pn.clear_rt = prologue.clear;
        std::memcpy(pn.clear_color, prologue.clear_color, sizeof(float) * 4);
        pn.viewport_w = prologue.viewport_w;
        pn.viewport_h = prologue.viewport_h;
        pn.enabled = true;
        out.addNode(pn);
    }

    for (int i = 0; i < sorted_count; i++) {
        RenderPassBase* pass = sorted[static_cast<size_t>(i)];
        PassRTBinding rt = policy.routePass(*pass, i, sorted_count);
        PassSyncHint sync = policy.syncHint(*pass);

        PipelineNode node;
        node.pass = pass;
        node.command = nullptr;
        node.enabled = true;
        node.rt_action = mapAction(rt.action);
        node.bind_rt = rt.rt;
        node.clear_rt = rt.clear;
        std::memcpy(node.clear_color, rt.clear_color, sizeof(float) * 4);
        node.viewport_w = rt.viewport_w;
        node.viewport_h = rt.viewport_h;
        node.barrier = sync.barrier_after;
        node.unbind_image_count = sync.unbind_image_count;

        out.addNode(node);
    }
}
