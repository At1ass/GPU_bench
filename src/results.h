#pragma once
#include "bench.h"
#include "renderer.h"
#include "hwinfo.h"
#include <vector>
#include <cstdio>

struct ExportConfig {
    int width = 0;
    int height = 0;
    int warmup_frames = 0;
    int measure_frames = 0;
    bool vsync = false;
    const char* gpu_tier = "unknown";  // "legacy", "low", "mid", "high"

    ExportConfig() = default;
};

void exportText(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name,
                const ExportConfig& ecfg,
                const CompositeScore* composite = nullptr,
                const BottleneckInfo* bottleneck = nullptr);

void exportCSV(FILE* out, const std::vector<BenchResult>& results,
               const HWInfo& hw, const RenderCaps& caps,
               const char* gpu_name, const char* gl_version,
               const char* renderer_name, const char* preset_name,
               const ExportConfig& ecfg);

void exportJSON(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name,
                const ExportConfig& ecfg,
                const CompositeScore* composite = nullptr,
                const BottleneckInfo* bottleneck = nullptr);
