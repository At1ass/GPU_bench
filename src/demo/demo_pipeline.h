#pragma once
#include "demo/render_pass.h"
#include "renderer/features.h"
#include "renderer/gl_debug.h"
#include "platform/logger.h"
#include <vector>
#include <cstring>

// Function pointer for inline commands (e.g., SSR texture copy)
typedef void (*PipelineCommandFn)(Renderer*, FrameData&,
                                   const TierResourceView&,
                                   const DemoTierConfig&,
                                   const SceneData&);

// Pipeline node: pass or command with optional RT management and barriers.
struct PipelineNode {
    DemoRenderPass* pass;
    PipelineCommandFn command;
    bool enabled;

    // Pre-execute: render target control
    enum RTAction : unsigned char {
        RT_NONE,       // don't change RT
        RT_BIND,       // bind specific RT
        RT_UNBIND,     // unbind to default framebuffer
        RT_BIND_DEST   // bind fd.dest_rt (resolved at execute time)
    };
    RTAction rt_action;
    RenderTargetHandle bind_rt;
    bool clear_rt;
    float clear_color[4];
    int viewport_w, viewport_h;  // 0 = use fd.viewport_w/h

    // Post-execute: compute sync
    bool barrier;
    int unbind_image_count;  // >0 = unbind image units 0..N-1

    PipelineNode()
        : pass(nullptr), command(nullptr), enabled(true)
        , rt_action(RT_NONE), bind_rt()
        , clear_rt(false), viewport_w(0), viewport_h(0)
        , barrier(false), unbind_image_count(0) {
        clear_color[0] = clear_color[1] = clear_color[2] = 0.0f;
        clear_color[3] = 1.0f;
    }
};

// Render pipeline with per-node RT management, barriers, and commands.
// Tier configuration = building the right sequence of nodes in setup().
class DemoPipeline {
public:
    // Simple pass (no RT change)
    void addPass(DemoRenderPass* pass, bool enabled = true) {
        PipelineNode n;
        n.pass = pass;
        n.enabled = enabled;
        nodes_.push_back(n);
    }

    // Pass with render target bind + optional clear before execution
    void addPassWithRT(DemoRenderPass* pass, RenderTargetHandle rt,
                       bool clear, float cr, float cg, float cb, float ca,
                       int vp_w = 0, int vp_h = 0) {
        PipelineNode n;
        n.pass = pass;
        n.enabled = true;
        n.rt_action = (rt == INVALID_RENDER_TARGET) ? PipelineNode::RT_UNBIND : PipelineNode::RT_BIND;
        n.bind_rt = rt;
        n.clear_rt = clear;
        n.clear_color[0] = cr; n.clear_color[1] = cg;
        n.clear_color[2] = cb; n.clear_color[3] = ca;
        n.viewport_w = vp_w;
        n.viewport_h = vp_h;
        nodes_.push_back(n);
    }

    // Pass that binds fd.dest_rt (resolved at execute time)
    void addPassToDest(DemoRenderPass* pass, bool clear,
                       float cr, float cg, float cb, float ca,
                       int vp_w = 0, int vp_h = 0) {
        PipelineNode n;
        n.pass = pass;
        n.enabled = true;
        n.rt_action = PipelineNode::RT_BIND_DEST;
        n.clear_rt = clear;
        n.clear_color[0] = cr; n.clear_color[1] = cg;
        n.clear_color[2] = cb; n.clear_color[3] = ca;
        n.viewport_w = vp_w;
        n.viewport_h = vp_h;
        nodes_.push_back(n);
    }

    // RT-only node (bind/unbind without executing a pass)
    void addRTSwitch(RenderTargetHandle rt) {
        PipelineNode n;
        n.enabled = true;
        n.rt_action = (rt == INVALID_RENDER_TARGET) ? PipelineNode::RT_UNBIND : PipelineNode::RT_BIND;
        n.bind_rt = rt;
        nodes_.push_back(n);
    }

    // Compute barrier + optional image unit unbind
    void addBarrier(int unbind_image_count = 0) {
        PipelineNode n;
        n.enabled = true;
        n.barrier = true;
        n.unbind_image_count = unbind_image_count;
        nodes_.push_back(n);
    }

