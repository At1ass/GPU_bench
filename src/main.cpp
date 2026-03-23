#include "app.h"
#include "platform/gpu_select.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Forward declaration (printHelp is generated from kArgs[], defined after it)
static void printHelp();

// Safe integer parsing with error detection (returns fallback on invalid input)
static int parseIntArg(const char* s, int fallback = 0) {
    char* end = nullptr;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        fprintf(stderr, "Warning: invalid integer '%s', using %d\n", s, fallback);
        return fallback;
    }
    return static_cast<int>(v);
}

// --- Enum value tables (defined once, used by both help and parsing) ---

struct NameVal {
    const char* name;
    int value;
};

static const NameVal kPresetVals[] = {
    {"light",   static_cast<int>(PresetIndex::Light)},
    {"medium",  static_cast<int>(PresetIndex::Medium)},
    {"heavy",   static_cast<int>(PresetIndex::Heavy)},
    {"ultra",   static_cast<int>(PresetIndex::Ultra)},
    {"extreme", static_cast<int>(PresetIndex::Extreme)}
};

static const NameVal kRendererVals[] = {
    {"gl2",  static_cast<int>(RendererBackend::GL2)},
    {"gl3",  static_cast<int>(RendererBackend::GL3)},
    {"gl4",  static_cast<int>(RendererBackend::GL4)},
    {"gles", static_cast<int>(RendererBackend::GLES)},
    {"auto", static_cast<int>(RendererBackend::Auto)}
};

static const NameVal kOutputVals[] = {
    {"text", static_cast<int>(OutputFormat::Text)},
    {"csv",  static_cast<int>(OutputFormat::CSV)},
    {"json", static_cast<int>(OutputFormat::JSON)}
};

static const NameVal kTimingVals[] = {
    {"sync", static_cast<int>(TimingMode::Sync)},
    {"gpu",  static_cast<int>(TimingMode::GPU)}
};

// --- Enum matching helper ---

template<typename T>
static int matchEnumArg(const char* arg, const NameVal* vals, int count,
                        T* out, const char* label) {
    for (int i = 0; i < count; i++) {
        if (strcmp(arg, vals[i].name) == 0) {
            *out = static_cast<T>(vals[i].value);
            return 0;
        }
    }
    fprintf(stderr, "Unknown %s: %s\n", label, arg);
    return 1;
}

// --- Argument table definition ---

using ParseFn = int (*)(const char* arg, AppConfig& cfg);

struct ArgDef {
    const char*    flag;
    const char*    alias;
    bool           takes_arg;
    const char*    help;
    const char*    meta;       // "<path>", "<n>", nullptr (auto from vals)
    const NameVal* vals;
    int            val_count;
    const char*    def_text;
    ParseFn        parse;
};

#define NVALS(a) (static_cast<int>(sizeof(a) / sizeof(a[0])))

