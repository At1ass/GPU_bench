#include "demo/demo_runner.h"
#include "renderer/renderer.h"
#include "renderer/render_context.h"
#include "bench/poll_callback.h"
#include "platform/logger.h"
#include "platform/timer.h"
#include <cmath>
#include <algorithm>
#include <numeric>

DemoRunner::DemoRunner() {}

double DemoRunner::targetFPS(DemoTier tier) {
    switch (tier) {
        case DemoTier::Basic:    return 60.0;
        case DemoTier::Enhanced: return 45.0;
        case DemoTier::Quality:  return 30.0;
        case DemoTier::Ultra:    return 20.0;
    }
    return 60.0;
}

DemoTierResult DemoRunner::runTier(Renderer* r, RenderContext* ctx,
                                    DemoTier tier, int duration_sec,
                                    int render_w, int render_h,
                                    PollCallback* poll_cb,
                                    DemoCallbacks* demo_cb,
                                    int tier_index, int total_tiers) {
    DemoTierResult result;
    result.tier = tier;
    result.target_fps = targetFPS(tier);

    DemoTierConfig tcfg = getTierConfig(tier);
    Log::info("Demo: starting tier %d (%d seconds, terrain=%d, shadow=%d, bloom=%d%s%s%s)",
              static_cast<int>(tier), duration_sec, tcfg.terrain_resolution,
              tcfg.shadow_map_size, tcfg.bloom_passes,
              tcfg.use_pcf ? ", PCF" : "",
              tcfg.use_pbr ? ", PBR" : "",
              tcfg.use_god_rays ? ", god rays" : "");

    DemoScene scene;
    if (!scene.setup(r, tier, render_w, render_h)) {
        Log::warn("Demo: tier %d setup failed, skipping", static_cast<int>(tier));
        return result;
    }

    ctx->setVSync(false);
    r->resetState();

    // Warmup: 1 second
    {
        Timer warmup_timer;
        while (warmup_timer.elapsed_sec() < 1.0) {
            if (poll_cb && !poll_cb->onPoll()) {
                scene.cleanup(r);
                return result;
            }
            r->setViewport(0, 0, render_w, render_h);
            float t = static_cast<float>(warmup_timer.elapsed_sec() / (duration_sec + 1.0));
            scene.renderFrame(r, t, static_cast<float>(warmup_timer.elapsed_sec()), render_w, render_h);
            ctx->swapBuffers();
        }
    }

    // Drain warmup pipeline
    r->finish();

    // Measurement loop
    // Interactive: game-style pipeline — no glFinish, swapBuffers is the only sync
    // Headless: single glFinish per frame (no swap chain for back-pressure)
    bool headless = ctx->isHeadless();
    std::vector<double> frame_times;
    frame_times.reserve(duration_sec * 120);

    // Ring buffer for UI graph
    std::vector<double> frame_history;
    frame_history.reserve(120);

    double prev_ms = 0;
    double prev_fps = 0;
    Timer frame_t;

    Timer total_timer;
    while (total_timer.elapsed_sec() < duration_sec) {
        if (poll_cb && !poll_cb->onPoll()) break;

        // Frame time = wall clock between present/sync points
        double ms = frame_t.elapsed_ms();
        frame_t = Timer();

        if (prev_ms > 0) {
            frame_times.push_back(prev_ms);
            if (frame_history.size() >= 120)
                frame_history.erase(frame_history.begin());
            frame_history.push_back(prev_ms);
        }
        prev_ms = ms;
        prev_fps = (ms > 0) ? 1000.0 / ms : 0;

        r->setViewport(0, 0, render_w, render_h);

        float t = static_cast<float>(total_timer.elapsed_sec() / duration_sec);
        float time = static_cast<float>(total_timer.elapsed_sec());

        scene.renderFrame(r, t, time, render_w, render_h);

        // Demo overlay (1-frame delay — standard for games)
        float tier_progress = static_cast<float>(total_timer.elapsed_sec() / duration_sec);
        if (demo_cb) {
            if (!demo_cb->onDemoFrame(tier, tier_index, total_tiers,
                                       tier_progress, prev_fps, prev_ms, frame_history)) {
                break;
            }
        }

        // Sync: swapBuffers (interactive) or glFinish (headless)
        r->unbindState();
        if (headless) {
            r->finish();
        } else {
            ctx->swapBuffers();
        }
    }

    if (prev_ms > 0) {
        frame_times.push_back(prev_ms);
    }

    scene.cleanup(r);
    ctx->setVSync(true);

    // Compute stats
    if (frame_times.empty()) return result;

    result.frames = static_cast<int>(frame_times.size());

    auto sorted = frame_times;
    std::sort(sorted.begin(), sorted.end());

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    result.avg_ms = sum / sorted.size();
    result.avg_fps = (result.avg_ms > 0) ? 1000.0 / result.avg_ms : 0;

    // Min FPS = from max frame time
    result.min_fps = (sorted.back() > 0) ? 1000.0 / sorted.back() : 0;

    // Percentile FPS (from frame time percentiles)
    auto percentile = [&](double p) -> double {
        double idx = p * (sorted.size() - 1);
        int lo = static_cast<int>(floor(idx));
        int hi = static_cast<int>(ceil(idx));
        if (lo == hi) return sorted[lo];
        double frac = idx - lo;
        return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
    };

    double p99_ms = percentile(0.99);
    double p1_ms = percentile(0.01);
    result.p99_fps = (p99_ms > 0) ? 1000.0 / p99_ms : 0; // 1% lows
    result.p1_fps = (p1_ms > 0) ? 1000.0 / p1_ms : 0;    // 99% (near-best)

    result.normalized_score = result.avg_fps / result.target_fps;
    result.valid = true;

    Log::info("Demo tier %d: avg %.1f FPS (%.2f ms), min %.1f, P1%% %.1f, P99%% %.1f, score %.2f",
              static_cast<int>(tier), result.avg_fps, result.avg_ms,
              result.min_fps, result.p99_fps, result.p1_fps, result.normalized_score);

    return result;
}

