#pragma once
#include "demo/demo_runner.h"
#include "platform/hwinfo.h"
#include <cstdio>

// Demo mode export functions
void exportDemoText(FILE* out, const DemoResults& results,
                    const HWInfo& hw, const char* gpu_name,
                    const char* gl_version, const char* renderer_name);

void exportDemoCSV(FILE* out, const DemoResults& results,
                   const HWInfo& hw, const char* gpu_name,
                   const char* gl_version, const char* renderer_name);

void exportDemoJSON(FILE* out, const DemoResults& results,
                    const HWInfo& hw, const char* gpu_name,
                    const char* gl_version, const char* renderer_name);

// Format-dispatching write: opens file (or stdout), calls exportDemoText/CSV/JSON, closes.
// Returns false on file open error.
enum class OutputFormat : int;
bool writeDemoResults(OutputFormat fmt, const char* output_file,
                      const DemoResults& results, const HWInfo& hw,
                      const char* gpu_name, const char* gl_version,
                      const char* renderer_name);