static const ArgDef kArgs[] = {
    {"--preset", nullptr, true, "Preset", nullptr,
     kPresetVals, NVALS(kPresetVals), "medium",
     +[](const char* a, AppConfig& c) -> int {
         return matchEnumArg(a, kPresetVals, NVALS(kPresetVals),
                             &c.preset_index, "preset");
     }},

    {"--renderer", nullptr, true, "Renderer", nullptr,
     kRendererVals, NVALS(kRendererVals), "auto",
     +[](const char* a, AppConfig& c) -> int {
         return matchEnumArg(a, kRendererVals, NVALS(kRendererVals),
                             &c.backend, "renderer");
     }},

    {"--config", nullptr, true, "Load config INI", "<path>",
     nullptr, 0, nullptr,
     +[](const char* a, AppConfig& c) -> int {
         c.config_path = a; return 0;
     }},

    {"--headless", nullptr, false, "No GUI, run tests, print results", nullptr,
     nullptr, 0, nullptr,
     +[](const char*, AppConfig& c) -> int {
         c.headless = true; return 0;
     }},

    {"--test", nullptr, true,
     "Run specific tests (comma-separated, or \"all\")", "<name,...>",
     nullptr, 0, nullptr,
     +[](const char* a, AppConfig& c) -> int {
         c.test_filter = a; return 0;
     }},

    {"--output", nullptr, true, "Output format", nullptr,
     kOutputVals, NVALS(kOutputVals), "text",
     +[](const char* a, AppConfig& c) -> int {
         return matchEnumArg(a, kOutputVals, NVALS(kOutputVals),
                             &c.output_format, "output format");
     }},

    {"--output-file", nullptr, true, "Write results to file", "<path>",
     nullptr, 0, nullptr,
     +[](const char* a, AppConfig& c) -> int {
         c.output_file = a; return 0;
     }},

    {"--width", nullptr, true, "Viewport width", "<n>",
     nullptr, 0, "800",
     +[](const char* a, AppConfig& c) -> int {
         c.width = parseIntArg(a, 800); return 0;
     }},

    {"--height", nullptr, true, "Viewport height", "<n>",
     nullptr, 0, "600",
     +[](const char* a, AppConfig& c) -> int {
         c.height = parseIntArg(a, 600); return 0;
     }},

    {"--timing", nullptr, true, "Timing mode", nullptr,
     kTimingVals, NVALS(kTimingVals), "sync",
     +[](const char* a, AppConfig& c) -> int {
         return matchEnumArg(a, kTimingVals, NVALS(kTimingVals),
                             &c.timing_mode, "timing mode");
     }},

    {"--render-res", nullptr, true,
     "Render resolution (e.g. 1920x1080)", "<WxH>",
     nullptr, 0, "native",
     +[](const char* a, AppConfig& c) -> int {
         int rw = 0, rh = 0;
         if (sscanf(a, "%dx%d", &rw, &rh) == 2 && rw >= 64 && rh >= 64) {
             c.render_width = rw;
             c.render_height = rh;
             return 0;
         }
         fprintf(stderr, "Invalid render resolution: %s (expected WxH, e.g. 1920x1080)\n", a);
         return 1;
     }},

    {"--stress", nullptr, true, "Stress test mode (headless only)", "<seconds>",
     nullptr, 0, nullptr,
     +[](const char* a, AppConfig& c) -> int {
         c.stress_duration_sec = parseIntArg(a, 60);
         c.headless = true;
         return 0;
     }},

    {"--demo", nullptr, false, "Visual demo benchmark (3DMark-style)", nullptr,
     nullptr, 0, nullptr,
     +[](const char*, AppConfig& c) -> int {
         c.demo_mode = true; return 0;
     }},

    {"--demo-tier", nullptr, true, "Run specific demo tier only", "<1-4>",
     nullptr, 0, nullptr,
     +[](const char* a, AppConfig& c) -> int {
         c.demo_tier = parseIntArg(a, 0);
         if (c.demo_tier < 1 || c.demo_tier > 4) {
             fprintf(stderr, "Invalid demo tier: %d (must be 1-4)\n", c.demo_tier);
             return 1;
         }
         return 0;
     }},

    {"--demo-duration", nullptr, true, "Duration per tier", "<seconds>",
     nullptr, 0, "15",
     +[](const char* a, AppConfig& c) -> int {
         c.demo_duration = parseIntArg(a, 15);
         if (c.demo_duration < 1) c.demo_duration = 1;
         return 0;
     }},

    {"--gpu", nullptr, true, "Select GPU by index (see --list-gpus)", "<index>",
     nullptr, 0, nullptr,
     +[](const char* a, AppConfig& c) -> int {
         c.gpu_index = parseIntArg(a, -1); return 0;
     }},

    {"--debug", nullptr, false, "Enable debug logging", nullptr,
     nullptr, 0, nullptr,
     +[](const char*, AppConfig& c) -> int {
         c.debug = true; return 0;
     }},

    {"--list-gpus", nullptr, false, "List available GPUs and exit", nullptr,
     nullptr, 0, nullptr,
     +[](const char*, AppConfig&) -> int {
         auto gpus = enumerateGPUs();
         printGPUList(gpus);
         return -1;
     }},

    {"--help", "-h", false, "Show this help", nullptr,
     nullptr, 0, nullptr,
     +[](const char*, AppConfig&) -> int {
         printHelp();
         return -1;
     }},
};

