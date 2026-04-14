// Standalone engine demo: scene + post-processing pipeline.
// Demonstrates ResourceDecl dependencies, FBO rendering, and fullscreen pass.
// Uses ONLY engine/ + renderer/ + platform/ + geometry/ layers. Zero demo/ code.
//
// Pipeline:
//   ScenePass (WRITE SceneColor) → renders spinning cube into FBO
//   PostProcessPass (READ SceneColor)  → reads FBO texture, inverts colors, outputs to screen
//
// Engine topological sort ensures ScenePass runs before PostProcessPass automatically.

#include "core/app_config.h"
#include "renderer/context/render_context.h"
#include "renderer/renderer_factory.h"
#include "renderer/renderer.h"
#include "geometry/mesh_gen.h"
#include "geometry/math_types.h"
#include "engine/pass_context.h"
#include "engine/render_pass.h"
#include "engine/fullscreen_pass.h"
#include "engine/render_state.h"
#include "engine/shader_cache.h"
#include "engine/shader_feature_set.h"
#include "engine/uniform_id.h"
#include "renderer/features.h"
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
#include <memory>

// ---------------------------------------------------------------------------
// Pass 1: Render scene (spinning cube) into FBO
// Declares WRITE SceneColor → pipeline knows this must run first
// ---------------------------------------------------------------------------
class ScenePass : public RenderPassBase {
public:
    const char* name() const override { return "scene"; }
    int executionOrder() const override { return 50; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor, ResourceDecl::WRITE }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }

    bool init(ShaderCache& cache, ShaderFeatureSet features,
              Renderer* r, RenderTargetHandle scene_rt) {
        scene_rt_ = scene_rt;

        MeshData md = MeshGen::cube();
        cube_ = r->createMesh(md);
        if (!cube_) return false;

        // Uber-compatible: uses ATTR_IN/VS_OUT/FRAG_COLOR macros from preamble
        const char* vs =
            "ATTR_IN vec3 a_pos;\n"
            "ATTR_IN vec3 a_normal;\n"
            "uniform mat4 u_proj, u_view, u_model;\n"
            "VS_OUT vec3 v_normal;\n"
            "void main() {\n"
            "    v_normal = mat3(u_model) * a_normal;\n"
            "    gl_Position = u_proj * u_view * u_model * vec4(a_pos, 1.0);\n"
            "}\n";

        const char* fs =
            "FS_IN vec3 v_normal;\n"
            "void main() {\n"
            "    vec3 n = normalize(v_normal);\n"
            "    float diff = max(dot(n, vec3(0.5, 0.8, 0.3)), 0.0);\n"
            "    vec3 color = vec3(0.9, 0.4, 0.2) * (0.15 + diff * 0.85);\n"
            "    FRAG_COLOR = vec4(color, 1.0);\n"
            "}\n";

        shader_ = cache.compileInline("scene_cube", vs, fs, features);
        if (!shader_) return false;
        ub_.init(shader_);
        return true;
    }

    void execute(PassContext& ctx, FrameData& fd, const SceneData&) override {
        Renderer* r = ctx.renderer();

        r->bindRenderTarget(scene_rt_);
        r->setViewport(0, 0, fd.viewport_w, fd.viewport_h);
        r->setDepthMask(true);
        r->clear(0.12f, 0.15f, 0.25f, 1.0f);

        ctx.applyState(RenderState::opaque());
        ub_.use();
        ub_.set(U::Proj, fd.proj);
        ub_.set(U::View, fd.view);
        ub_.set(U::Model, Mat4::rotateY(fd.time * 0.7f));
        ctx.drawMesh(cube_);

        r->bindRenderTarget(INVALID_RENDER_TARGET);
    }

    void cleanup(Renderer* r) {
        if (cube_) { r->destroyMesh(cube_); cube_ = MeshHandle(); }
        // shader owned by ShaderCache
    }

private:
    RenderTargetHandle scene_rt_;
    MeshHandle cube_;
    ShaderProgram* shader_ = nullptr;
    UniformBlock ub_;
};

