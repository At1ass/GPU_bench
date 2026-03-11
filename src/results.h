#pragma once
#include "bench.h"
#include "renderer.h"
#include "hwinfo.h"
#include <vector>
#include <cstdio>

struct ExportConfig {
    int width;
    int height;
    int warmup_frames;
    int measure_frames;
    bool vsync;
    const char* gpu_tier;  // "legacy", "low", "mid", "high"

    ExportConfig() : width(0), height(0), warmup_frames(0), measure_frames(0),
                     vsync(false), gpu_tier("unknown") {}
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
