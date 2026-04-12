#include "bench/bench_runner.h"
#include "tests/tests.h"
#include "tests/test_registry.h"
#include "renderer/renderer.h"
#include "renderer/features.h"
#include "renderer/context/render_context.h"
#include "platform/logger.h"
#include <cmath>
#include <algorithm>
#include <memory>

// Probe test parameters
static const int PROBE_MAX_FRAMES  = 60;
static const int PROBE_MIN_FRAMES  = 20;
static const double PROBE_TIME_BUDGET = 2.0; // seconds per sub-probe
static const int PROBE_RESOLUTION  = 512;
static const int PROBE_FILL_LAYERS = 200;
static const int PROBE_GEOM_GRID   = 25;
static const int PROBE_WARMUP      = 5;
static const double PROBE_WARMUP_BUDGET = 0.5; // seconds for warmup

// Trim fraction for measurements (drop top/bottom 15%)
static const double PROBE_TRIM_FRAC = 0.15;

// Trim outliers from sorted times: drop PROBE_TRIM_FRAC from each end
static std::vector<double> trimmedTimes(const std::vector<double>& raw) {
    std::vector<double> times(raw);
    std::sort(times.begin(), times.end());
    int n = static_cast<int>(times.size());
    int trim = static_cast<int>(n * PROBE_TRIM_FRAC);
    if (trim * 2 >= n) trim = 0;
    return std::vector<double>(times.begin() + trim, times.end() - trim);
}

BenchRunner::BenchRunner()
    : progress_(0), active_(false), bench_rt_(INVALID_RENDER_TARGET) {}

void BenchRunner::clearResults() {
    results_.clear();
    composite_ = CompositeScore();
    bottleneck_ = BottleneckInfo();
}

void BenchRunner::computeAnalysis() {
    composite_ = computeCompositeScores(results_);
    bottleneck_ = detectBottleneck(results_, composite_);
}

void BenchRunner::runSelected(Renderer* r, RenderContext* ctx,
                              const BenchPreset& preset, const BenchConfig& cfg,
                              const bool* test_enabled, uint32_t available_caps,
                              BenchCallbacks* cb) {
    results_.clear();
    for (int i = 0; i < NUM_TESTS; i++) {
        if (!test_enabled[i]) continue;
        // Skip tests requiring unsupported capabilities
        if ((g_tests[i].required_caps & available_caps) != g_tests[i].required_caps) {
            LOG_INF("Skipping '%s' — requires unsupported capabilities", g_tests[i].display_name);
            continue;
        }
        if (cb && !cb->onPoll()) break;
        auto test = g_tests[i].factory(preset);
        runTest(test.get(), r, ctx, cfg, cb);
    }
    // Clean up render target
    if (bench_rt_ != INVALID_RENDER_TARGET) {
        r->destroyRenderTarget(bench_rt_);
        bench_rt_ = INVALID_RENDER_TARGET;
    }
    computeAnalysis();
}

void BenchRunner::runAll(Renderer* r, RenderContext* ctx,
                         const BenchPreset& preset, const BenchConfig& cfg,
                         uint32_t available_caps, BenchCallbacks* cb) {
    bool all_enabled[NUM_TESTS];
    for (int i = 0; i < NUM_TESTS; i++) all_enabled[i] = true;
    runSelected(r, ctx, preset, cfg, all_enabled, available_caps, cb);
}

