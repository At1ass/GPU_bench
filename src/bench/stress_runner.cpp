#include "bench/stress_runner.h"
#include "core/poll_callback.h"
#include "renderer/renderer.h"
#include "renderer/render_context.h"
#include "geometry/mesh_gen.h"
#include "tests/tests.h"          // for genColorNoise
#include "platform/logger.h"
#include "platform/timer.h"

// Stress test parameters
static constexpr double STRESS_TARGET_FRAME_MS  = 40.0;
static constexpr double STRESS_REPORT_INTERVAL  = 10.0;
static constexpr int    STRESS_MAX_PASSES        = 10000;
static constexpr double STRESS_CALIBRATION_RATIO = 0.8;
static constexpr double STRESS_THROTTLE_SEVERE   = 5.0;
static constexpr double STRESS_THROTTLE_WARN     = 2.0;

void StressRunner::run(Renderer* r, RenderContext* ctx,
                       int shader_iterations,
                       int render_w, int render_h,
                       int duration_sec,
                       PollCallback* cb) {
    static const char* STRESS_VS_120 =
        "#version 120\n"
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_uv;\n"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
        "}\n";

    static const char* STRESS_FS_120 =
        "#version 120\n"
        "varying vec2 v_uv;\n"
        "uniform sampler2D u_tex;\n"
        "uniform int u_iterations;\n"
        "uniform float u_time;\n"
        "void main() {\n"
        "    vec4 t0 = texture2D(u_tex, v_uv);\n"
        "    vec4 t1 = texture2D(u_tex, v_uv * 2.0 + vec2(u_time * 0.01));\n"
        "    vec4 t2 = texture2D(u_tex, v_uv * 4.0 - vec2(u_time * 0.02));\n"
        "    vec4 t3 = texture2D(u_tex, v_uv.yx * 3.0 + vec2(u_time * 0.015));\n"
        "    vec4 acc = t0 * 0.25 + t1 * 0.25 + t2 * 0.25 + t3 * 0.25;\n"
        "    vec4 fma_acc = acc;\n"
        "    for (int i = 0; i < u_iterations; i++) {\n"
        "        fma_acc = fma_acc * acc + fma_acc;\n"
        "        fma_acc = fma_acc * vec4(0.5) + vec4(0.25);\n"
        "        acc.x = sin(fma_acc.x * 3.14159);\n"
        "        acc.y = cos(fma_acc.y * 3.14159);\n"
        "        acc.z = sqrt(abs(fma_acc.z));\n"
        "        acc.w = fma_acc.w;\n"
        "        fma_acc = fma_acc * acc + fma_acc;\n"
        "        fma_acc = fma_acc * vec4(0.5) + vec4(0.25);\n"
        "    }\n"
        "    gl_FragColor = vec4(fma_acc.rgb * 0.5 + 0.25, 0.8);\n"
        "}\n";

    // GLSL 150 core profile variants
    static const char* STRESS_VS_150 =
        "#version 150\n"
        "in vec2 a_pos;\n"
        "in vec2 a_uv;\n"
        "out vec2 v_uv;\n"
        "void main() {\n"
        "    v_uv = a_uv;\n"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
        "}\n";

    static const char* STRESS_FS_150 =
        "#version 150\n"
        "in vec2 v_uv;\n"
        "uniform sampler2D u_tex;\n"
        "uniform int u_iterations;\n"
        "uniform float u_time;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec4 t0 = texture(u_tex, v_uv);\n"
        "    vec4 t1 = texture(u_tex, v_uv * 2.0 + vec2(u_time * 0.01));\n"
        "    vec4 t2 = texture(u_tex, v_uv * 4.0 - vec2(u_time * 0.02));\n"
        "    vec4 t3 = texture(u_tex, v_uv.yx * 3.0 + vec2(u_time * 0.015));\n"
        "    vec4 acc = t0 * 0.25 + t1 * 0.25 + t2 * 0.25 + t3 * 0.25;\n"
        "    vec4 fma_acc = acc;\n"
        "    for (int i = 0; i < u_iterations; i++) {\n"
        "        fma_acc = fma_acc * acc + fma_acc;\n"
        "        fma_acc = fma_acc * vec4(0.5) + vec4(0.25);\n"
        "        acc.x = sin(fma_acc.x * 3.14159);\n"
        "        acc.y = cos(fma_acc.y * 3.14159);\n"
        "        acc.z = sqrt(abs(fma_acc.z));\n"
        "        acc.w = fma_acc.w;\n"
        "        fma_acc = fma_acc * acc + fma_acc;\n"
        "        fma_acc = fma_acc * vec4(0.5) + vec4(0.25);\n"
        "    }\n"
        "    fragColor = vec4(fma_acc.rgb * 0.5 + 0.25, 0.8);\n"
        "}\n";

    const char* stress_vs = r->isCoreProfile() ? STRESS_VS_150 : STRESS_VS_120;
    const char* stress_fs = r->isCoreProfile() ? STRESS_FS_150 : STRESS_FS_120;

    ctx->setVSync(false);
    r->resetState();

    MeshHandle stress_quad = r->createMesh(MeshGen::quad());
    ShaderHandle stress_shader = r->createCustomShader(stress_vs, stress_fs);

    int tex_size = 1024;
    auto noise = genColorNoise(tex_size, 42);
    TextureHandle stress_tex = r->createTexture(tex_size, tex_size, 3, noise.data());

    int u_tex_loc = -1, u_iter_loc = -1, u_time_loc = -1;
    if (stress_shader != INVALID_SHADER) {
        u_tex_loc = r->getCustomUniformLoc(stress_shader, "u_tex");
        u_iter_loc = r->getCustomUniformLoc(stress_shader, "u_iterations");
        u_time_loc = r->getCustomUniformLoc(stress_shader, "u_time");
    }

    #define STRESS_RENDER_PASS(time_val) do { \
        if (stress_shader != INVALID_SHADER) { \
            r->useCustomShader(stress_shader); \
            r->setUniform1i(u_iter_loc, shader_iterations); \
            r->setUniform1f(u_time_loc, time_val); \
            r->setUniform1i(u_tex_loc, 0); \
        } \
        r->bindTexture(stress_tex); \
        r->drawMesh(stress_quad); \
    } while(0)

    // --- Calibration ---
    const double TARGET_FRAME_MS = STRESS_TARGET_FRAME_MS;
    int passes = 1;
    float stress_time = 0.0f;

    LOG_INF("Stress test: calibrating...");

    r->setViewport(0, 0, render_w, render_h);
    r->setDepthTest(false);
    r->setBlending(true);

    for (int attempt = 0; attempt < 8; attempt++) {
        if (cb && !cb->onPoll()) break;

        r->clear(0.1f, 0.1f, 0.12f, 1.0f);
        for (int i = 0; i < passes; i++) STRESS_RENDER_PASS(stress_time);
        r->finish();

        Timer cal_t;
        r->clear(0.1f, 0.1f, 0.12f, 1.0f);
        for (int i = 0; i < passes; i++) STRESS_RENDER_PASS(stress_time);
        r->finish();
        double ms = cal_t.elapsed_ms();

        LOG_INF("  %d passes = %.1f ms", passes, ms);

        if (ms >= TARGET_FRAME_MS * STRESS_CALIBRATION_RATIO) break;

        int new_passes = (ms > 0.1)
            ? static_cast<int>(lround(passes * TARGET_FRAME_MS / ms)) : passes * 4;
        if (new_passes <= passes) new_passes = passes + 1;
        if (new_passes > STRESS_MAX_PASSES) new_passes = STRESS_MAX_PASSES;
        passes = new_passes;
    }
    ctx->swapBuffers();

    LOG_INF("Stress test: combined shader (%d iters, %d passes/frame), %d seconds",
            shader_iterations, passes, duration_sec);

    // --- Main stress loop ---
    {
        Timer stress_timer;
        Timer interval_timer;
        const double REPORT_INTERVAL = STRESS_REPORT_INTERVAL;
        int total_frames = 0;
        double interval_time_sum = 0;
        int interval_frames = 0;
        double baseline_fps = 0;
        int interval_num = 0;

        while (stress_timer.elapsed_sec() < duration_sec) {
            if (cb && !cb->onPoll()) break;

            r->setViewport(0, 0, render_w, render_h);
            r->setDepthTest(false);
            r->setBlending(true);
            r->clear(0.1f, 0.1f, 0.12f, 1.0f);

            r->finish();
            Timer frame_t;
            for (int i = 0; i < passes; i++) {
                STRESS_RENDER_PASS(stress_time);
                stress_time += 0.001f;
            }
            r->finish();
            double ms = frame_t.elapsed_ms();

            interval_time_sum += ms;
            interval_frames++;
            total_frames++;

            ctx->swapBuffers();

            if (interval_timer.elapsed_sec() >= REPORT_INTERVAL) {
                interval_num++;
                double avg_ms = interval_time_sum / interval_frames;
                double fps = (avg_ms > 0) ? 1000.0 / avg_ms : 0;

                if (interval_num == 1) {
                    baseline_fps = fps;
                    LOG_INF("[%3.0fs] Baseline: %.1f FPS (avg %.1f ms)",
                            stress_timer.elapsed_sec(), fps, avg_ms);
                } else {
                    double degradation = (baseline_fps > 0)
                        ? (baseline_fps - fps) / baseline_fps * 100.0 : 0;
                    if (degradation > STRESS_THROTTLE_SEVERE) {
                        LOG_WRN("[%3.0fs] %.1f FPS (avg %.1f ms) — THROTTLING: %.1f%% degradation",
                                stress_timer.elapsed_sec(), fps, avg_ms, degradation);
                    } else if (degradation > STRESS_THROTTLE_WARN) {
                        LOG_INF("[%3.0fs] %.1f FPS (avg %.1f ms) — minor degradation: %.1f%%",
                                stress_timer.elapsed_sec(), fps, avg_ms, degradation);
                    } else {
                        LOG_INF("[%3.0fs] %.1f FPS (avg %.1f ms)",
                                stress_timer.elapsed_sec(), fps, avg_ms);
                    }
                }

                interval_time_sum = 0;
                interval_frames = 0;
                interval_timer.reset();
            }
        }

        double total_sec = stress_timer.elapsed_sec();
        LOG_INF("Stress test completed: %d frames in %.0fs (avg %.1f FPS)",
                total_frames, total_sec,
                (total_sec > 0) ? total_frames / total_sec : 0);
    }

    #undef STRESS_RENDER_PASS

    r->destroyMesh(stress_quad);
    r->destroyTexture(stress_tex);
    if (stress_shader != INVALID_SHADER)
        r->destroyCustomShader(stress_shader);
    ctx->setVSync(true);
}