DemoResults DemoRunner::run(Renderer* r, RenderContext* ctx,
                             const DemoConfig& cfg,
                             int render_w, int render_h,
                             PollCallback* poll_cb,
                             DemoCallbacks* demo_cb) {
    DemoResults results;
    results.gpu_name = r->getGPURenderer();
    results.render_w = render_w;
    results.render_h = render_h;

    int max_tier = maxSupportedTier(*r);
    Log::info("Demo mode: max supported tier = %d", max_tier);

    // Determine which tiers to run
    std::vector<DemoTier> tiers_to_run;
    if (cfg.tier_override > 0 && cfg.tier_override <= 4) {
        if (cfg.tier_override <= max_tier) {
            tiers_to_run.push_back(static_cast<DemoTier>(cfg.tier_override));
        } else {
            Log::warn("Demo: requested tier %d but max supported is %d", cfg.tier_override, max_tier);
            tiers_to_run.push_back(static_cast<DemoTier>(max_tier));
        }
    } else {
        // Run all supported tiers
        for (int t = 1; t <= max_tier; t++) {
            tiers_to_run.push_back(static_cast<DemoTier>(t));
        }
    }

    int total_tiers = static_cast<int>(tiers_to_run.size());

    for (int i = 0; i < total_tiers; i++) {
        if (poll_cb && !poll_cb->onPoll()) break;

        DemoTierResult tr = runTier(r, ctx, tiers_to_run[i], cfg.duration_per_tier,
                                     render_w, render_h, poll_cb, demo_cb,
                                     i, total_tiers);
        if (tr.valid) {
            results.tiers.push_back(tr);
        }
    }

    // Compute demo score: geometric mean of normalized scores × 10000
    if (!results.tiers.empty()) {
        double product = 1.0;
        int count = 0;
        for (const auto& t : results.tiers) {
            if (t.normalized_score > 0) {
                product *= t.normalized_score;
                count++;
            }
        }
        if (count > 0) {
            results.demo_score = pow(product, 1.0 / count) * 10000.0;
        }
    }

    Log::info("Demo Score: %.0f", results.demo_score);
    return results;
}