#undef NVALS

static const int kNumArgs = static_cast<int>(sizeof(kArgs) / sizeof(kArgs[0]));

// --- Help text generation from kArgs[] ---

static void printHelp() {
    fprintf(stderr, "gpu_benchmark [options]\n");
    for (int i = 0; i < kNumArgs; i++) {
        const ArgDef& a = kArgs[i];

        // Build left column: "  --flag <meta>"
        char left[64];
        if (a.takes_arg) {
            if (a.meta) {
                snprintf(left, sizeof(left), "  %s %s", a.flag, a.meta);
            } else if (a.vals && a.val_count > 0) {
                // Auto-generate <val1|val2|...>
                char vals_str[128];
                int pos = 0;
                vals_str[pos++] = '<';
                for (int v = 0; v < a.val_count; v++) {
                    if (v > 0) vals_str[pos++] = '|';
                    const char* n = a.vals[v].name;
                    while (*n && pos < 126) vals_str[pos++] = *n++;
                }
                vals_str[pos++] = '>';
                vals_str[pos] = '\0';
                snprintf(left, sizeof(left), "  %s %s", a.flag, vals_str);
            } else {
                snprintf(left, sizeof(left), "  %s", a.flag);
            }
        } else {
            snprintf(left, sizeof(left), "  %s", a.flag);
        }

        // Right column: description + default
        // Ensure at least one space between columns
        int left_len = static_cast<int>(strlen(left));
        int pad = (left_len >= 44) ? 1 : 44 - left_len;
        if (a.def_text) {
            fprintf(stderr, "%s%*s%s (default: %s)\n", left, pad, "", a.help, a.def_text);
        } else {
            fprintf(stderr, "%s%*s%s\n", left, pad, "", a.help);
        }
    }
}

// --- Argument parsing loop ---

// Parse command line arguments. Returns 0 on success, 1 on error, -1 on early exit (--help, --list-gpus).
static int parseArgs(int argc, char* argv[], AppConfig& cfg) {
    for (int i = 1; i < argc; i++) {
        const ArgDef* found = nullptr;
        for (int j = 0; j < kNumArgs; j++) {
            if (strcmp(argv[i], kArgs[j].flag) == 0 ||
                (kArgs[j].alias && strcmp(argv[i], kArgs[j].alias) == 0)) {
                found = &kArgs[j];
                break;
            }
        }

        if (!found) {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            printHelp();
            return 1;
        }

        const char* value = nullptr;
        if (found->takes_arg) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s requires an argument\n", argv[i]);
                return 1;
            }
            value = argv[++i];
        }

        int rc = found->parse(value, cfg);
        if (rc != 0) return rc;
    }

    if (cfg.width < 64) cfg.width = 64;
    if (cfg.height < 64) cfg.height = 64;

    return 0;
}

int main(int argc, char* argv[]) {
    AppConfig cfg;

    int rc = parseArgs(argc, argv, cfg);
    if (rc != 0) return (rc < 0) ? 0 : rc;

    // GPU selection must happen before SDL_Init / GL context creation.
    if (cfg.gpu_index >= 0)
        selectGPUAndReexec(cfg.gpu_index, argc, argv);

    App app;  // ~App() calls shutdown() automatically
    if (!app.init(cfg)) {
        fprintf(stderr, "Failed to initialize GPU_benchmark\n");
        return 1;
    }

    app.run();
    return 0;
}
