#include "app.h"
#include "gpu_select.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static void printHelp() {
    fprintf(stderr,
        "gpu_benchmark [options]\n"
        "  --preset <light|medium|heavy|ultra>   Preset (default: medium)\n"
        "  --renderer <gl2|gl3|gl4|gles|auto>     Renderer (default: auto)\n"
        "  --config <path>                       Load config INI\n"
        "  --headless                            No GUI, run tests, print results\n"
        "  --test <name,...>                      Run specific tests (comma-separated, or \"all\")\n"
        "  --output <text|csv|json>              Output format (default: text)\n"
        "  --output-file <path>                  Write results to file\n"
        "  --width <n>                           Viewport width (default: 800)\n"
        "  --height <n>                          Viewport height (default: 600)\n"
        "  --timing <sync|gpu>                   Timing mode (default: sync)\n"
        "  --render-res <WxH>                    Render resolution (e.g. 1920x1080, default: native)\n"
        "  --stress <seconds>                    Stress test mode (headless only)\n"
        "  --gpu <index>                         Select GPU by index (see --list-gpus)\n"
        "  --list-gpus                           List available GPUs and exit\n"
        "  --help                                Show this help\n"
    );
}

int main(int argc, char* argv[]) {
    AppConfig cfg;
    cfg.width = 800;
    cfg.height = 600;
    cfg.preset_index = PRESET_MEDIUM;
    cfg.backend = RendererBackend::Auto;
    cfg.headless = false;
    cfg.output_format = OUTPUT_TEXT;
    cfg.timing_mode = TIMING_SYNC;
    cfg.output_file = "";
    cfg.config_path = "";
    cfg.test_filter = "all";
    cfg.stress_duration_sec = 0;
    cfg.render_width = 0;
    cfg.render_height = 0;

    int gpu_index = -1; // -1 = no selection (use default)

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printHelp();
            return 0;
        } else if (strcmp(argv[i], "--list-gpus") == 0) {
            std::vector<GPUDevice> gpus = enumerateGPUs();
            printGPUList(gpus);
            return 0;
        } else if (strcmp(argv[i], "--gpu") == 0 && i + 1 < argc) {
            gpu_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--preset") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "light") == 0)       cfg.preset_index = PRESET_LIGHT;
            else if (strcmp(argv[i], "medium") == 0)  cfg.preset_index = PRESET_MEDIUM;
            else if (strcmp(argv[i], "heavy") == 0)   cfg.preset_index = PRESET_HEAVY;
            else if (strcmp(argv[i], "ultra") == 0)   cfg.preset_index = PRESET_ULTRA;
            else { fprintf(stderr, "Unknown preset: %s\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--renderer") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "gl2") == 0)        cfg.backend = RendererBackend::GL2;
            else if (strcmp(argv[i], "gl3") == 0)   cfg.backend = RendererBackend::GL3;
            else if (strcmp(argv[i], "gl4") == 0)   cfg.backend = RendererBackend::GL4;
            else if (strcmp(argv[i], "gles") == 0)  cfg.backend = RendererBackend::GLES;
            else if (strcmp(argv[i], "auto") == 0)  cfg.backend = RendererBackend::Auto;
            else { fprintf(stderr, "Unknown renderer: %s\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            cfg.config_path = argv[++i];
        } else if (strcmp(argv[i], "--headless") == 0) {
            cfg.headless = true;
        } else if (strcmp(argv[i], "--test") == 0 && i + 1 < argc) {
            cfg.test_filter = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "text") == 0)       cfg.output_format = OUTPUT_TEXT;
            else if (strcmp(argv[i], "csv") == 0)   cfg.output_format = OUTPUT_CSV;
            else if (strcmp(argv[i], "json") == 0)  cfg.output_format = OUTPUT_JSON;
            else { fprintf(stderr, "Unknown output format: %s\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--output-file") == 0 && i + 1 < argc) {
            cfg.output_file = argv[++i];
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg.width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg.height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--timing") == 0 && i + 1 < argc) {
            i++;
            if (strcmp(argv[i], "sync") == 0)      cfg.timing_mode = TIMING_SYNC;
            else if (strcmp(argv[i], "gpu") == 0)  cfg.timing_mode = TIMING_GPU;
            else { fprintf(stderr, "Unknown timing mode: %s\n", argv[i]); return 1; }
        } else if (strcmp(argv[i], "--render-res") == 0 && i + 1 < argc) {
            i++;
            int rw = 0, rh = 0;
            if (sscanf(argv[i], "%dx%d", &rw, &rh) == 2 && rw >= 64 && rh >= 64) {
                cfg.render_width = rw;
                cfg.render_height = rh;
            } else {
                fprintf(stderr, "Invalid render resolution: %s (expected WxH, e.g. 1920x1080)\n", argv[i]);
                return 1;
            }
        } else if (strcmp(argv[i], "--stress") == 0 && i + 1 < argc) {
            cfg.stress_duration_sec = atoi(argv[++i]);
            cfg.headless = true;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            printHelp();
            return 1;
        }
    }

    if (cfg.width < 64) cfg.width = 64;
    if (cfg.height < 64) cfg.height = 64;

    // GPU selection must happen before SDL_Init / GL context creation.
    if (gpu_index >= 0) {
        selectGPUAndReexec(gpu_index, argc, argv);
        const char* dri = getenv("DRI_PRIME");
        const char* glx = getenv("__GLX_VENDOR_LIBRARY_NAME");
        fprintf(stderr, "GPU %d selected", gpu_index);
        if (dri) fprintf(stderr, " (DRI_PRIME=%s)", dri);
        if (glx) fprintf(stderr, " (GLX=%s)", glx);
        fprintf(stderr, "\n");
    }

    App app;
    if (!app.init(cfg)) {
        fprintf(stderr, "Failed to initialize GPU_benchmark\n");
        return 1;
    }

    app.run();
    app.shutdown();
    return 0;
}
