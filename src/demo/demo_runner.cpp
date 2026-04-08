#include "demo/demo_runner.h"
#include "renderer/renderer.h"
#include "renderer/render_context.h"
#include "core/poll_callback.h"
#include "platform/logger.h"
#include "platform/timer.h"
#include "imgui.h"
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
                                    int window_w, int window_h,
                                    PollCallback* poll_cb,
                                    DemoCallbacks* demo_cb,
                                    int tier_index, int total_tiers,
                                    const TierResourceView& resources) {
    DemoTierResult result;
    result.tier = tier;
    result.target_fps = targetFPS(tier);

    DemoTierConfig tcfg = getTierConfig(tier);
    LOG_INF("Demo: starting tier %d (%d seconds, fur_shells=%d)",
              static_cast<int>(tier), duration_sec,
              tcfg.fur_shells);

    DemoScene scene;
    if (!scene.setup(r, tier, render_w, render_h, resources)) {
        LOG_WRN("Demo: tier %d setup failed, skipping", static_cast<int>(tier));
        return result;
    }

    TechniqueInfo tech_info = scene.getTechniqueInfo();

    // Create FBO if render resolution differs from window
    bool use_fbo = (render_w != window_w || render_h != window_h)
                   && r->supportsRenderTargets();
    RenderTargetHandle rt = INVALID_RENDER_TARGET;

    if (use_fbo) {
        rt = r->createRenderTarget(render_w, render_h);
        if (rt == INVALID_RENDER_TARGET) {
            use_fbo = false;
            LOG_WRN("Demo: FBO creation failed for %dx%d, falling back to window size",
                      render_w, render_h);
            render_w = window_w;
            render_h = window_h;
        } else {
            LOG_INF("Demo: rendering to FBO %dx%d", render_w, render_h);
        }
    }

    ctx->setVSync(false);
    r->resetState();

    // Warmup: show loading screen, prime GPU with a few hidden scene renders
    {
        LOG_DBG("Demo: tier %d warmup start", static_cast<int>(tier));
        bool headless_warmup = ctx->isHeadless();
        Timer warmup_timer;
        while (warmup_timer.elapsed_sec() < 1.0) {
            if (poll_cb && !poll_cb->onPoll()) {
                scene.cleanup(r);
                if (use_fbo) r->destroyRenderTarget(rt);
                return result;
            }

            // Render scene to FBO/backbuffer (primes GPU caches) but don't show it
            if (use_fbo) r->bindRenderTarget(rt);
            r->setViewport(0, 0, render_w, render_h);
            float t = 0.0f;  // Static camera during warmup (GPU priming only)
            RenderTargetHandle dest = use_fbo ? rt : INVALID_RENDER_TARGET;
            scene.renderFrame(r, t, static_cast<float>(warmup_timer.elapsed_sec()), render_w, render_h, dest);

            // Show loading screen instead of the scene
            if (!headless_warmup) {
                if (use_fbo) r->bindRenderTarget(INVALID_RENDER_TARGET);
                r->setViewport(0, 0, window_w, window_h);
                r->clear(0.08f, 0.08f, 0.10f, 1.0f);

                ctx->imguiNewFrame();
                ImVec2 screen(static_cast<float>(window_w), static_cast<float>(window_h));
                char msg[64];
                snprintf(msg, sizeof(msg), "Preparing tier %d...", static_cast<int>(tier));
                ImVec2 text_size = ImGui::CalcTextSize(msg);
                ImGui::SetNextWindowPos(ImVec2((screen.x - text_size.x) * 0.5f,
                                               (screen.y - text_size.y) * 0.5f));
                ImGui::SetNextWindowSize(ImVec2(0, 0));
                ImGui::Begin("##warmup", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "%s", msg);
                ImGui::End();
                ctx->imguiRender();
                r->unbindState();
            }

            ctx->swapBuffers();
        }
        LOG_DBG("Demo: tier %d warmup complete (%.1f ms)", static_cast<int>(tier),
                 warmup_timer.elapsed_ms());
    }

    // Drain warmup pipeline
    r->finish();

    // Measurement loop
    bool headless = ctx->isHeadless();
    std::vector<double> frame_times;
    frame_times.reserve(static_cast<size_t>(duration_sec) * 120);

    std::vector<double> frame_history;
    frame_history.reserve(120);

    double prev_ms = 0;
    double prev_fps = 0;
    Timer frame_t;

    Timer total_timer;
    while (total_timer.elapsed_sec() < static_cast<double>(duration_sec)) {
        if (poll_cb && !poll_cb->onPoll()) break;

        double ms = frame_t.elapsed_ms();
        frame_t = Timer();

        if (prev_ms > 0) {
            frame_times.push_back(prev_ms);
            if (frame_history.size() >= 120u)
                frame_history.erase(frame_history.begin());
            frame_history.push_back(prev_ms);
        }
        prev_ms = ms;
        prev_fps = (ms > 0) ? 1000.0 / ms : 0;

        if (use_fbo) r->bindRenderTarget(rt);
        r->setViewport(0, 0, render_w, render_h);

        float t = static_cast<float>(total_timer.elapsed_sec() / static_cast<double>(duration_sec));
        float time = static_cast<float>(total_timer.elapsed_sec());

        RenderTargetHandle dest = use_fbo ? rt : INVALID_RENDER_TARGET;
        scene.renderFrame(r, t, time, render_w, render_h, dest);

        // Blit to screen before overlay
        if (use_fbo) {
            r->bindRenderTarget(INVALID_RENDER_TARGET);
            r->setViewport(0, 0, window_w, window_h);
            r->blitToScreen(rt, 0, 0, window_w, window_h);
        }

        // Demo overlay (drawn to screen, not FBO)
        float tier_progress = static_cast<float>(total_timer.elapsed_sec() / static_cast<double>(duration_sec));
        if (demo_cb) {
            const FrameStats* fstats = &scene.lastFrameStats();
            if (!demo_cb->onDemoFrame(tier, tier_index, total_tiers,
                                       tier_progress, prev_fps, prev_ms, frame_history,
                                       &tech_info, fstats)) {
                break;
            }
        }

        // Sync
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
    if (use_fbo) {
        r->destroyRenderTarget(rt);
        LOG_DBG("Demo: destroyed FBO");
    }
    ctx->setVSync(true);

    // Compute stats
    if (frame_times.empty()) return result;

    result.frames = static_cast<int>(frame_times.size());

    auto sorted = frame_times;
    std::sort(sorted.begin(), sorted.end());

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    result.avg_ms = sum / static_cast<double>(sorted.size());
    result.avg_fps = (result.avg_ms > 0) ? 1000.0 / result.avg_ms : 0;

    result.min_fps = (sorted.back() > 0) ? 1000.0 / sorted.back() : 0;

    auto percentile = [&](double p) -> double {
        double idx = p * static_cast<double>(sorted.size() - 1);
        size_t lo = static_cast<size_t>(floor(idx));
        size_t hi = static_cast<size_t>(ceil(idx));
        if (lo == hi) return sorted[lo];
        double frac = idx - static_cast<double>(lo);
        return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
    };

    double p99_ms = percentile(0.99);
    double p1_ms = percentile(0.01);
    result.p99_fps = (p99_ms > 0) ? 1000.0 / p99_ms : 0;
    result.p1_fps = (p1_ms > 0) ? 1000.0 / p1_ms : 0;

    result.normalized_score = result.avg_fps / result.target_fps;
    result.valid = true;

    LOG_INF("Demo tier %d: avg %.1f FPS (%.2f ms), min %.1f, P1%% %.1f, P99%% %.1f, score %.2f",
              static_cast<int>(tier), result.avg_fps, result.avg_ms,
              result.min_fps, result.p99_fps, result.p1_fps, result.normalized_score);
    LOG_DBG("Demo: tier %d measurement: %d frames, avg %.1f fps, score %.2f",
             static_cast<int>(tier), result.frames, result.avg_fps, result.normalized_score);

    return result;
}

DemoResults DemoRunner::run(Renderer* r, RenderContext* ctx,
                             const DemoConfig& cfg,
                             int render_w, int render_h,
                             int window_w, int window_h,
                             PollCallback* poll_cb,
                             DemoCallbacks* demo_cb) {
    DemoResults results;
    results.gpu_name = r->getGPURenderer();
    results.render_w = render_w;
    results.render_h = render_h;

    int max_tier = maxSupportedTier(*r);
    LOG_INF("Demo mode: max supported tier = %d", max_tier);

    std::vector<DemoTier> tiers_to_run;
    if (cfg.tier_override > 0 && cfg.tier_override <= 4) {
        if (cfg.tier_override <= max_tier) {
            tiers_to_run.push_back(static_cast<DemoTier>(cfg.tier_override));
        } else {
            LOG_WRN("Demo: requested tier %d but max supported is %d", cfg.tier_override, max_tier);
            tiers_to_run.push_back(static_cast<DemoTier>(max_tier));
        }
    } else {
        for (int t = 1; t <= max_tier; t++) {
            tiers_to_run.push_back(static_cast<DemoTier>(t));
        }
    }

    int total_tiers = static_cast<int>(tiers_to_run.size());

    // Show loading screen before preparation
    if (!ctx->isHeadless()) {
        r->resetState();
        r->setViewport(0, 0, window_w, window_h);
        r->clear(0.08f, 0.08f, 0.10f, 1.0f);
        ctx->imguiNewFrame();

        ImVec2 screen(static_cast<float>(window_w), static_cast<float>(window_h));
        const char* msg = "Initializing resources...";
        ImVec2 text_size = ImGui::CalcTextSize(msg);
        ImGui::SetNextWindowPos(ImVec2((screen.x - text_size.x) * 0.5f,
                                       (screen.y - text_size.y) * 0.5f));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("##loading", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "%s", msg);
        ImGui::End();

        ctx->imguiRender();
        r->unbindState();
        ctx->swapBuffers();
    }

    // Preparation stage: load all shared resources once
    DemoResources resources;
    if (!resources.prepare(r, max_tier, render_w, render_h)) {
        LOG_ERR("Demo: resource preparation failed");
        return results;
    }

    for (int i = 0; i < total_tiers; i++) {
        if (poll_cb && !poll_cb->onPoll()) break;

        TierResourceView view = resources.viewForTier(tiers_to_run[static_cast<size_t>(i)]);
        DemoTierResult tr = runTier(r, ctx, tiers_to_run[static_cast<size_t>(i)], cfg.duration_per_tier,
                                     render_w, render_h, window_w, window_h,
                                     poll_cb, demo_cb,
                                     i, total_tiers, view);
        if (tr.valid) {
            results.tiers.push_back(tr);
        }
    }

    resources.destroy();

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

    LOG_INF("Demo Score: %.0f", results.demo_score);
    return results;
}