// ---------------------------------------------------------------------------
// Pass 2: Fullscreen post-process (sepia + vignette) using FullscreenPass.
// Declares READ SceneColor → engine ensures scene pass runs first.
// FullscreenPass handles: RT binding, fullscreen state, draw.
// ---------------------------------------------------------------------------
class PostProcessPass : public FullscreenPass {
public:
    const char* name() const override { return "postprocess"; }
    int executionOrder() const override { return 100; }
    PassRole passRole() const override { return PassRole::FinalComposite; }

    const ResourceDecl* resourceDecls() const override {
        static const ResourceDecl d[] = {
            { ResourceId::SceneColor, ResourceDecl::READ }
        };
        return d;
    }
    int resourceDeclCount() const override { return 1; }

    bool init(ShaderCache& cache, ShaderFeatureSet features, RenderTargetHandle scene_rt) {
        scene_rt_ = scene_rt;

        // Uber-compatible vertex shader: dual-path for GL2 (VBO) and GL3+ (gl_VertexID)
        // Uses ATTR_IN/VS_OUT macros from ShaderCache preamble
        const char* vs =
            "#ifdef GLSL_120\n"
            "ATTR_IN vec3 a_pos;\n"
            "#endif\n"
            "VS_OUT vec2 v_uv;\n"
            "void main() {\n"
            "#ifdef GLSL_120\n"
            "    v_uv = a_pos.xy * 0.5 + 0.5;\n"
            "    gl_Position = vec4(a_pos.xy, 0.0, 1.0);\n"
            "#else\n"
            "    vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0;\n"
            "    v_uv = p;\n"
            "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
            "#endif\n"
            "}\n";

        const char* fs =
            "FS_IN vec2 v_uv;\n"
            "uniform sampler2D u_scene;\n"
            "uniform float u_time;\n"
            "void main() {\n"
            "    vec3 color = COMPAT_TEX2D(u_scene, v_uv).rgb;\n"
            "    float gray = dot(color, vec3(0.299, 0.587, 0.114));\n"
            "    vec3 sepia = vec3(gray * 1.2, gray * 1.0, gray * 0.8);\n"
            "    float t = sin(u_time * 0.8) * 0.5 + 0.5;\n"
            "    color = mix(color, sepia, t);\n"
            "    float dist = length(v_uv - vec2(0.5));\n"
            "    float vig = smoothstep(0.8, 0.3, dist);\n"
            "    color *= vig;\n"
            "    FRAG_COLOR = vec4(color, 1.0);\n"
            "}\n";

        // compileInline: prepends #version + feature defines + compat macros
        ShaderProgram* prog = cache.compileInline("postprocess", vs, fs, features);
        if (!prog) return false;

        ub().init(prog);
        setShader(prog);
        setOutputScreen(0, 0);
        // No setQuad() → FullscreenPass uses ctx.drawFullscreen()
        // which picks GL2 quad or GL3+ triangle automatically

        return true;
    }

    void onExecute(PassContext& ctx, FrameData& fd, const SceneData&) override {
        ctx.renderer()->bindRenderTargetTexture(scene_rt_, 0);
        ub().set(U::SceneTex, 0);
        ub().set(U::Time, fd.time);
    }

private:
    RenderTargetHandle scene_rt_;
};

// ---------------------------------------------------------------------------
// Pipeline policy: ScenePass → FBO, PostProcessPass → default FB
// ---------------------------------------------------------------------------
class PostProcessPolicy : public PipelinePolicy {
public:
    PostProcessPolicy(RenderTargetHandle scene_rt, int w, int h)
        : scene_rt_(scene_rt), w_(w), h_(h) {}