void BenchRunner::runTest(BenchTest* test, Renderer* r, RenderContext* ctx,
                          const BenchConfig& cfg, BenchCallbacks* cb) {
    active_ = true;
    status_ = std::string("Running: ") + test->name();
    progress_ = 0;
    LOG_DBG("BenchRunner: starting test '%s' (%dx%d, timing=%s)",
             test->name(), cfg.render_w, cfg.render_h,
             cfg.timing_mode == TimingMode::GPU ? "gpu" : "sync");

    ctx->setVSync(false);

    // Determine if we need render target (render resolution != window)
    bool use_fbo = (cfg.render_w != cfg.window_w || cfg.render_h != cfg.window_h) && r->supportsRenderTargets();
    if (use_fbo) {
        if (bench_rt_ != INVALID_RENDER_TARGET) {
            r->destroyRenderTarget(bench_rt_);
            bench_rt_ = INVALID_RENDER_TARGET;
        }
        bench_rt_ = r->createRenderTarget(cfg.render_w, cfg.render_h);
        if (bench_rt_ == INVALID_RENDER_TARGET) {
            use_fbo = false;
            LOG_WRN("Render target creation failed, falling back to viewport-only");
        }
    }

    if (use_fbo) r->bindRenderTarget(bench_rt_);

    r->resetState();
    LOG_DBG("Bench: setting up '%s'", test->name());
    test->setup(r, cfg.render_w, cfg.render_h);

    bool use_gpu_timer = (cfg.timing_mode == TimingMode::GPU) && r->hasTimerQueries();
    RenderTargetHandle rt = use_fbo ? bench_rt_ : INVALID_RENDER_TARGET;

    // Warmup
    bool sanity_ok = true;
    bool cancelled = false;
    status_ = std::string("Warmup: ") + test->name();
    for (int i = 0; i < cfg.warmup_frames && !cancelled; i++) {
        if (use_fbo) r->bindRenderTarget(bench_rt_);
        r->setViewport(0, 0, cfg.render_w, cfg.render_h);
        r->clear(0.1f, 0.1f, 0.12f, 1.0f);
        test->render(r);
        r->finish();

        // Sanity check after first warmup frame (per-test-type dispatch)
        if (i == 0) {
            SanityType stype = test->sanityType();
            if (stype == SanityType::Framebuffer) {
                unsigned char px[4 * 4 * 4] = {0};
                int cx = cfg.render_w / 2 - 2, cy = cfg.render_h / 2 - 2;
                if (cx < 0) cx = 0;
                if (cy < 0) cy = 0;
                r->readPixels(cx, cy, 4, 4, px);
                int nonzero = 0;
                for (int j = 0; j < 4 * 4 * 4; j++) nonzero += (px[j] > 0) ? 1 : 0;
                if (nonzero == 0) {
                    sanity_ok = false;
                    LOG_WRN("Sanity check FAILED for '%s' — render output is black", test->name());
                    LOG_DBG("Bench: sanity pixel sample at (%d,%d): R=%d G=%d B=%d A=%d",
                             cx, cy, px[0], px[1], px[2], px[3]);
                }
            } else if (stype == SanityType::ComputeBuffer) {
                BufferHandle buf = test->getOutputBuffer();
                auto* comp = r->features<ComputeFeatures>();
                if (comp && buf != INVALID_BUFFER) {
                    unsigned char data[64] = {0};
                    comp->readSSBO(buf, data, 0, 64);
                    int nonzero = 0;
                    for (int j = 0; j < 64; j++) nonzero += (data[j] != 0) ? 1 : 0;
                    if (nonzero == 0) {
                        sanity_ok = false;
                        LOG_WRN("Sanity check FAILED for '%s' — compute output buffer is zero", test->name());
                    }
                }
            }
            // SanityType::None — sanity_ok remains true
        }

        progress_ = static_cast<int>(100.0 * i / cfg.warmup_frames * 0.1);
        if (cb && !cb->onFrame(rt)) cancelled = true;
    }

    if (!cancelled) {
        LOG_DBG("Bench: warmup done for '%s' (%d frames)", test->name(), cfg.warmup_frames);

        // Measurement
        status_ = std::string("Measuring: ") + test->name();
        std::vector<double> times;
        std::vector<double> gpu_times;
        times.reserve(static_cast<size_t>(cfg.measure_frames) * 2);
        if (use_gpu_timer) gpu_times.reserve(static_cast<size_t>(cfg.measure_frames) * 2);
        Timer total_timer;
        Timer frame_t;

        int frame = 0;
        for (;;) {
            if (use_fbo) r->bindRenderTarget(bench_rt_);
            r->setViewport(0, 0, cfg.render_w, cfg.render_h);
            r->clear(0.1f, 0.1f, 0.12f, 1.0f);

            r->finish();

            if (use_gpu_timer) r->timerBegin();
            frame_t.reset();
            test->render(r);
            if (use_gpu_timer) r->timerEnd();
            r->finish();
            double ms = frame_t.elapsed_ms();
            times.push_back(ms);
            if (use_gpu_timer) gpu_times.push_back(r->timerElapsedMs());

            double elapsed = total_timer.elapsed_sec();
            progress_ = 10 + static_cast<int>(90.0 * std::min(
                static_cast<double>(frame) / cfg.measure_frames,
                elapsed / cfg.min_duration_sec
            ));

            if (cb && !cb->onFrame(rt)) { cancelled = true; break; }

            frame++;
            if (frame >= cfg.measure_frames && elapsed >= cfg.min_duration_sec)
                break;
        }

        if (!cancelled) {
            if (use_fbo) r->bindRenderTarget(INVALID_RENDER_TARGET);

            double score = test->computeScore(times, cfg.render_w, cfg.render_h);
            BenchResult result = computeStats(test->name(), test->scoreUnit(), times, score);

            if (!gpu_times.empty()) {
                auto trimmed_gpu = trimmedTimes(gpu_times);
                double gpu_sum = 0;
                for (size_t i = 0; i < trimmed_gpu.size(); i++) gpu_sum += trimmed_gpu[i];
                result.gpu_ms = trimmed_gpu.empty() ? 0 : gpu_sum / static_cast<double>(trimmed_gpu.size());
            }

            result.sanity_ok = sanity_ok;
            if (!sanity_ok) result.valid = false;

            LOG_DBG("Bench: '%s' measured %d frames in %.1fs", test->name(),
                     result.frames, result.avg_ms * result.frames / 1000.0);
            results_.push_back(result);
        }
    }

    // Cleanup (always runs)
    if (bench_rt_ != INVALID_RENDER_TARGET)
        r->bindRenderTarget(INVALID_RENDER_TARGET);
    test->cleanup(r);
    LOG_DBG("Bench: cleanup '%s'", test->name());
    active_ = false;
    status_ = "Done";
    ctx->setVSync(true);
}

