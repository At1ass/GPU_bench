#include "bench/results.h"
#include "tests/test_registry.h"
#include "platform/compat.h"
#include "platform/logger.h"

void exportText(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                uint32_t available_caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name,
                const ExportConfig& ecfg,
                const CompositeScore* composite,
                const BottleneckInfo* bottleneck) {
    fprintf(out, "=== GPU Benchmark Results ===\n");
    fprintf(out, "Preset: %s\n", preset_name);
    fprintf(out, "CPU: %s\n", hw.cpu_name.c_str());
    fprintf(out, "GPU: %s\n", gpu_name);
    fprintf(out, "GL Version: %s\n", gl_version);
    fprintf(out, "Renderer: %s\n", renderer_name);
    fprintf(out, "OS: %s %s\n", hw.os_name.c_str(), hw.os_version.c_str());
    if (caps.estimated_vram_mb > 0)
        fprintf(out, "VRAM: %d MB\n", caps.estimated_vram_mb);
    fprintf(out, "MaxTex: %d  Attribs: %d  VAO: %s  Instancing: %s  FBO: %s  TimerQ: %s\n",
            caps.max_texture_size, caps.max_vertex_attribs,
            (available_caps & Cap_VAO) ? "yes" : "no",
            (available_caps & Cap_Instancing) ? "yes" : "no",
            (available_caps & Cap_FBO) ? "yes" : "no",
            (available_caps & Cap_TimerQuery) ? "yes" : "no");
    fprintf(out, "GPU Tier: %s\n", ecfg.gpu_tier);
    fprintf(out, "Resolution: %dx%d\n", ecfg.width, ecfg.height);
    fprintf(out, "Warmup: %d frames, Measure: %d frames\n", ecfg.warmup_frames, ecfg.measure_frames);
    fprintf(out, "\n");

    if (composite && composite->overall > 0) {
        fprintf(out, "Composite Score: %.1f\n", composite->overall);
        fprintf(out, "  Fill: %.1f  Geometry: %.1f  Compute: %.1f  Overhead: %.1f\n",
                composite->fill, composite->geometry, composite->compute, composite->overhead);
        fprintf(out, "\n");
    }

    fprintf(out, "%-12s %10s %-10s %10s %10s %10s %10s %10s %10s %6s %6s\n",
            "Test", "Score", "Unit", "Avg(ms)", "Min(ms)", "Max(ms)", "P1(ms)", "Median(ms)", "P99(ms)", "Frames", "CV%");
    fprintf(out, "------------ ---------- ---------- ---------- ---------- ---------- ---------- ---------- ---------- ------ ------\n");

    for (const auto& r : results) {
        fprintf(out, "%-12s %10.1f %-10s %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f %6d %5.1f%%",
                r.name.c_str(), r.score, r.unit.c_str(),
                r.avg_ms, r.min_ms, r.max_ms, r.p1_ms, r.median_ms, r.p99_ms, r.frames, r.cv * 100.0);
        if (!r.sanity_ok) fprintf(out, " [SANITY FAIL]");
        else if (!r.valid) fprintf(out, " [UNSTABLE]");
        fprintf(out, "\n");
    }

    if (bottleneck && !bottleneck->detail.empty()) {
        fprintf(out, "\nBottleneck: %s\n", bottleneck->detail.c_str());
    }
}

// RFC 4180 CSV field escaping: wrap in quotes, double any embedded quotes
static void csvField(FILE* out, const char* s) {
    if (!s) { fputc(',', out); return; }
    fputc('"', out);
    while (*s) {
        if (*s == '"') fputc('"', out);
        fputc(*s, out);
        s++;
    }
    fputc('"', out);
}

void exportCSV(FILE* out, const std::vector<BenchResult>& results,
               const HWInfo& hw, const RenderCaps& caps,
               uint32_t available_caps,
               const char* gpu_name, const char* gl_version,
               const char* renderer_name, const char* preset_name,
               const ExportConfig& ecfg) {
    (void)available_caps;
    (void)ecfg;
    fprintf(out, "preset,cpu,gpu,gl_version,renderer,os,vram_mb,test,score,unit,avg_ms,min_ms,max_ms,median_ms,p1_ms,p99_ms,cv,frames,valid,sanity_ok\n");
    for (const auto& r : results) {
        csvField(out, preset_name); fputc(',', out);
        csvField(out, hw.cpu_name.c_str()); fputc(',', out);
        csvField(out, gpu_name); fputc(',', out);
        csvField(out, gl_version); fputc(',', out);
        csvField(out, renderer_name); fputc(',', out);
        std::string os = hw.os_name + " " + hw.os_version;
        csvField(out, os.c_str()); fputc(',', out);
        fprintf(out, "%d,", caps.estimated_vram_mb);
        csvField(out, r.name.c_str()); fputc(',', out);
        fprintf(out, "%.2f,", r.score);
        csvField(out, r.unit.c_str()); fputc(',', out);
        fprintf(out, "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%d,%s,%s\n",
                r.avg_ms, r.min_ms, r.max_ms, r.median_ms, r.p1_ms, r.p99_ms,
                r.cv, r.frames, r.valid ? "true" : "false",
                r.sanity_ok ? "true" : "false");
    }
}

static void jsonEscape(FILE* out, const char* s) {
    if (!s) return;
    while (*s) {
        switch (*s) {
        case '"':  fprintf(out, "\\\""); break;
        case '\\': fprintf(out, "\\\\"); break;
        case '\n': fprintf(out, "\\n"); break;
        case '\r': fprintf(out, "\\r"); break;
        case '\t': fprintf(out, "\\t"); break;
        case '\b': fprintf(out, "\\b"); break;
        case '\f': fprintf(out, "\\f"); break;
        default:
            if (static_cast<unsigned char>(*s) < 0x20)
                fprintf(out, "\\u%04x", static_cast<unsigned char>(*s));
            else
                fputc(*s, out);
            break;
        }
        s++;
    }
}