    // Inline command (function pointer, for SSR texture copy etc.)
    void addCommand(PipelineCommandFn fn, bool enabled = true) {
        PipelineNode n;
        n.command = fn;
        n.enabled = enabled;
        nodes_.push_back(n);
    }

    // Unbind to default framebuffer with viewport and clear
    void addDefaultFBWithClear(float cr, float cg, float cb, float ca,
                               int vp_w, int vp_h) {
        PipelineNode n;
        n.enabled = true;
        n.rt_action = PipelineNode::RT_UNBIND;
        n.clear_rt = true;
        n.clear_color[0] = cr; n.clear_color[1] = cg;
        n.clear_color[2] = cb; n.clear_color[3] = ca;
        n.viewport_w = vp_w;
        n.viewport_h = vp_h;
        nodes_.push_back(n);
    }

    void clear() { nodes_.clear(); }

    void execute(Renderer* r, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) {
        for (size_t i = 0; i < nodes_.size(); i++) {
            const PipelineNode& n = nodes_[i];
            if (!n.enabled) continue;

            // Pre-execute: render target setup
            int vp_w = n.viewport_w > 0 ? n.viewport_w : fd.viewport_w;
            int vp_h = n.viewport_h > 0 ? n.viewport_h : fd.viewport_h;

            switch (n.rt_action) {
            case PipelineNode::RT_BIND:
                r->bindRenderTarget(n.bind_rt);
                r->setViewport(0, 0, vp_w, vp_h);
                if (n.clear_rt) {
                    r->clear(n.clear_color[0], n.clear_color[1],
                             n.clear_color[2], n.clear_color[3]);
                    r->setDepthTest(true);
                    r->setCullFace(true);
                }
                break;
            case PipelineNode::RT_UNBIND:
                r->bindRenderTarget(INVALID_RENDER_TARGET);
                if (n.viewport_w > 0 || n.viewport_h > 0)
                    r->setViewport(0, 0, vp_w, vp_h);
                if (n.clear_rt) {
                    r->clear(n.clear_color[0], n.clear_color[1],
                             n.clear_color[2], n.clear_color[3]);
                    r->setDepthTest(true);
                    r->setCullFace(true);
                }
                break;
            case PipelineNode::RT_BIND_DEST:
                r->bindRenderTarget(fd.dest_rt);
                r->setViewport(0, 0, vp_w, vp_h);
                if (n.clear_rt)
                    r->clear(n.clear_color[0], n.clear_color[1],
                             n.clear_color[2], n.clear_color[3]);
                break;
            case PipelineNode::RT_NONE:
                break;
            }

            // Execute pass or command (with GL debug groups for RenderDoc/Nsight)
            if (n.pass) {
                LOG_TRC("Pipeline: execute pass '%s' (node %d/%d, rt_action=%d)",
                        n.pass->name(), static_cast<int>(i + 1),
                        static_cast<int>(nodes_.size()), static_cast<int>(n.rt_action));
                GLDebug::pushGroup(n.pass->name());
                n.pass->execute(r, fd, res, cfg, scene);
                GLDebug::popGroup();
            } else if (n.command) {
                LOG_TRC("Pipeline: execute command (node %d/%d)",
                        static_cast<int>(i + 1), static_cast<int>(nodes_.size()));
                GLDebug::pushGroup("pipeline_command");
                n.command(r, fd, res, cfg, scene);
                GLDebug::popGroup();
            }

            // Post-execute: compute sync
            if (n.unbind_image_count > 0) {
                GL4Features* g4 = r->features<GL4Features>();
                if (g4) {
                    for (int iu = 0; iu < n.unbind_image_count; iu++)
                        g4->bindImageTexture(INVALID_TEXTURE, iu, false, false);
                }
            }
            if (n.barrier) {
                ComputeFeatures* cf = r->features<ComputeFeatures>();
                if (cf) cf->computeMemoryBarrier();
            }
        }
    }

    size_t passCount() const { return nodes_.size(); }
    size_t enabledCount() const {
        size_t n = 0;
        for (size_t i = 0; i < nodes_.size(); i++)
            if (nodes_[i].enabled) n++;
        return n;
    }

private:
    std::vector<PipelineNode> nodes_;
};