GPUTier BenchRunner::runQuickProbe(Renderer* r, RenderContext* ctx,
                                    int render_w, int render_h,
                                    BenchCallbacks* cb) {
    const int PROBE_W = PROBE_RESOLUTION, PROBE_H = PROBE_RESOLUTION;

    (void)render_w;
    (void)render_h;

    ctx->setVSync(false);
    r->resetState();

    double fill_score = 0;
    double geom_score = 0;

    // Probe fillrate
    {
        FillrateParams fp; fp.layers = PROBE_FILL_LAYERS;
        FillrateTest test(fp);
        test.setup(r, PROBE_W, PROBE_H);
        r->setViewport(0, 0, PROBE_W, PROBE_H);

        // Time-budgeted warmup
        Timer warmup_t;
        for (int i = 0; i < PROBE_WARMUP && warmup_t.elapsed_sec() < PROBE_WARMUP_BUDGET; i++) {
            r->clear(0.0f, 0.0f, 0.0f, 1.0f);
            test.render(r);
            r->finish();
        }

        // Time-budgeted measurement
        std::vector<double> times;
        times.reserve(PROBE_MAX_FRAMES);
        Timer budget_t;
        Timer ft;
        for (int i = 0; i < PROBE_MAX_FRAMES; i++) {
            if (i >= PROBE_MIN_FRAMES && budget_t.elapsed_sec() >= PROBE_TIME_BUDGET) break;
            if (cb && !cb->onPoll()) break;
            r->clear(0.0f, 0.0f, 0.0f, 1.0f);
            r->finish();
            ft.reset();
            test.render(r);
            r->finish();
            times.push_back(ft.elapsed_ms());
        }
        auto trimmed = trimmedTimes(times);
        fill_score = test.computeScore(trimmed, PROBE_W, PROBE_H);
        test.cleanup(r);
    }

    // Probe geometry
    {
        GeometryParams gp; gp.grid_size = PROBE_GEOM_GRID;
        GeometryTest test(gp);
        r->resetState();
        test.setup(r, PROBE_W, PROBE_H);
        r->setViewport(0, 0, PROBE_W, PROBE_H);

        Timer warmup_t;
        for (int i = 0; i < PROBE_WARMUP && warmup_t.elapsed_sec() < PROBE_WARMUP_BUDGET; i++) {
            r->clear(0.0f, 0.0f, 0.0f, 1.0f);
            test.render(r);
            r->finish();
        }

        std::vector<double> times;
        times.reserve(PROBE_MAX_FRAMES);
        Timer budget_t;
        Timer ft;
        for (int i = 0; i < PROBE_MAX_FRAMES; i++) {
            if (i >= PROBE_MIN_FRAMES && budget_t.elapsed_sec() >= PROBE_TIME_BUDGET) break;
            if (cb && !cb->onPoll()) break;
            r->clear(0.0f, 0.0f, 0.0f, 1.0f);
            r->finish();
            ft.reset();
            test.render(r);
            r->finish();
            times.push_back(ft.elapsed_ms());
        }
        auto trimmed = trimmedTimes(times);
        geom_score = test.computeScore(trimmed, PROBE_W, PROBE_H);
        test.cleanup(r);
    }

    r->resetState();
    ctx->setVSync(true);

    GPUTier tier = classifyGPUTier(r->getCaps(), getAvailableCaps(*r), fill_score, geom_score);
    LOG_INF("Quick probe: fill=%.1f Mpix/s, geom=%.1f Mtris/s -> tier=%s",
            fill_score, geom_score, gpuTierName(tier));
    return tier;
}