    PassRTBinding routePass(const RenderPassBase& pass, int, int) const override {
        PassRTBinding b;
        if (pass.passRole() == PassRole::FinalComposite) {
            // PostProcessPass → render to screen (default FB)
            b.action = PassRTBinding::Unbind;
        }
        // ScenePass manages its own RT binding internally
        return b;
    }

private:
    RenderTargetHandle scene_rt_;
    int w_, h_;
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    AppConfig cfg;
    cfg.width = 800;
    cfg.height = 600;
    cfg.backend = RendererBackend::Auto;

    std::unique_ptr<RenderContext> ctx(createRenderContext(cfg));
    if (!ctx || !ctx->init(cfg)) {
        fprintf(stderr, "Failed to create render context\n");
        return 1;
    }

    std::unique_ptr<Renderer> renderer(createRenderer(cfg.backend));
    if (!renderer || !renderer->init(cfg.width, cfg.height)) {
        fprintf(stderr, "Failed to init renderer\n");
        ctx->shutdown();
        return 1;
    }

    int draw_w = 0, draw_h = 0;
    ctx->getDrawableSize(&draw_w, &draw_h);

    printf("Post-process demo: %s (GL %s)\n",
           renderer->getGPURenderer(), renderer->getGLVersion());

    // Create scene FBO
    RenderTargetHandle scene_rt = renderer->createRenderTarget(draw_w, draw_h);
    if (scene_rt == INVALID_RENDER_TARGET) {
        fprintf(stderr, "Failed to create scene FBO (GPU may not support render targets)\n");
        renderer->shutdown();
        ctx->shutdown();
        return 1;
    }

    // ShaderCache for uber inline shaders
    ShaderCache shader_cache;
    shader_cache.init(renderer.get());

    // Detect GL feature level for shader preamble
    ShaderFeatureSet features = renderer->isCoreProfile()
        ? static_cast<ShaderFeatureSet>(SF_GLSL_150)
        : static_cast<ShaderFeatureSet>(SF_GLSL_120);
    if (renderer->features<GL3Features>())
        features = static_cast<ShaderFeatureSet>(SF_GLSL_150);

    // Create passes
    ScenePass scene_pass;
    PostProcessPass pp_pass;
    if (!scene_pass.init(shader_cache, features, renderer.get(), scene_rt) ||
        !pp_pass.init(shader_cache, features, scene_rt)) {
        fprintf(stderr, "Failed to init passes\n");
        renderer->destroyRenderTarget(scene_rt);
        renderer->shutdown();
        ctx->shutdown();
        return 1;
    }

    // Build pipeline — engine sorts by ResourceDecl dependencies
    // ScenePass WRITES SceneColor, PostProcessPass READS SceneColor
    // → topological sort guarantees scene runs before invert
    std::vector<RenderPassBase*> passes;
    passes.push_back(&pp_pass);      // intentionally out of order!
    passes.push_back(&scene_pass);   // engine will sort correctly

    PostProcessPolicy policy(scene_rt, draw_w, draw_h);
    RenderPipeline pipeline;
    buildPipeline(pipeline, passes, policy, draw_w, draw_h);

    printf("Pipeline: %d nodes (passes added out of order, engine sorted by dependencies)\n",
           pipeline.nodeCount());

    // Render loop
    Timer timer;
    bool running = true;
    SDL_Event e;

    while (running) {
        while (ctx->pollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        float t = static_cast<float>(timer.elapsed_sec());
        float aspect = static_cast<float>(draw_w) / static_cast<float>(draw_h > 0 ? draw_h : 1);

        float cam_r = 3.0f, cam_h = 1.8f;
        Vec3 eye(sinf(t * 0.3f) * cam_r, cam_h, cosf(t * 0.3f) * cam_r);

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

        pipeline.execute(pass_ctx, fd, scene);

        ctx->swapBuffers();
    }

    // Cleanup
    scene_pass.cleanup(renderer.get());
    // pp_pass shader owned by shader_cache — no manual cleanup needed
    shader_cache.destroy();
    renderer->destroyRenderTarget(scene_rt);
    renderer->shutdown();
    ctx->shutdown();

    printf("Done.\n");
    return 0;
}
