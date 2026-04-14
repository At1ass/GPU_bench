// Standalone engine demo: spinning cube.
// Uses ONLY engine/ + renderer/ + platform/ + geometry/ layers.
// Zero dependencies on demo/ — proves engine is self-contained.
//
// Demonstrates the full engine pipeline:
//   RenderPassBase → buildPipeline() → PipelinePolicy → RenderPipeline::execute()

#define SDL_MAIN_HANDLED
#include "core/app_config.h"
#include "renderer/context/render_context.h"
#include "renderer/renderer_factory.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "geometry/math_types.h"
#include "engine/pass_context.h"
#include "engine/render_pass.h"
#include "engine/render_state.h"
#include "engine/render_pipeline.h"
#include "engine/pipeline_builder.h"
#include "engine/pipeline_policy.h"
#include "engine/frame_data.h"
#include "engine/scene_data.h"
#include "engine/shader_program.h"
#include "platform/timer.h"
#include <cmath>
#include <cstdio>
#include <vector>

// ---------------------------------------------------------------------------
// Pass 1: Clear background with gradient (fullscreen)
// ---------------------------------------------------------------------------
class ClearPass : public RenderPassBase {
public:
    const char* name() const override { return "clear"; }
    int executionOrder() const override { return 0; }

    void execute(PassContext& ctx, FrameData& fd, const SceneData&) override {
        ctx.renderer()->setViewport(0, 0, fd.viewport_w, fd.viewport_h);
        ctx.renderer()->clear(0.08f, 0.09f, 0.12f, 1.0f);
    }
};

// ---------------------------------------------------------------------------
// Pass 2: Spinning lit cube (geometry)
// ---------------------------------------------------------------------------
class SpinningCubePass : public RenderPassBase {
public:
    const char* name() const override { return "spinning_cube"; }
    int executionOrder() const override { return 50; }

    bool init(Renderer* r) {
        MeshData md = MeshGen::cube();
        cube_ = r->createMesh(md);
        if (!cube_) return false;

        const char* vs =
            "#version 120\n"
            "attribute vec3 a_pos;\n"
            "attribute vec3 a_normal;\n"
            "uniform mat4 u_proj;\n"
            "uniform mat4 u_view;\n"
            "uniform mat4 u_model;\n"
            "varying vec3 v_normal;\n"
            "void main() {\n"
            "    v_normal = mat3(u_model) * a_normal;\n"
            "    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);\n"
            "}\n";

        const char* fs =
            "#version 120\n"
            "varying vec3 v_normal;\n"
            "uniform vec3 u_light_dir;\n"
            "void main() {\n"
            "    vec3 n = normalize(v_normal);\n"
            "    float diff = max(dot(n, normalize(u_light_dir)), 0.0);\n"
            "    vec3 color = vec3(0.4, 0.6, 0.9) * (0.15 + diff * 0.85);\n"
            "    gl_FragColor = vec4(color, 1.0);\n"
            "}\n";

        if (!shader_.create(r, vs, fs)) return false;
        return true;
    }

    void execute(PassContext& ctx, FrameData& fd, const SceneData&) override {
        ctx.applyState(RenderState::opaque());
        shader_.use();
        shader_.setMat4("u_proj", fd.proj);
        shader_.setMat4("u_view", fd.view);
        shader_.setMat4("u_model", Mat4::rotateY(fd.time * 0.7f));
        shader_.set3f("u_light_dir", 0.5f, 0.8f, 0.3f);
        ctx.drawMesh(cube_);
    }

    void cleanup(Renderer* r) {
        if (cube_) { r->destroyMesh(cube_); cube_ = MeshHandle(); }
        shader_.reset();
    }

private:
    MeshHandle cube_;
    ShaderProgram shader_;
};

// ---------------------------------------------------------------------------
// Pipeline policy: everything to default framebuffer (simplest possible)
// ---------------------------------------------------------------------------
class SimplePipelinePolicy : public PipelinePolicy {
public:
    PassRTBinding routePass(const RenderPassBase&, int, int) const override {
        return PassRTBinding();  // default FB, no RT binding
    }
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // 1. Window + GL context
    AppConfig cfg;
    cfg.width = 800;
    cfg.height = 600;
    cfg.backend = RendererBackend::Auto;

    std::unique_ptr<RenderContext> ctx(createRenderContext(cfg));
    if (!ctx || !ctx->init(cfg)) {
        fprintf(stderr, "Failed to create render context\n");
        return 1;
    }

    // 2. Renderer
    std::unique_ptr<Renderer> renderer(createRenderer(cfg.backend));
    if (!renderer || !renderer->init(cfg.width, cfg.height)) {
        fprintf(stderr, "Failed to init renderer\n");
        ctx->shutdown();
        return 1;
    }

    printf("Engine standalone demo: %s\n", renderer->getGPURenderer());
    printf("GL: %s\n", renderer->getGLVersion());

    // 3. Create passes
    ClearPass clear_pass;
    SpinningCubePass cube_pass;
    if (!cube_pass.init(renderer.get())) {
        fprintf(stderr, "Failed to init cube pass\n");
        renderer->shutdown();
        ctx->shutdown();
        return 1;
    }

    // 4. Build pipeline using engine infrastructure
    //    Topological sort by resourceDecls + policy-driven RT routing
    std::vector<RenderPassBase*> passes;
    passes.push_back(&clear_pass);
    passes.push_back(&cube_pass);

    SimplePipelinePolicy policy;
    RenderPipeline pipeline;
    buildPipeline(pipeline, passes, policy, cfg.width, cfg.height);

    printf("Pipeline: %d nodes built\n", pipeline.nodeCount());

    // 5. Render loop
    Timer timer;
    bool running = true;
    SDL_Event e;
    int draw_w = 0, draw_h = 0;
    ctx->getDrawableSize(&draw_w, &draw_h);

    while (running) {
        while (ctx->pollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                ctx->getDrawableSize(&draw_w, &draw_h);
                // Rebuild pipeline with new viewport
                pipeline.clear();
                buildPipeline(pipeline, passes, policy, draw_w, draw_h);
            }
        }

        float t = static_cast<float>(timer.elapsed_sec());
        float aspect = static_cast<float>(draw_w) / static_cast<float>(draw_h > 0 ? draw_h : 1);

        float cam_r = 2.8f, cam_h = 1.5f;
        Vec3 eye(sinf(t * 0.4f) * cam_r, cam_h, cosf(t * 0.4f) * cam_r);

        FrameData fd;
        fd.proj = Mat4::perspective(60.0f, aspect, 0.1f, 50.0f);
        fd.view = Mat4::lookAt(eye, Vec3(0, 0, 0), Vec3(0, 1, 0));
        fd.cam_pos = eye;
        fd.time = t;
        fd.viewport_w = draw_w;
        fd.viewport_h = draw_h;

        SceneData scene;
        PassContext pass_ctx(renderer.get());
        pass_ctx.beginFrame();

        // Execute the pipeline — engine handles pass ordering and RT management
        pipeline.execute(pass_ctx, fd, scene);

        ctx->swapBuffers();
    }

    // 6. Cleanup
    cube_pass.cleanup(renderer.get());
    renderer->shutdown();
    ctx->shutdown();

    printf("Done.\n");
    return 0;
}
