#include "results.h"

void exportText(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name) {
    fprintf(out, "=== GPU Benchmark Results ===\n");
    fprintf(out, "Preset: %s\n", preset_name);
    fprintf(out, "CPU: %s\n", hw.cpu_name.c_str());
    fprintf(out, "GPU: %s\n", gpu_name);
    fprintf(out, "GL Version: %s\n", gl_version);
    fprintf(out, "Renderer: %s\n", renderer_name);
    fprintf(out, "OS: %s %s\n", hw.os_name.c_str(), hw.os_version.c_str());
    if (caps.estimated_vram_mb > 0)
        fprintf(out, "VRAM: %d MB\n", caps.estimated_vram_mb);
    fprintf(out, "\n");

    fprintf(out, "%-12s %10s %-10s %10s %10s %10s %10s %6s\n",
            "Test", "Score", "Unit", "Avg(ms)", "Min(ms)", "Max(ms)", "Median(ms)", "Frames");
    fprintf(out, "------------ ---------- ---------- ---------- ---------- ---------- ---------- ------\n");

    for (size_t i = 0; i < results.size(); i++) {
        const BenchResult& r = results[i];
        fprintf(out, "%-12s %10.1f %-10s %10.3f %10.3f %10.3f %10.3f %6d\n",
                r.name.c_str(), r.score, r.unit.c_str(),
                r.avg_ms, r.min_ms, r.max_ms, r.median_ms, r.frames);
    }
}

void exportCSV(FILE* out, const std::vector<BenchResult>& results,
               const HWInfo& hw, const RenderCaps& caps,
               const char* gpu_name, const char* gl_version,
               const char* renderer_name, const char* preset_name) {
    fprintf(out, "preset,cpu,gpu,gl_version,renderer,os,vram_mb,test,score,unit,avg_ms,min_ms,max_ms,median_ms,p1_ms,p99_ms,frames\n");
    for (size_t i = 0; i < results.size(); i++) {
        const BenchResult& r = results[i];
        fprintf(out, "%s,\"%s\",\"%s\",\"%s\",%s,\"%s %s\",%d,%s,%.2f,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",
                preset_name,
                hw.cpu_name.c_str(), gpu_name, gl_version, renderer_name,
                hw.os_name.c_str(), hw.os_version.c_str(),
                caps.estimated_vram_mb,
                r.name.c_str(), r.score, r.unit.c_str(),
                r.avg_ms, r.min_ms, r.max_ms, r.median_ms, r.p1_ms, r.p99_ms, r.frames);
    }
}

static void jsonEscape(FILE* out, const char* s) {
    while (*s) {
        if (*s == '"') fprintf(out, "\\\"");
        else if (*s == '\\') fprintf(out, "\\\\");
        else if (*s == '\n') fprintf(out, "\\n");
        else fputc(*s, out);
        s++;
    }
}

void exportJSON(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name) {
    fprintf(out, "{\n");
    fprintf(out, "  \"preset\": \"%s\",\n", preset_name);
    fprintf(out, "  \"system\": {\n");
    fprintf(out, "    \"cpu\": \""); jsonEscape(out, hw.cpu_name.c_str()); fprintf(out, "\",\n");
    fprintf(out, "    \"gpu\": \""); jsonEscape(out, gpu_name); fprintf(out, "\",\n");
    fprintf(out, "    \"gl_version\": \""); jsonEscape(out, gl_version); fprintf(out, "\",\n");
    fprintf(out, "    \"renderer\": \"%s\",\n", renderer_name);
    fprintf(out, "    \"os\": \""); jsonEscape(out, hw.os_name.c_str()); fprintf(out, " ");
    jsonEscape(out, hw.os_version.c_str()); fprintf(out, "\",\n");
    fprintf(out, "    \"vram_mb\": %d\n", caps.estimated_vram_mb);
    fprintf(out, "  },\n");
    fprintf(out, "  \"results\": [\n");

    for (size_t i = 0; i < results.size(); i++) {
        const BenchResult& r = results[i];
        fprintf(out, "    {\n");
        fprintf(out, "      \"test\": \"%s\",\n", r.name.c_str());
        fprintf(out, "      \"score\": %.2f,\n", r.score);
        fprintf(out, "      \"unit\": \"%s\",\n", r.unit.c_str());
        fprintf(out, "      \"avg_ms\": %.3f,\n", r.avg_ms);
        fprintf(out, "      \"min_ms\": %.3f,\n", r.min_ms);
        fprintf(out, "      \"max_ms\": %.3f,\n", r.max_ms);
        fprintf(out, "      \"median_ms\": %.3f,\n", r.median_ms);
        fprintf(out, "      \"p1_ms\": %.3f,\n", r.p1_ms);
        fprintf(out, "      \"p99_ms\": %.3f,\n", r.p99_ms);
        fprintf(out, "      \"frames\": %d\n", r.frames);
        fprintf(out, "    }%s\n", (i + 1 < results.size()) ? "," : "");
    }

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
}
