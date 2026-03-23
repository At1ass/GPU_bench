#pragma once
#include "demo/render_pass.h"
#include "platform/logger.h"
#include <vector>
#include <cstring>

// Ordered list of render passes with runtime enable/disable.
// Tier configuration = adding passes and toggling enabled flags.
class DemoPipeline {
public:
    void addPass(DemoRenderPass* pass, bool enabled = true) {
        passes_.push_back({ pass, enabled });
    }

    void setEnabled(const char* name, bool on) {
        for (size_t i = 0; i < passes_.size(); i++) {
            if (strcmp(passes_[i].pass->name(), name) == 0) {
                passes_[i].enabled = on;
                return;
            }
        }
    }

    void clear() { passes_.clear(); }

    void execute(Renderer* r, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) {
        for (size_t i = 0; i < passes_.size(); i++) {
            if (!passes_[i].enabled) continue;
            passes_[i].pass->execute(r, fd, res, cfg, scene);
        }
    }

    size_t passCount() const { return passes_.size(); }
    size_t enabledCount() const {
        size_t n = 0;
        for (size_t i = 0; i < passes_.size(); i++)
            if (passes_[i].enabled) n++;
        return n;
    }

private:
    struct Entry {
        DemoRenderPass* pass;
        bool enabled;
    };
    std::vector<Entry> passes_;
};