void exportJSON(FILE* out, const std::vector<BenchResult>& results,
                const HWInfo& hw, const RenderCaps& caps,
                uint32_t available_caps,
                const char* gpu_name, const char* gl_version,
                const char* renderer_name, const char* preset_name,
                const ExportConfig& ecfg,
                const CompositeScore* composite,
                const BottleneckInfo* bottleneck) {
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
    fprintf(out, "  \"gpu_caps\": {\n");
    fprintf(out, "    \"gl_major\": %d,\n", caps.gl_major);
    fprintf(out, "    \"gl_minor\": %d,\n", caps.gl_minor);
    fprintf(out, "    \"max_texture_size\": %d,\n", caps.max_texture_size);
    fprintf(out, "    \"max_vertex_attribs\": %d,\n", caps.max_vertex_attribs);
    fprintf(out, "    \"has_vao\": %s,\n", (available_caps & Cap_VAO) ? "true" : "false");
    fprintf(out, "    \"has_instancing\": %s,\n", (available_caps & Cap_Instancing) ? "true" : "false");
    fprintf(out, "    \"has_fbo\": %s,\n", (available_caps & Cap_FBO) ? "true" : "false");
    fprintf(out, "    \"has_timer_queries\": %s,\n", (available_caps & Cap_TimerQuery) ? "true" : "false");
    fprintf(out, "    \"has_generate_mipmap\": %s,\n", caps.has_generate_mipmap_func ? "true" : "false");
    fprintf(out, "    \"vram_mb\": %d,\n", caps.estimated_vram_mb);
    fprintf(out, "    \"gpu_tier\": \"%s\"\n", ecfg.gpu_tier);
    fprintf(out, "  },\n");
    fprintf(out, "  \"config\": {\n");
    fprintf(out, "    \"resolution_w\": %d,\n", ecfg.width);
    fprintf(out, "    \"resolution_h\": %d,\n", ecfg.height);
    fprintf(out, "    \"warmup_frames\": %d,\n", ecfg.warmup_frames);
    fprintf(out, "    \"measure_frames\": %d,\n", ecfg.measure_frames);
    fprintf(out, "    \"vsync\": %s\n", ecfg.vsync ? "true" : "false");
    fprintf(out, "  },\n");

    if (composite && composite->overall > 0) {
        fprintf(out, "  \"composite\": {\n");
        fprintf(out, "    \"overall\": %.2f,\n", composite->overall);
        fprintf(out, "    \"fill\": %.2f,\n", composite->fill);
        fprintf(out, "    \"geometry\": %.2f,\n", composite->geometry);
        fprintf(out, "    \"compute\": %.2f,\n", composite->compute);
        fprintf(out, "    \"overhead\": %.2f\n", composite->overhead);
        fprintf(out, "  },\n");
    }

    if (bottleneck && !bottleneck->detail.empty()) {
        fprintf(out, "  \"bottleneck\": {\n");
        fprintf(out, "    \"category\": \""); jsonEscape(out, bottleneck->weakest_category.c_str()); fprintf(out, "\",\n");
        fprintf(out, "    \"weakness_ratio\": %.3f,\n", bottleneck->weakness_ratio);
        fprintf(out, "    \"detail\": \""); jsonEscape(out, bottleneck->detail.c_str()); fprintf(out, "\"\n");
        fprintf(out, "  },\n");
    }

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
        fprintf(out, "      \"cv\": %.4f,\n", r.cv);
        fprintf(out, "      \"p99_median_ratio\": %.4f,\n", r.p99_median_ratio);
        fprintf(out, "      \"gpu_ms\": %.3f,\n", r.gpu_ms);
        fprintf(out, "      \"frames\": %d,\n", r.frames);
        fprintf(out, "      \"valid\": %s,\n", r.valid ? "true" : "false");
        fprintf(out, "      \"sanity_ok\": %s\n", r.sanity_ok ? "true" : "false");
        fprintf(out, "    }%s\n", (i + 1 < results.size()) ? "," : "");
    }

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
}

bool writeBenchResults(OutputFormat fmt, const char* output_file,
                       const std::vector<BenchResult>& results,
                       const HWInfo& hw, const RenderCaps& caps,
                       uint32_t available_caps,
                       const char* gpu_name, const char* gl_version,
                       const char* renderer_name, const char* preset_name,
                       const ExportConfig& ecfg,
                       const CompositeScore* composite,
                       const BottleneckInfo* bottleneck) {
    FileGuard fg;
    FILE* out = stdout;
    if (output_file && output_file[0]) {
        fg.reset(fopen(output_file, "w"));
        if (!fg) return false;
        out = fg.get();
    }

    switch (fmt) {
        case OutputFormat::CSV:
            exportCSV(out, results, hw, caps, available_caps, gpu_name, gl_version, renderer_name, preset_name, ecfg);
            break;
        case OutputFormat::JSON:
            exportJSON(out, results, hw, caps, available_caps, gpu_name, gl_version, renderer_name, preset_name, ecfg,
                       composite, bottleneck);
            break;
        default:
            exportText(out, results, hw, caps, available_caps, gpu_name, gl_version, renderer_name, preset_name, ecfg,
                       composite, bottleneck);
            break;
    }

    const char* fmt_name = (fmt == OutputFormat::CSV) ? "CSV" : (fmt == OutputFormat::JSON) ? "JSON" : "Text";
    if (output_file && output_file[0])
        LOG_DBG("Export: wrote %s results to '%s'", fmt_name, output_file);
    else
        LOG_DBG("Export: wrote %s results to stdout", fmt_name);
    return true;
}
