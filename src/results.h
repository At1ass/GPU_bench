#pragma once
#include "bench.h"
#include "renderer.h"
#include "hwinfo.h"
#include <vector>
#include <cstdio>

void exportText(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name);

void exportCSV(FILE* out, const std::vector<BenchResult>& results,
               const HWInfo& hw, const RenderCaps& caps,
               const char* gpu_name, const char* gl_version,
               const char* renderer_name, const char* preset_name);

void exportJSON(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name);
