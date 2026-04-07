#include "demo/demo_export.h"
#include "platform/compat.h"
#include <string>

static const char* demoTierName(DemoTier tier) {
    switch (tier) {
        case DemoTier::Basic:    return "Basic";
        case DemoTier::Enhanced: return "Enhanced";
        case DemoTier::Quality:  return "Quality";
        case DemoTier::Ultra:    return "Ultra";
    }
    return "?";
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

void exportDemoText(FILE* out, const DemoResults& results,
                    const HWInfo& hw, const char* gpu_name,
                    const char* gl_version, const char* renderer_name) {
    fprintf(out, "=== GPU Benchmark — Demo Mode ===\n");
    fprintf(out, "CPU: %s\n", hw.cpu_name.c_str());
    fprintf(out, "GPU: %s\n", gpu_name);
    fprintf(out, "GL Version: %s\n", gl_version);
    fprintf(out, "Renderer: %s\n", renderer_name);
    if (results.render_w > 0 && results.render_h > 0)
        fprintf(out, "Render Resolution: %dx%d\n", results.render_w, results.render_h);
    fprintf(out, "OS: %s %s\n", hw.os_name.c_str(), hw.os_version.c_str());
    fprintf(out, "\n");
    fprintf(out, "Demo Score: %.0f\n\n", results.demo_score);

    fprintf(out, "%-10s %8s %8s %8s %8s %8s %8s %6s\n",
            "Tier", "AvgFPS", "MinFPS", "P1%FPS", "P99%FPS", "AvgMs", "Score", "Frames");
    fprintf(out, "---------- -------- -------- -------- -------- -------- -------- ------\n");

    for (const auto& t : results.tiers) {
        fprintf(out, "T%d %-7s %8.1f %8.1f %8.1f %8.1f %8.2f %8.2f %6d\n",
                static_cast<int>(t.tier), demoTierName(t.tier),
                t.avg_fps, t.min_fps, t.p1_fps, t.p99_fps,
                t.avg_ms, t.normalized_score, t.frames);
    }
}

void exportDemoCSV(FILE* out, const DemoResults& results,
                   const HWInfo& hw, const char* gpu_name,
                   const char* gl_version, const char* renderer_name) {
    fprintf(out, "mode,cpu,gpu,gl_version,renderer,os,demo_score,tier,tier_name,avg_fps,min_fps,p1_fps,p99_fps,avg_ms,target_fps,normalized_score,frames\n");
    for (const auto& t : results.tiers) {
        fprintf(out, "demo,");
        csvField(out, hw.cpu_name.c_str()); fputc(',', out);
        csvField(out, gpu_name); fputc(',', out);
        csvField(out, gl_version); fputc(',', out);
        csvField(out, renderer_name); fputc(',', out);
        std::string os = std::string(hw.os_name) + " " + hw.os_version;
        csvField(out, os.c_str());
        fprintf(out, ",%.0f,%d,%s,%.2f,%.2f,%.2f,%.2f,%.3f,%.0f,%.4f,%d\n",
                results.demo_score,
                static_cast<int>(t.tier), demoTierName(t.tier),
                t.avg_fps, t.min_fps, t.p1_fps, t.p99_fps,
                t.avg_ms, t.target_fps, t.normalized_score, t.frames);
    }
}

void exportDemoJSON(FILE* out, const DemoResults& results,
                    const HWInfo& hw, const char* gpu_name,
                    const char* gl_version, const char* renderer_name) {
    fprintf(out, "{\n");
    fprintf(out, "  \"mode\": \"demo\",\n");
    fprintf(out, "  \"system\": {\n");
    fprintf(out, "    \"cpu\": \""); jsonEscape(out, hw.cpu_name.c_str()); fprintf(out, "\",\n");
    fprintf(out, "    \"gpu\": \""); jsonEscape(out, gpu_name); fprintf(out, "\",\n");
    fprintf(out, "    \"gl_version\": \""); jsonEscape(out, gl_version); fprintf(out, "\",\n");
    fprintf(out, "    \"renderer\": \"%s\",\n", renderer_name);
    fprintf(out, "    \"os\": \""); jsonEscape(out, hw.os_name.c_str()); fprintf(out, " ");
    jsonEscape(out, hw.os_version.c_str()); fprintf(out, "\"\n");
    fprintf(out, "  },\n");
    if (results.render_w > 0 && results.render_h > 0)
        fprintf(out, "  \"render_resolution\": \"%dx%d\",\n", results.render_w, results.render_h);
    fprintf(out, "  \"demo_score\": %.0f,\n", results.demo_score);
    fprintf(out, "  \"tiers\": [\n");

    for (size_t i = 0; i < results.tiers.size(); i++) {
        const auto& t = results.tiers[i];
        fprintf(out, "    {\n");
        fprintf(out, "      \"tier\": %d,\n", static_cast<int>(t.tier));
        fprintf(out, "      \"name\": \"%s\",\n", demoTierName(t.tier));
        fprintf(out, "      \"avg_fps\": %.2f,\n", t.avg_fps);
        fprintf(out, "      \"min_fps\": %.2f,\n", t.min_fps);
        fprintf(out, "      \"p1_fps\": %.2f,\n", t.p1_fps);
        fprintf(out, "      \"p99_fps\": %.2f,\n", t.p99_fps);
        fprintf(out, "      \"avg_ms\": %.3f,\n", t.avg_ms);
        fprintf(out, "      \"target_fps\": %.0f,\n", t.target_fps);
        fprintf(out, "      \"normalized_score\": %.4f,\n", t.normalized_score);
        fprintf(out, "      \"frames\": %d\n", t.frames);
        fprintf(out, "    }%s\n", (i + 1 < results.tiers.size()) ? "," : "");
    }

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
}

bool writeDemoResults(OutputFormat fmt, const char* output_file,
                      const DemoResults& results, const HWInfo& hw,
                      const char* gpu_name, const char* gl_version,
                      const char* renderer_name) {
    FileGuard fg;
    FILE* out = stdout;
    if (output_file && output_file[0]) {
        fg.reset(fopen(output_file, "w"));
        if (!fg) return false;
        out = fg.get();
    }

    switch (fmt) {
        case OutputFormat::JSON:
            exportDemoJSON(out, results, hw, gpu_name, gl_version, renderer_name);
            break;
        case OutputFormat::CSV:
            exportDemoCSV(out, results, hw, gpu_name, gl_version, renderer_name);
            break;
        default:
            exportDemoText(out, results, hw, gpu_name, gl_version, renderer_name);
            break;
    }
    return true;
}
