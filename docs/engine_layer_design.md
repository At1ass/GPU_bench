# GPU Benchmark: Engine Layer Design Document

**Автор**: Claude (Opus 4.6) по запросу at1ass
**Дата**: 2026-03-29
**Статус**: Draft v2.0 (дополнен 2026-04-07 по результатам анализа кодовой базы)

## 1. Мотивация и цели

### Проблема

Demo-режим (~6100 строк, 23 render pass'а) напрямую вызывает ~280 методов
Renderer'а. Каждый pass вручную управляет GL-состоянием (depth test, cull face,
blending, depth mask), привязывает render target'ы, текстурные юниты, ставит
compute-барьеры. Это создает:

1. **Высокий порог входа** -- человек, собирающий сцену или эффект, должен знать
   GL state machine, а не только шейдеры и материалы
2. **Boilerplate** -- 5 повторяющихся паттернов копируются между pass'ами
3. **Хрупкость** -- забытый `setCullFace(true)` в одном pass'е ломает рендеринг
   следующего

### Цель

Добавить тонкий слой (**engine**) между Renderer и Demo, который:
- Скроет GL state management от pass-авторов
- Предоставит типизированные шаблоны для 3 основных паттернов pass'ов
- Не затронет Benchmark (продолжает использовать Renderer напрямую)
- Не затронет существующие абстракции (ShaderCache, UniformBlock, PipelineBuilder)

### Принципы

- **Не строим game engine** -- нет ECS, scene graph, asset pipeline
- **Ломаем обратную совместимость** свободно -- проект в разработке, пользователей нет
- **Опираемся на индустрию** -- паттерны из sokol_gfx (state groups), bgfx (sort keys),
  Frostbite (frame graph), Molecular Matters (command buckets)
- **C++11** -- без std::optional, std::variant, if constexpr
- **Минимальный scope** -- ~1500 строк нового кода, ~800 строк миграции pass'ов

## 2. Архитектура: текущая vs целевая

### 2.1 Текущая

```
┌─────────────────────────────────────────────────┐
│ gpu_demo executable                             │
│  DemoScene, DemoResources, DemoRunner, DemoUI   │
│  23 DemoRenderPass implementations              │
│  ShaderCache, ShaderProgram, UniformBlock        │
│  PipelineBuilder, ResourceDecl, FrameData       │
│      │                                          │
│      │  ~280 прямых вызовов                     │
│      ▼                                          │
├─────────────────────────────────────────────────┤
│ gpubench_core.a                                 │
│  Renderer (GL2→GL3→GL4, GL3/GL4/Compute Features│
│  RenderContext, MeshGen, OBJ Loader             │
│  Platform (Logger, Timer, HWInfo, DataPath)      │
├─────────────────────────────────────────────────┤
│ imgui_lib.a                                     │
│  ImGui + SDL2/GL3 backends                      │
└─────────────────────────────────────────────────┘
```

### 2.2 Целевая

```
┌─────────────────────────────────────────────────────────────┐
│ gpu_demo executable                                         │
│  DemoScene, DemoResources, DemoRunner, DemoUI               │
│  23 pass'ов: мигрированы на FullscreenPass/GeometryPass/    │
│              ComputePass шаблоны (или оставлены как есть     │
│              для специальных случаев)                        │
│  ShaderCache, ShaderProgram, UniformBlock -- БЕЗ ИЗМЕНЕНИЙ  │
│  PipelineBuilder, ResourceDecl, FrameData -- БЕЗ ИЗМЕНЕНИЙ  │
├─────────────────────────────────────────────────────────────┤
│ gpubench_engine.a (НОВЫЙ -- convenience layer)              │
│  RenderState, PassContext, TextureSlots                     │
│  FullscreenPass, GeometryPass, ComputePass (шаблоны)        │
│  DrawList (сортировка + batch submit)                       │
│  StateCache (redundant GL call elimination)                 │
├─────────────────────────────────────────────────────────────┤
│ gpubench_core.a (БЕЗ ИЗМЕНЕНИЙ в API, +StateCache внутри)  │
│  Renderer, Features, RenderContext                          │
│  MeshGen, OBJ Loader, Platform                              │
├─────────────────────────────────────────────────────────────┤
│ imgui_lib.a (БЕЗ ИЗМЕНЕНИЙ)                                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ gpu_benchmark executable (НЕ ЗАВИСИТ от engine)             │
│  BenchRunner, StressRunner, 30 тестов, BenchUI              │
│  Использует gpubench_core.a НАПРЯМУЮ                        │
└─────────────────────────────────────────────────────────────┘
```

## 3. Новые сущности

### 3.1 RenderState -- декларативное GL-состояние

**Файл:** `src/engine/render_state.h`

**Мотивация:** Сейчас pass'ы вызывают 5-8 отдельных `r->set*()` методов
для настройки состояния. Порядок вызовов не важен, но забытый вызов --
источник багов. sokol_gfx решает это через Pipeline State Object,
мы -- через POD-структуру + атомарное применение.

```cpp
#pragma once

// Декларативное описание GL render state.
// Применяется атомарно через PassContext::applyState().
// Паттерн из sokol_gfx (sg_pipeline) адаптированный для GL 2.1+.
struct RenderState {
    bool depth_test;       // glEnable(GL_DEPTH_TEST)
    bool depth_write;      // glDepthMask()
    bool cull_face;        // glEnable(GL_CULL_FACE)
    bool blending;         // glEnable(GL_BLEND)
    bool additive_blend;   // GL_ONE + GL_ONE (vs SRC_ALPHA + 1-SRC_ALPHA)

    // Color mask для glColorMask(). По умолчанию все true.
    // depth_only() пресет ставит всё в false для shadow mapping.
    // GL2Renderer::setColorMask() уже существует, но pass'ы его не используют.
    bool color_mask_r, color_mask_g, color_mask_b, color_mask_a;

    // Polygon offset для shadow acne mitigation.
    // Стандартная практика Khronos: glEnable(GL_POLYGON_OFFSET_FILL)
    // + glPolygonOffset(factor, units). Сейчас не используется в кодовой базе
    // (0 вхождений glPolygonOffset), но подготовлено для оптимизации ShadowPass.
    bool polygon_offset;
    float offset_factor;
    float offset_units;

    // Предустановки для типичных сценариев
    static RenderState opaque() {
        return { true, true, true, false, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState transparent() {
        return { true, false, false, true, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState additive() {
        return { true, false, false, true, true,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState fullscreen() {
        return { false, false, false, false, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    static RenderState shadow() {
        return { true, true, true, false, false,
                 true, true, true, true,
                 false, 0.0f, 0.0f };
    }
    // Оптимизированный пресет для shadow pass: записывает только depth.
    // Отключение color writes экономит bandwidth на фрагментном этапе.
    static RenderState depth_only() {
        return { true, true, true, false, false,
                 false, false, false, false,
                 true, 1.1f, 4.0f };
    }
};
```

**Размер:** ~55 строк. Без виртуальных методов, без аллокаций. Тривиально копируемый.

### 3.2 TextureSlots -- конвенция текстурных юнитов

**Файл:** `src/engine/texture_slots.h`

**Мотивация:** Сейчас номера текстурных юнитов (0, 1, 3, 4, 5, 6) хардкодятся
в каждом pass'е без единого источника правды. Конфликт юнитов между pass'ами --
потенциальный баг. Паттерн зафиксированных binding points
из Khronos best practices.

```cpp
#pragma once

// Фиксированные текстурные юниты для демо-сцены.
// Шейдеры привязывают sampler'ы к этим номерам.
// Конвенция: юнит не меняется между pass'ами в пределах кадра.
namespace TexSlot {
    enum : int {
        Primary      = 0,   // основная текстура (albedo, scene color)
        Secondary    = 1,   // вторичная (noise, mask, fur mask)
        AO           = 2,   // ambient occlusion result
        ShadowMap    = 3,   // shadow depth map
        NormalMap    = 4,   // normal map
        SSRColor     = 5,   // SSR color snapshot
        SSRDepth     = 6,   // SSR depth snapshot
        PostSource   = 7,   // post-process source (ping-pong)
        Depth        = 8,   // глубина сцены (HDR depth для SSAO/GTAO/SSR/DoF/VolumetricFog)
        Fog          = 9,   // volumetric fog output texture
    };
}
```

**Текущее использование юнитов в pass'ах** (по результатам анализа 24 вызовов
`bindTextureUnit()` в `src/demo/passes/`):

| Юнит | Текущее назначение | Pass'ы |
|:----:|-------------------|--------|
| 0 | albedo, scene depth, fur texture | SSAO, GTAO, DoF, VolumetricFog, Bloom, CompositePass |
| 1 | noise, bloom mips, hdr_depth, fur mask | SSAO, HDRComposite, SSR, DoF |
| 2 | GTAO blur result | HDRComposite |
| 3 | shadow depth map | OpaquePass, WaterPass, FurPass, GrassPass, TessPass |
| 4 | normal map, SSR texture | OpaquePass, HDRComposite |
| 5 | SSR color snapshot, DoF texture | WaterPass, HDRComposite |
| 6 | SSR depth snapshot | WaterPass |
| 7 | (зарезервирован, не используется ни одним pass'ом) | -- |
| 8 | (новый: Depth) | -- |
| 9 | (новый: Fog) | -- |

**Замечание:** Юниты 0-1 перегружены -- hdr_depth_tex привязывается к юниту 0
в GTAO/VolumetricFog и к юниту 1 в SSR/DoF. Выделенный `Depth = 8` устраняет
эту неоднозначность при миграции pass'ов.

**Размер:** ~30 строк. Enum, не класс.

### 3.3 PassContext -- обертка взаимодействия pass↔renderer

**Файл:** `src/engine/pass_context.h`, `src/engine/pass_context.cpp`

**Мотивация:** Это ядро engine layer. Инкапсулирует три операции,
которые каждый pass делает вручную:
1. Привязка render target + viewport + clear
2. Привязка текстур на фиксированные юниты
3. Применение RenderState

Паттерн из sokol_gfx: `sg_begin_pass()` / `sg_apply_pipeline()` /
`sg_apply_bindings()`.

```cpp
#pragma once
#include "engine/render_state.h"
#include "engine/texture_slots.h"
#include "renderer/renderer.h"
#include "renderer/features.h"

class PassContext {
public:
    explicit PassContext(Renderer* r);

    // --- Frame lifecycle ---

    // Вызывается один раз в начале кадра из DemoScene::renderFrame().
    // Инициализирует features cache и сбрасывает StateCache.
    void beginFrame();

    // --- Render Target ---

    // Привязать RT для записи. viewport автоматически из размеров RT.
    // clear_color: если не nullptr, делает clear с указанным цветом.
    void beginRT(RenderTargetHandle rt, int w, int h,
                 const float* clear_color = nullptr);

    // Привязать default framebuffer (экран)
    void beginScreen(int w, int h, const float* clear_color = nullptr);

    // Завершить работу с RT (unbind)
    void endRT();

    // --- State ---

    // Атомарно применить группу GL-состояний.
    // Внутри использует StateCache для пропуска redundant вызовов.
    void applyState(const RenderState& state);

    // Per-object state override (для two_sided материалов)
    void setCullFace(bool enable);

    // --- Textures ---

    // Привязать текстуру к фиксированному юниту
    void bindTexture(int slot, TextureHandle tex);

    // Привязать color-текстуру RT к юниту
    void bindRTTexture(int slot, RenderTargetHandle rt);

    // --- Compute (GL4+) ---

    // Привязать image для compute load/store
    void bindImage(int unit, TextureHandle tex, bool read, bool write);

    // Dispatch compute с параметризованным барьером (см. 3.3.2 BarrierFlags).
    // По умолчанию Barrier_All -- безопасно. Точечная оптимизация по необходимости.
    void dispatch(int groups_x, int groups_y, int groups_z,
                  unsigned int barriers = Barrier_All);

    // Привязать SSBO к binding point
    void bindSSBO(BufferHandle buf, int binding);

    // Read-back из SSBO
    void readSSBO(BufferHandle buf, void* out, int offset, int size);

    // --- Accessors ---

    Renderer* renderer() { return r_; }
    GL3Features* gl3();    // кешированный features<GL3Features>()
    GL4Features* gl4();    // кешированный features<GL4Features>()
    ComputeFeatures* compute();  // кешированный features<ComputeFeatures>()

private:
    Renderer* r_;
    GL3Features* gl3_;          // кешируется при beginFrame()
    GL4Features* gl4_;
    ComputeFeatures* compute_;
    bool features_cached_;
    RenderTargetHandle current_rt_;

    void cacheFeatures();
};
```

#### 3.3.1 Lifecycle: per-frame singleton

**Проблема текущего дизайна (v1.0):** PassContext создавался на стеке в каждом
`execute()` вызове:
```cpp
void FullscreenPass::execute(Renderer* r, ...) {
    PassContext ctx(r);   // создается 23 раза за кадр
    ...
}
```

Это приводило к:
- **Потере features cache** между pass'ами (23 повторных `queryFeature()` --
  23 виртуальных dispatch'а вместо 1)
- **Невозможности StateCache tracking** между pass'ами: OpaquePass ставит
  `depth_test=true`, следующий SSAOPass тоже ставит `depth_test=true` --
  без per-frame PassContext StateCache не знает, что depth_test уже true

**Решение (v2.0):** PassContext создаётся один раз в `DemoScene::renderFrame()`:

```cpp
// DemoScene::renderFrame()
PassContext ctx(renderer_);
ctx.beginFrame();   // features cache + StateCache reset
pipeline_.execute(ctx, fd, res_, config_, scene_data_);
```

**Изменение сигнатуры execute():**
```cpp
// Было (v1.0):
void execute(Renderer* r, FrameData& fd, const TierResourceView& res,
             const DemoTierConfig& cfg, const SceneData& scene) override;

// Стало (v2.0):
void execute(PassContext& ctx, FrameData& fd, const TierResourceView& res,
             const DemoTierConfig& cfg, const SceneData& scene) override;
```

Renderer доступен через `ctx.renderer()`. Все 23 pass'а требуют механической
правки `r->` → `ctx.renderer()->` (или сокращение `auto* r = ctx.renderer()`).

**Обоснование:** StateCache внутри Renderer (как в v1.0) работает для простых
состояний, но texture binding cache и FBO cache требуют per-frame tracking.
PassContext singleton per frame -- единственный способ гарантировать корректность
кеша между pass'ами без изменения API Renderer'а.

#### 3.3.2 BarrierFlags -- параметризация compute-барьеров

**Мотивация:** Текущие compute pass'ы используют разные паттерны барьеров:
- `ssr_pass.cpp:66` -- только `imageMemoryBarrier()`
- `compute_particles_pass.cpp:50` -- только `computeMemoryBarrier()`
- `gtao_pass.cpp:49,69` -- только `imageMemoryBarrier()`
- `auto_exposure_pass.cpp:44,57` -- только `computeMemoryBarrier()`

Dispatch с Barrier_All (v1.0) вызывал ОБА барьера всегда -- корректно,
но избыточно для pass'ов, которые используют только один тип.

```cpp
enum BarrierFlags : unsigned int {
    Barrier_None    = 0,
    Barrier_Image   = 1 << 0,  // GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
    Barrier_Texture = 1 << 1,  // GL_TEXTURE_FETCH_BARRIER_BIT
    Barrier_SSBO    = 1 << 2,  // GL_SHADER_STORAGE_BARRIER_BIT
    Barrier_All     = Barrier_Image | Barrier_Texture | Barrier_SSBO,
};
```

**Реализация dispatch():**

```cpp
void PassContext::dispatch(int gx, int gy, int gz, unsigned int barriers) {
    cacheFeatures();
    if (!compute_) return;
    compute_->dispatchCompute(gx, gy, gz);
    if (barriers == Barrier_None) return;
    if ((barriers & Barrier_Image) && gl4_)
        gl4_->imageMemoryBarrier();
    if (barriers & (Barrier_Texture | Barrier_SSBO))
        compute_->computeMemoryBarrier();
}
```

**Реализация beginRT():**

```cpp
void PassContext::beginRT(RenderTargetHandle rt, int w, int h,
                          const float* clear_color) {
    r_->bindRenderTarget(rt);
    r_->setViewport(0, 0, w, h);
    if (clear_color)
        r_->clear(clear_color[0], clear_color[1],
                  clear_color[2], clear_color[3]);
    current_rt_ = rt;
}
```

**Размер:** ~130 строк header + ~160 строк impl = ~290 строк.

### 3.4 Pass-шаблоны: FullscreenPass, GeometryPass, ComputePass

**Файлы:** `src/engine/fullscreen_pass.h`, `src/engine/geometry_pass.h`,
`src/engine/compute_pass.h`

**Мотивация:** Из 23 pass'ов, 20 следуют одному из трех паттернов.
Каждый паттерн имеет фиксированный boilerplate (state setup, RT binding,
object iteration, barrier placement). Шаблоны инкапсулируют boilerplate,
оставляя автору только специфичную логику.

Паттерн из Frostbite FrameGraph: typed pass с lambda для setup/execute.
Адаптация для C++11: наследование вместо lambda (нет generic lambda в C++11).

#### 3.4.1 FullscreenPass

Покрывает: bloom extract, bloom blur, SSAO, SSAO blur, composite,
HDR composite, volumetric fog (7 pass'ов).

```cpp
#pragma once
#include "demo/render_pass.h"
#include "engine/pass_context.h"
#include "demo/uniform_block.h"

// Шаблон для pass'а "fullscreen quad": input текстуры → шейдер → output RT.
// Автоматически: bind RT, set viewport, set fullscreen state, draw quad.
// Автор реализует: inputs(), uniforms().

class FullscreenPass : public DemoRenderPass {
public:
    virtual void setup(const TierResourceView& res) = 0;
    virtual void inputs(PassContext& ctx,
                        const TierResourceView& res,
                        const FrameData& fd) = 0;
    virtual void uniforms(UniformBlock& ub,
                          const FrameData& fd,
                          const DemoTierConfig& cfg) {}

    void execute(PassContext& ctx, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader);
    void setQuad(MeshHandle quad);
    void setOutputRT(RenderTargetHandle rt, int w, int h);
    void setOutputScreen(int w, int h);
    void setClearColor(float r, float g, float b, float a);

private:
    UniformBlock ub_;
    MeshHandle quad_;
    RenderTargetHandle output_rt_;
    int out_w_, out_h_;
    bool has_clear_;
    float clear_[4];
    bool to_screen_;
};
```

**execute() реализация** (~25 строк):
```cpp
void FullscreenPass::execute(PassContext& ctx, FrameData& fd,
                             const TierResourceView& res,
                             const DemoTierConfig& cfg,
                             const SceneData& scene) {
    if (to_screen_)
        ctx.beginScreen(out_w_, out_h_, has_clear_ ? clear_ : nullptr);
    else
        ctx.beginRT(output_rt_, out_w_, out_h_, has_clear_ ? clear_ : nullptr);

    ctx.applyState(RenderState::fullscreen());
    inputs(ctx, res, fd);
    ub_.use();
    uniforms(ub_, fd, cfg);
    ctx.renderer()->drawMesh(quad_);
    ctx.endRT();
}
```

#### 3.4.2 GeometryPass

Покрывает: opaque, shadow, water, torch, tessellation (5 pass'ов).

**Замечание v2.0:** OpaquePass уже реализует frustum culling через
`sphereInFrustum()` (opaque_pass.cpp:95). GeometryPass::objectFilter()
предоставляет это как default behaviour для всех geometry pass'ов.

```cpp
#pragma once
#include "demo/render_pass.h"
#include "engine/pass_context.h"
#include "demo/uniform_block.h"
#include "demo/scene_data.h"
#include "demo/demo_utils.h"

class GeometryPass : public DemoRenderPass {
public:
    virtual void setup(const TierResourceView& res) = 0;

    virtual void sceneSetup(UniformBlock& ub, PassContext& ctx,
                            const FrameData& fd,
                            const TierResourceView& res,
                            const DemoTierConfig& cfg) = 0;

    // По умолчанию: frustum culling. Переопределяется для фильтрации
    // (например, WaterPass: return obj.is_water).
    virtual bool objectFilter(const SceneObject& obj,
                              const FrameData& fd) {
        return sphereInFrustum(fd.frustum,
                               obj.bounds_center, obj.bounds_radius);
    }

    virtual void perObject(UniformBlock& ub, PassContext& ctx,
                           const SceneObject& obj) {
        ub.set(U::Model, obj.transform);
        ub.set(U::MatColor, obj.mat.color_a);
        ub.set(U::MatColorB, obj.mat.color_b);
        ub.set(U::Metallic, obj.mat.metallic);
        ub.set(U::Roughness, obj.mat.roughness);
        ub.set(U::NoiseScale, obj.mat.noise_scale);
        ub.set(U::NoiseIntensity, obj.mat.noise_intensity);
        ub.set(U::WarpStrength, obj.mat.warp_strength);
        ub.set(U::DetailScale, obj.mat.detail_scale);
        ub.set(U::ProcTex, static_cast<float>(
                   static_cast<int>(obj.mat.proc_type)));
        ub.set(U::Specular, obj.mat.specular);
    }

    virtual const std::vector<SceneObject>* objectList(
            const SceneData& scene) {
        return scene.opaque_objects;
    }

    void execute(PassContext& ctx, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader);
    void setOutputRT(RenderTargetHandle rt, int w, int h);
    void setState(const RenderState& state);
    void setClearColor(float r, float g, float b, float a);
    UniformBlock& ub() { return ub_; }

private:
    UniformBlock ub_;
    RenderTargetHandle output_rt_;
    int out_w_, out_h_;
    RenderState state_;
    bool has_clear_;
    float clear_[4];
};
```

**execute() реализация** (~35 строк):
```cpp
void GeometryPass::execute(PassContext& ctx, FrameData& fd,
                           const TierResourceView& res,
                           const DemoTierConfig& cfg,
                           const SceneData& scene) {
    ctx.beginRT(output_rt_, out_w_, out_h_, has_clear_ ? clear_ : nullptr);
    ctx.applyState(state_);

    ub_.use();
    sceneSetup(ub_, ctx, fd, res, cfg);

    const std::vector<SceneObject>* objects = objectList(scene);
    if (!objects) { ctx.endRT(); return; }

    for (const SceneObject& obj : *objects) {
        if (!objectFilter(obj, fd)) continue;

        perObject(ub_, ctx, obj);

        if (obj.mat.two_sided) ctx.setCullFace(false);
        ctx.renderer()->drawMesh(obj.mesh);
        if (obj.mat.two_sided) ctx.setCullFace(state_.cull_face);
    }

    ctx.endRT();
}
```

#### 3.4.3 ComputePass

Покрывает: GTAO, bloom compute (down+up), SSR, DoF, auto-exposure,
compute particles (7 pass'ов).

```cpp
#pragma once
#include "demo/render_pass.h"
#include "engine/pass_context.h"
#include "demo/uniform_block.h"

class ComputePassBase : public DemoRenderPass {
public:
    virtual void setup(const TierResourceView& res) = 0;

    virtual void bind(PassContext& ctx, UniformBlock& ub,
                      const TierResourceView& res,
                      const FrameData& fd,
                      const DemoTierConfig& cfg) = 0;

    virtual void workgroups(const FrameData& fd,
                            const DemoTierConfig& cfg,
                            int& gx, int& gy, int& gz) = 0;

    // По умолчанию Barrier_All. Переопределяется для оптимизации:
    // GTAO → Barrier_Image (image store → texture read)
    // ComputeParticles → Barrier_SSBO (SSBO write → SSBO read)
    virtual unsigned int barrierFlags() const { return Barrier_All; }

    QueueType queueType() const override { return QueueType::Compute; }

    void execute(PassContext& ctx, FrameData& fd,
                 const TierResourceView& res,
                 const DemoTierConfig& cfg,
                 const SceneData& scene) override;

protected:
    void setShader(ShaderProgram* shader);

private:
    UniformBlock ub_;
};
```

**execute() реализация** (~20 строк):
```cpp
void ComputePassBase::execute(PassContext& ctx, FrameData& fd,
                              const TierResourceView& res,
                              const DemoTierConfig& cfg,
                              const SceneData& scene) {
    if (!ctx.compute()) return;

    ub_.use();
    bind(ctx, ub_, res, fd, cfg);

    int gx = 0, gy = 0, gz = 0;
    workgroups(fd, cfg, gx, gy, gz);
    ctx.dispatch(gx, gy, gz, barrierFlags());
}
```

### 3.5 StateCache -- устранение избыточных GL-вызовов

**Файл:** `src/engine/state_cache.h`

**Мотивация:** Рендерер сейчас не кеширует GL-состояние -- каждый
`setDepthTest(true)` делает `glEnable(GL_DEPTH_TEST)` даже если depth test
уже включен (`gl2_renderer.cpp:885` -- прямой вызов без guard).
При ~66 вызовах state-setters за кадр (измерено), ~50% избыточны.

Паттерн из sokol_gfx (shadow state), Godot (GL state cache).
Правило: **никогда не использовать glGet* для проверки** -- только CPU shadow.

```cpp
#pragma once

// Shadow state cache для GL.
// Отсекает redundant glEnable/glDisable/glBind* вызовы.
// Правило: CPU shadow state всегда synchronized с GPU.
// Правило: reset() вызывается при init, после resetState(), и в beginFrame().
class StateCache {
public:
    void reset();

    // Возвращает true если состояние изменилось (нужен GL-вызов)
    bool setDepthTest(bool enable);
    bool setDepthMask(bool write);
    bool setCullFace(bool enable);
    bool setBlending(bool enable);
    bool setBlendingAdditive(bool enable);
    bool setColorMask(bool r, bool g, bool b, bool a);
    bool useProgram(unsigned int program);
    bool bindFramebuffer(unsigned int fbo);

    // Texture unit cache: запоминает какая текстура привязана к юниту.
    // Предотвращает повторный glActiveTexture + glBindTexture для одной пары.
    // Из анализа: GTAO привязывает hdr_depth_tex к unit 0 дважды (строки 30, 59).
    bool bindTexture(int unit, unsigned int tex_id);

private:
    enum : int { UNKNOWN = -1 };
    int depth_test_;
    int depth_mask_;
    int cull_face_;
    int blend_;
    int blend_additive_;
    int color_mask_[4];
    unsigned int current_program_;
    unsigned int current_fbo_;

    static const int MAX_TEXTURE_UNITS = 10; // соответствует TexSlot::Fog + 1
    unsigned int bound_textures_[MAX_TEXTURE_UNITS];
};
```

**Интеграция в Renderer:** StateCache встраивается в GL2Renderer как
private member. Методы `setDepthTest()` и др. проверяют кеш перед GL-вызовом.
Это **не breaking change** -- API Renderer'а не меняется.

**Размер:** ~80 строк header + ~110 строк impl = ~190 строк.

### 3.6 DrawList (Phase 3) -- сортировка draw call'ов

**Файл:** `src/engine/draw_list.h`, `src/engine/draw_list.cpp`

**Мотивация:** Для текущих 25-28 объектов сортировка не критична.
Но при росте сцены (>100 объектов) state changes станут bottleneck.
DrawList минимизирует переключения шейдеров и материалов через
sort key (паттерн из bgfx и Molecular Matters CommandBucket).

Иерархия стоимости state changes (DOOM 2016, Christer Ericson):
RT switch (~100x) > Program (~10x) > Texture (~3x) > Uniform (~1x) > VBO (~1x).
Sort key отражает эту иерархию в старших битах.

**Реализуется во Phase 3** после миграции pass'ов. Описан здесь для полноты.

```cpp
#pragma once
#include "renderer/renderer.h"
#include "demo/scene_data.h"
#include "demo/uniform_block.h"
#include "engine/pass_context.h"

struct DrawCmd {
    uint32_t sort_key;    // MSB: shader(8) | material(8) | depth(16) : LSB
    const SceneObject* obj;

    bool operator<(const DrawCmd& o) const { return sort_key < o.sort_key; }
};

class DrawList {
public:
    void clear();
    void push(const SceneObject& obj, uint8_t shader_id,
              uint8_t material_id, float depth);
    void sortAndSubmit(PassContext& ctx, UniformBlock& ub);

private:
    std::vector<DrawCmd> cmds_;
};
```

**Размер:** ~100 строк header + ~80 строк impl.

### 3.7 PassRole -- типизированная роль pass'а в пайплайне

**Файл:** расширение `src/demo/render_pass.h`

**Мотивация:** `pipeline_builder.cpp` использует 3 strcmp() вызова
(строки 244, 250, 292) для определения special-case обработки pass'ов:
`"scene_to_fbo"`, `"hdr_composite"`, `"composite"`. Это хрупкое строковое
сравнение -- опечатка в `name()` ломает пайплайн незаметно.

```cpp
enum class PassRole {
    Default,        // обычный pass, без special handling
    SceneContainer, // scene_to_fbo: управляет своим FBO
    FinalComposite, // composite/hdr_composite: рендерит в dest framebuffer
};

// В DemoRenderPass:
virtual PassRole passRole() const { return PassRole::Default; }
```

**Замена в pipeline_builder.cpp:**
```cpp
// Было:
if (strcmp(pname, "scene_to_fbo") == 0) { ... }
if (strcmp(pname, "hdr_composite") == 0) { ... }
if (strcmp(pname, "composite") == 0) { ... }

// Стало:
if (pass->passRole() == PassRole::SceneContainer) { ... }
if (pass->passRole() == PassRole::FinalComposite) {
    // HDR vs non-HDR определяется наличием ResourceId::HDRColor
    // в resourceDecls(), а не по имени pass'а.
    ...
}
```

**Размер:** ~15 строк в render_pass.h, ~10 строк diff в pipeline_builder.cpp.

### 3.8 light_vp: инициализация в buildFrameData()

**Проблема:** `computeLightMatrix(fd)` вызывается ТОЛЬКО в
`ShadowPass::execute()` (`shadow_pass.cpp:20`). Это единственное
место, где `fd.light_vp` получает значение. 5 pass'ов читают `fd.light_vp`
через guard `if (fd.has_shadows)`.

Текущий код безопасен по совпадению:
1. PipelineBuilder гарантирует, что ShadowPass выполняется перед
   OpaquePass (через ResourceDecl зависимость ShadowMap WRITE→READ)
2. `fd.has_shadows` == false → light_vp не читается

Но это хрупкая конструкция:
- Если pass будет использовать light_vp без проверки has_shadows --
  неинициализированные данные
- Если ShadowPass будет disabled (debug override) -- light_vp мусор
- `computeLightMatrix()` -- pure function от `fd.sun_dir`, не от shadow RT

**Решение:** Перенести вызов в `buildFrameData()`:

```cpp
FrameData DemoScene::buildFrameData(...) {
    ...
    fd.sun_dir = SUN_DIR_RAW.normalized();
    computeLightMatrix(fd);  // ВСЕГДА вычисляется
    ...
}
```

`ShadowPass::execute()` убирает `computeLightMatrix(fd)` (строка 20),
оставляя только `ub_.set(U::LightVP, fd.light_vp)`.

**Стоимость:** `computeLightMatrix()` содержит 1 normalize, 1 cross product,
2 matrix multiply (`demo_utils.h:62-70`). Вызывается 1 раз за кадр. ≈ 0.

**Фаза реализации:** Phase 0 (подготовка). Не зависит от engine layer.

### 3.9 UniformBlock: текущее поведение и интеграция

**Текущее поведение:** `UniformBlock::set()` немедленно вызывает
`glUniform*` через `ShaderProgram::set*_raw()` (`uniform_block.h:29-53`).
Нет буферизации, нет отложенного применения. Uniform locations кешируются
(lazy resolution при первом `set()`).

**Решение: оставить без изменений.**

Обоснование:
- 350 `glUniform*` за кадр (25 объектов × ~14 uniform'ов) -- допустимая
  нагрузка для CPU-side. Каждый вызов ~50-100 ns. Итого: ~35 мкс.
- Uniform'ы имеют уникальные значения per-object (Model matrix,
  material params) -- кеширование по значению даст минимальный эффект.
- **UBO migration path (Phase 4+):**
  1. Собрать per-object data в структуру (pad to std140)
  2. Upload одним `glBufferSubData`
  3. `glBindBufferRange` per object
  Это ~4x сокращение GL-вызовов (350 → ~80), но требует std140 layout
  в шейдерах и struct padding. GL3Features уже имеет `createUBO`/`updateUBO`/
  `bindUBO` (`features.h:45-48`).

**Интеграция:** Pass-шаблоны (FullscreenPass, GeometryPass, ComputePass)
владеют UniformBlock как private member. PassContext НЕ управляет
uniform'ами -- это ответственность pass-автора через виртуальные
методы (`uniforms()`, `sceneSetup()`, `perObject()`).

## 4. Изменения в существующих модулях

### 4.1 Renderer (gpubench_core) -- внутренняя оптимизация

**Что меняется:** Добавляется StateCache внутрь GL2Renderer.
**Что НЕ меняется:** Публичный API Renderer'а. Все 46 методов остаются.

```diff
 class GL2Renderer : public Renderer {
+    StateCache state_cache_;
 public:
     void setDepthTest(bool enable) override {
+        if (!state_cache_.setDepthTest(enable)) return;  // redundant
         if (enable) glEnable(GL_DEPTH_TEST);
         else glDisable(GL_DEPTH_TEST);
     }
```

Benchmark продолжает вызывать те же методы. StateCache -- чистая оптимизация,
прозрачная для вызывающего кода.

### 4.2 Demo passes -- миграция на шаблоны

**Стратегия:** постепенная, pass за pass'ом. Старый `DemoRenderPass::execute()`
и новые шаблоны сосуществуют -- PipelineBuilder не знает разницы.

**Матрица миграции:**

| Pass | Текущий паттерн | Целевой шаблон | Сложность | Примечания |
|------|----------------|----------------|-----------|------------|
| SkyPass | Geometry (1 mesh) | GeometryPass (custom objectList) | Низкая | |
| ShadowPass | Geometry (loop) | GeometryPass | Низкая | light_vp → buildFrameData (см. 3.8) |
| OpaquePass | Geometry (loop) | GeometryPass | Средняя | frustum culling уже активен; 38 set() + 14 per-object uniforms |
| FurPass | Geometry (instanced) | Специальный (оставить как есть) | -- | 25+ uniform set calls |
| ParticlePass | Geometry (billboards) | GeometryPass | Низкая | |
| TorchPass | Geometry (point lights) | GeometryPass (custom objectList) | Низкая | |
| GrassInstancedPass | Instanced draw | Специальный (оставить как есть) | -- | |
| SSAOPass | Fullscreen | FullscreenPass | Низкая | 6 GL calls, 3 tex units |
| SSAOBlurPass | Fullscreen | FullscreenPass | Низкая | |
| BloomExtractPass | Fullscreen | FullscreenPass | Низкая | |
| BloomBlurPass | Fullscreen (ping-pong) | Специальный (2x fullscreen) | Средняя | 3 sub-passes |
| CompositePass | Fullscreen | FullscreenPass | Низкая | |
| HDRCompositePass | Fullscreen + SSBO read | FullscreenPass + override | Средняя | |
| GTAOPass | Compute | ComputePassBase | Низкая | 2 dispatches, 3 image bindings |
| GTAOBlurPass | Compute | ComputePassBase | Низкая | |
| BloomComputePass | Compute (mip chain) | Специальный (multi-dispatch) | Средняя | |
| AutoExposurePass | Compute (2-stage) | Специальный (2x compute) | Средняя | |
| SSRPass | Compute | ComputePassBase | Низкая | |
| DoFPass | Compute | ComputePassBase | Низкая | |
| ComputeParticlesPass | Compute | ComputePassBase | Низкая | |
| ComputeParticlesDrawPass | Geometry (SSBO read) | Специальный (оставить как есть) | -- | |
| VolumetricFogPass | Fullscreen | FullscreenPass | Низкая | |
| WaterPass | Geometry (water filter) | GeometryPass (custom objectFilter: is_water) | Средняя | Почти идентичен OpaquePass |
| TessellatedModelPass | Geometry (tessellation) | Специальный (оставить как есть) | -- | |
| SceneToFBOPass | Container | Оставить как есть (контейнер sub-pass'ов) | -- | |
| SSRCopyPass | Special (copyImageSubData) | Оставить как есть | -- | |

**Итого:** 15 pass'ов мигрируют на шаблоны, 8 остаются специальными.

### 4.3 demo_export.cpp -- удалить мёртвый include

```diff
- #include "bench/results.h"    // не используется
```

### 4.4 Что НЕ меняется (Phase 0-3)

- **ShaderCache** -- компиляция шейдерных перестановок
- **ShaderProgram** -- RAII обёртка + lazy uniform cache
- **UniformBlock** -- type-safe uniform access через X-macro (см. 3.9)
- **PipelineBuilder** -- топологическая сортировка pass'ов (+ замена strcmp, см. 3.7)
- **ResourceDecl / ResourceId** -- декларации зависимостей
- **FrameData** -- per-frame shared state
- **SceneData / SceneObject / MaterialDef** -- CPU scene data
- **DemoResources** -- без изменений в Phase 0-3 (декомпозиция планируется в Phase 4+, см. 4.5)
- **TierResourceView** -- без изменений в Phase 0-3 (декомпозиция T4 в Phase 4+, см. 4.6)
- **DemoRunner / DemoCamera** -- orchestration и камера
- **BenchRunner / StressRunner / все 30 тестов** -- бенчмарк без изменений

### 4.5 DemoResources: предлагаемая декомпозиция (Phase 4+)

`DemoResources` (`demo_resources.h`) содержит 68 полей (shader programs,
mesh handles, texture handles, render targets, SSBOs) с паттерном
дублирования: `ShaderProgram xxx_shader_` (legacy ownership) +
`ShaderProgram* xxx_cache_` (non-owning from ShaderCache).
Это legacy-паттерн, который ShaderCache постепенно замещает.

**Предлагаемая декомпозиция НЕ входит в Phase 0-3 engine layer.
Документируется как tech debt:**

```
struct CoreResources   { sky, island, fur, particle shaders + meshes }
struct ShadowResources { shader, RT, depth_tex, map_size }
struct BloomResources  { 3 shaders, quad, 3 RTs, strength }
struct SSAOResources   { 2 shaders, 2 RTs, noise/depth tex }
```

`TierResourceView` уже структурирован правильно (sub-structs Shadow, Bloom,
SSAO). `DemoResources` -- нет. Синхронизация: DemoResources повторяет
группировку TierResourceView для упрощения `viewForTier()`.

**Оценка объёма:** ~200 строк рефакторинга, 0 изменений в семантике.
`prepare()` уже имеет приватные методы, группирующие логику.

### 4.6 TierResourceView::T4: предлагаемая декомпозиция (Phase 4+)

`TierResourceView::T4` (`tier_resource_view.h:68-127`): 30+ полей без
sub-группировки. В отличие от Shadow/Bloom/SSAO, которые имеют именованные
sub-structs, T4 -- один монолит.

**Предлагаемая структура:**

```cpp
struct T4 {
    struct HDR {
        RenderTargetHandle scene_rt, bright_rt;
        TextureHandle depth_tex, color_tex;
        RenderTargetHandle fog_rt;
        ShaderProgram* tone_map_shader;
        ShaderProgram* volumetric_fog_shader;
    } hdr;

    struct GTAO {
        ShaderProgram* shader, *blur_shader;
        TextureHandle tex, blur_tex;
    } gtao;

    struct ComputeBloom {
        ShaderProgram* down_compute, *up_compute;
        static const int MIP_COUNT = 6;
        TextureHandle mips[MIP_COUNT];
    } bloom;

    struct AutoExposure {
        ShaderProgram* histogram_shader, *exposure_shader;
        BufferHandle histogram_ssbo, exposure_ssbo;
    } exposure;

    struct SSR {
        ShaderProgram* shader;
        TextureHandle tex, color_snapshot, depth_snapshot;
    } ssr;

    struct DoF {
        ShaderProgram* shader;
        TextureHandle tex;
    } dof;

    // Оставшееся (без sub-struct):
    ShaderProgram* tess_shader;
    ShaderProgram* compute_particle_shader;
    ShaderProgram* particle_render_shader;
    BufferHandle particle_ssbo;
    int compute_particle_count;
    static const int PUDDLE_COUNT = 3;
    MeshHandle puddle_meshes[PUDDLE_COUNT];
};
```

**Breaking change:** `res.t4.gtao_shader` → `res.t4.gtao.shader`.
23 pass'а + `DemoResources::viewForTier()` потребуют механической правки.
**Рекомендация:** делать одновременно с Phase 2 миграцией pass'ов,
чтобы не делать двойную правку.

## 5. Структура сборки (CMake)

### 5.1 Новый target: gpubench_engine

```cmake
# Engine convenience layer (between core and demo)
add_library(gpubench_engine STATIC
    src/engine/pass_context.cpp
    src/engine/fullscreen_pass.cpp
    src/engine/geometry_pass.cpp
    src/engine/compute_pass.cpp
    src/engine/state_cache.cpp
)

target_include_directories(gpubench_engine PUBLIC src/)
target_link_libraries(gpubench_engine PUBLIC gpubench_core)
```

### 5.2 Иерархия зависимостей

```
imgui_lib.a
    ↑
gpubench_core.a  (renderer, platform, geometry)
    ↑           ↑
    │    gpubench_engine.a  (pass templates, state cache, pass context)
    │           ↑
    │      gpu_demo  (demo scene, passes, runner, UI)
    │
gpu_benchmark  (bench runner, 30 tests, bench UI)
```

**Ключевое:** `gpu_benchmark` **НЕ** линкуется с `gpubench_engine`.
Бенчмарк остаётся лёгким, без engine overhead.

### 5.3 UI

BenchUI и DemoUI остаются в своих исполняемых файлах.
Обе используют ImGui через `imgui_lib.a` (уже часть `gpubench_core`).
Engine layer не содержит UI-кода.

### 5.4 Полная структура файлов engine

```
src/engine/
├── render_state.h          # ~55 строк  -- POD struct + presets (incl. color_mask, polygon_offset)
├── texture_slots.h         # ~30 строк  -- enum фиксированных юнитов (0-9)
├── state_cache.h           # ~80 строк  -- GL state shadow cache (incl. texture units)
├── state_cache.cpp         # ~110 строк
├── pass_context.h          # ~130 строк -- RT/state/texture/compute wrapper + BarrierFlags
├── pass_context.cpp        # ~160 строк
├── fullscreen_pass.h       # ~70 строк  -- шаблон fullscreen quad pass
├── fullscreen_pass.cpp     # ~40 строк
├── geometry_pass.h         # ~90 строк  -- шаблон geometry draw pass
├── geometry_pass.cpp       # ~50 строк
├── compute_pass.h          # ~60 строк  -- шаблон compute dispatch pass
├── compute_pass.cpp        # ~30 строк
└── draw_list.h             # ~80 строк  -- Phase 3 (sort-based draw)
    draw_list.cpp           # ~80 строк  -- Phase 3
```

**Итого Phase 1:** ~905 строк нового кода.
**Phase 3 (DrawList):** +160 строк.

## 6. Зависимости между модулями

```
engine/render_state.h      → (ничего, self-contained POD)
engine/texture_slots.h     → (ничего, enum)
engine/state_cache.h       → (ничего, self-contained)
engine/state_cache.cpp     → state_cache.h (только)

engine/pass_context.h      → renderer/renderer.h, renderer/features.h,
                             engine/render_state.h, engine/texture_slots.h
engine/pass_context.cpp    → pass_context.h, engine/state_cache.h

engine/fullscreen_pass.h   → demo/render_pass.h, engine/pass_context.h,
                             demo/uniform_block.h
engine/fullscreen_pass.cpp → fullscreen_pass.h

engine/geometry_pass.h     → demo/render_pass.h, engine/pass_context.h,
                             demo/uniform_block.h, demo/scene_data.h,
                             demo/demo_utils.h (sphereInFrustum)
engine/geometry_pass.cpp   → geometry_pass.h

engine/compute_pass.h      → demo/render_pass.h, engine/pass_context.h,
                             demo/uniform_block.h
engine/compute_pass.cpp    → compute_pass.h
```

**Замечание:** engine зависит от `demo/render_pass.h`, `demo/uniform_block.h`,
`demo/scene_data.h`. Это сознательное решение -- эти хедеры определяют
интерфейсы, которые engine расширяет. В будущем, если потребуется полная
независимость, эти определения можно вынести в `engine/` или `core/`.

## 7. Plan итераций с чекпоинтами

### Phase 0: Подготовка (1 сессия)

**Задачи:**
- [ ] Создать `src/engine/` директорию
- [ ] Добавить `gpubench_engine` target в CMakeLists.txt
- [ ] Удалить мёртвый `#include "bench/results.h"` из `demo_export.cpp`
- [ ] Перенести `computeLightMatrix()` из ShadowPass в `buildFrameData()` (см. 3.8)
- [ ] Добавить `PassRole` enum в `render_pass.h`, обновить `pipeline_builder.cpp` (см. 3.7)
- [ ] Убедиться что оба executable собираются и работают

**Чекпоинт 0:** `./scripts/build.sh native` -- оба бинарника собираются.
Engine library пустая (placeholder .cpp). light_vp вычисляется в buildFrameData().
PipelineBuilder использует PassRole вместо strcmp. Никакого визуального изменения.

---

### Phase 1a: Фундамент engine (1 сессия)

**Задачи:**
- [ ] Реализовать `RenderState` с color_mask, polygon_offset, depth_only() (render_state.h)
- [ ] Реализовать `TextureSlots` с Depth=8, Fog=9 (texture_slots.h)
- [ ] Реализовать `StateCache` с texture unit и color_mask кешем (state_cache.h/cpp)
- [ ] Реализовать `PassContext` с per-frame lifecycle и BarrierFlags (pass_context.h/cpp)
- [ ] Изменить сигнатуру `DemoRenderPass::execute()`: `Renderer* r` → `PassContext& ctx`
- [ ] Интегрировать StateCache в GL2Renderer (internal, не меняя API)

**Чекпоинт 1a:** Компиляция engine library. Все 23 pass'а обновлены на новую
сигнатуру execute(). PassContext создаётся в renderFrame() как singleton.
StateCache отсекает повторные GL-вызовы. Демо и бенчмарк работают без изменений.

---

### Phase 1b: Pass-шаблоны (1 сессия)

**Задачи:**
- [ ] Реализовать `FullscreenPass` (fullscreen_pass.h/cpp)
- [ ] Реализовать `GeometryPass` (geometry_pass.h/cpp)
- [ ] Реализовать `ComputePassBase` (compute_pass.h/cpp)
- [ ] Написать один пример каждого типа для проверки (не заменяя существующие)

**Чекпоинт 1b:** Три тестовых pass'а работают через шаблоны.
Можно добавить их в PipelineBuilder рядом с существующими pass'ами
и увидеть идентичный рендеринг. Существующие pass'ы не тронуты.

---

### Phase 2a: Миграция Fullscreen pass'ов (1 сессия)

**Задачи:**
- [ ] Мигрировать SSAOPass → FullscreenPass
- [ ] Мигрировать BloomExtract → FullscreenPass
- [ ] Мигрировать CompositePass → FullscreenPass
- [ ] Мигрировать VolumetricFogPass → FullscreenPass
- [ ] Удалить старые реализации

**Чекпоинт 2a:** Demo T2-T3 рендерится идентично до и после миграции.
Визуальное сравнение скриншотов. 4 pass'а теперь используют шаблон.

---

### Phase 2b: Миграция Compute pass'ов (1 сессия)

**Задачи:**
- [ ] Мигрировать GTAOPass → ComputePassBase
- [ ] Мигрировать SSRPass → ComputePassBase
- [ ] Мигрировать DoFPass → ComputePassBase
- [ ] Мигрировать ComputeParticlesPass → ComputePassBase
- [ ] Удалить старые реализации

**Чекпоинт 2b:** Demo T4 рендерится идентично. 4 compute pass'а
используют шаблон. Compute barriers корректно ставятся с BarrierFlags.

---

### Phase 2c: Миграция Geometry pass'ов (1 сессия)

**Задачи:**
- [ ] Мигрировать ShadowPass → GeometryPass
- [ ] Мигрировать OpaquePass → GeometryPass
- [ ] Мигрировать ParticlePass → GeometryPass
- [ ] Мигрировать TorchPass → GeometryPass
- [ ] Мигрировать WaterPass → GeometryPass (custom objectFilter: is_water)
- [ ] Удалить старые реализации

**Чекпоинт 2c:** Demo T1-T4 рендерится идентично. 5 geometry pass'ов
используют шаблон. Two-sided материалы корректно обрабатываются.

---

### Phase 2d: Миграция сложных pass'ов (1 сессия)

**Задачи:**
- [ ] Мигрировать HDRCompositePass → FullscreenPass с SSBO readback override
- [ ] Мигрировать BloomBlurPass → двойной FullscreenPass (ping-pong)
- [ ] Оценить AutoExposurePass -- 2-stage compute, возможно оставить как есть

**Чекпоинт 2d:** Все мигрируемые pass'ы (15 из 23) используют шаблоны.
8 специальных pass'ов остаются на прямом `execute()`.

---

### Phase 3: DrawList (опционально, по необходимости)

**Задачи:**
- [ ] Реализовать DrawList (draw_list.h/cpp)
- [ ] Интегрировать в GeometryPass как opt-in
- [ ] Профилировать: сравнить draw call'ы до и после сортировки

**Чекпоинт 3:** GeometryPass с DrawList рисует те же объекты с меньшим
числом state changes. Измеримо на сценах >100 объектов.

---

### Phase 4: Документация, cleanup и tech debt

**Задачи:**
- [ ] Обновить CLAUDE.md с описанием engine layer
- [ ] Написать комментарии к публичным API engine
- [ ] Удалить неиспользуемый код из мигрированных pass'ов
- [ ] Проверить что бенчмарк не затронут (нет регрессий)
- [ ] Рассмотреть декомпозицию DemoResources (см. 4.5)
- [ ] Рассмотреть декомпозицию T4 struct (см. 4.6)

**Чекпоинт 4:** Проект собирается, оба бинарника работают.
Engine layer документирован. Новый pass можно написать за 20 строк
вместо 60.

## 8. Что нужно знать новому участнику (после рефакторинга)

### Добавить объект на сцену

Знать: `SceneObject`, `MaterialDef`, `Materials::*()`, `MeshHandle`.
Файл: `demo_scene.cpp`, метод `buildScene()`.
GL-знания: **не нужны**.

### Добавить fullscreen эффект (bloom вариант, color grading, etc.)

Знать: `FullscreenPass`, `UniformBlock`, `TexSlot::*`, `ResourceDecl`.
Написать: ~20-30 строк C++ + шейдер.
GL-знания: **не нужны** (PassContext скрывает RT binding, state, viewport).

### Добавить compute эффект (новый AO, motion blur, etc.)

Знать: `ComputePassBase`, `UniformBlock`, `PassContext::bindImage/bindSSBO`.
Написать: ~20-30 строк C++ + compute shader.
GL-знания: **минимальные** (понимание image units и SSBO binding points).

### Добавить geometry pass (новый тип объектов, декали, etc.)

Знать: `GeometryPass`, `UniformBlock`, `SceneObject`, `objectFilter()`.
Написать: ~30-40 строк C++.
GL-знания: **не нужны** (state, culling, per-object uniforms -- автоматически).

### Написать специальный pass (нестандартная логика)

Знать: `DemoRenderPass::execute()`, `PassContext`, `Renderer` API.
GL-знания: **нужны** (но это редкий случай для нетривиальных эффектов).

## 9. Риски и митигация

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| Engine layer добавляет overhead | Низкая | PassContext -- inline делегирование, нет аллокаций. StateCache -- net positive (сокращает GL calls) |
| Шаблоны не покрывают edge case | Средняя | 8 pass'ов остаются специальными. DemoRenderPass::execute() всегда доступен как fallback |
| StateCache рассинхронизируется | Низкая | reset() при init + resetState() + beginFrame(). Нет glGet* вызовов |
| Миграция ломает рендеринг | Средняя | По одному pass'у за раз. Визуальное сравнение после каждого. Старый pass рядом до подтверждения |
| Engine зависит от demo headers | Низкая | Зависимость от интерфейсов (render_pass.h, uniform_block.h), не от реализаций. При необходимости -- вынести в engine/ |
| PassContext lifecycle change | Средняя | Высокий blast radius: все 23 pass'а + render_pass.h. Митигация: Phase 1a до миграции, механическая правка |
| DemoResources scope creep | Средняя | Отложена на Phase 4+. Strict scope -- только перегруппировка полей |
| T4 struct breaking change | Низкая | ~60 механических правок. Рекомендация: одновременно с Phase 2 |
| UniformBlock не масштабируется | Низкая | При >100 объектах ~1400 glUniform*/кадр. Документирован UBO migration path (см. 3.9) |

## 10. Метрики успеха

| Метрика | До | После |
|---------|-----|-------|
| Строк кода в типичном fullscreen pass | ~50-60 | ~20-25 |
| Строк кода в типичном geometry pass | ~60-80 | ~30-40 |
| Строк кода в типичном compute pass | ~40-50 | ~15-20 |
| GL-знания для fullscreen pass | Обязательны | Не нужны |
| GL-знания для geometry pass | Обязательны | Не нужны |
| GL-знания для добавления объекта | Не нужны | Не нужны |
| Redundant GL calls за кадр | ~300+ | ~50 (с StateCache) |
| Концепций для нового участника | 9 | 5 (PassContext, шаблон, UniformBlock, TexSlot, ResourceDecl) |

## 11. Анализ производительности

### 11.1 Текущие GL-вызовы за кадр (T4 Ultra, 25-28 объектов)

| Категория | Вызовов/кадр | Redundant (оценка) | Источник |
|-----------|:---:|:---:|----------|
| setDepthTest/setCullFace/setBlending/setDepthMask | ~66 | ~50% | 23 pass'а, each 3-10 state calls |
| bindTextureUnit (×3 GL-вызова каждый) | ~28 (= 84 GL) | ~20-33% | GTAO дублирует unit 0 |
| bindRenderTarget | ~28 | ~46% | 9 FBO, SceneToFBO bind/unbind |
| useCustomShader | ~18 | ~17% | opaque+shadow могут совпадать |
| dispatchCompute | ~12 | 0% | |
| memory barriers | ~14 | ~30% | BarrierFlags может устранить |
| glUniform* (через UniformBlock::set) | ~350 | 0% | 25 obj × 14 uniforms |
| drawMesh / drawMeshInstanced | ~27 | 0% | |
| **Итого GL-вызовов** | **~600** | | |

### 11.2 Ожидаемые улучшения от StateCache

| Оптимизация | Redundant устранено | Оценка |
|-------------|:---:|:---:|
| State caching (depth/cull/blend/mask/color) | ~33 | -50% state calls |
| Texture unit caching | ~24 GL calls (~8 binds × 3) | -30% tex calls |
| FBO binding caching | ~13 | -46% RT calls |
| Program binding caching | ~3 | -17% shader calls |
| Barrier granularity (BarrierFlags) | ~4-5 | -30% barriers |
| **Итого** | **~78** | **~600 → ~520 (-13%)** |

### 11.3 GPU-side стоимость redundant вызовов

Иерархия стоимости state changes (DOOM 2016, Christer Ericson):

| Уровень | Стоимость/вызов | Redundant | GPU overhead |
|---------|:---:|:---:|:---:|
| RT switch | ~10-50 μs | ~13 | ~325 μs |
| Program switch | ~1-10 μs | ~3 | ~9 μs |
| Texture bind | ~0.5-2 μs | ~12 | ~12 μs |
| glEnable/Disable | ~0.1-0.5 μs | ~33 | ~8 μs |
| **Итого** | | | **~354 μs/кадр (~2.1% бюджета при 60 FPS)** |

### 11.4 Позитивные находки

1. **Zero heap allocations** в горячих путях -- отлично
2. **Mesh VAO caching** (`last_drawn_mesh_`) эффективен
3. **Frustum culling** активен в 3 pass'ах (OpaquePass, WaterPass, TessellatedModelPass)
4. **Сцена маленькая** (25-28 объектов) -- DrawList sort не оправдан до >100 объектов
5. **Timer precision** -- `clock_gettime(CLOCK_MONOTONIC)` с наносекундной точностью

## 12. Архитектурные проблемы и смягчения

### 12.1 light_vp: инициализация в ShadowPass

**Проблема:** `fd.light_vp` вычисляется как side effect в `ShadowPass::execute()`.
5 pass'ов читают через guard `if (fd.has_shadows)`.

**Смягчение:** Перенести `computeLightMatrix()` в `buildFrameData()` (см. 3.8).
**Статус:** Phase 0.

### 12.2 DemoResources: паттерн дублирования

**Проблема:** 68 полей с парами `ShaderProgram owned + ShaderProgram* cached`.
ShaderCache постепенно замещает owned паттерн, но старые поля не удалены.

**Смягчение:** Документировано для Phase 4+ (см. 4.5). Не блокирует engine layer.

### 12.3 PipelineBuilder: строковое связывание

**Проблема:** 3 strcmp() вызова для special-case обработки.

**Смягчение:** PassRole enum (см. 3.7). **Статус:** Phase 0.

### 12.4 UniformBlock: immediate GL calls

**Проблема:** Каждый `set()` → `glUniform*`. Нет batching.

**Текущее влияние:** 350 `glUniform*`/кадр при 25 объектах. Приемлемо для GPU-bound.

**Будущий путь:** UBO (GL3Features уже поддерживает). Phase 4+. (см. 3.9)

### 12.5 Отсутствие shader sorting

**Проблема:** Объекты рендерятся в порядке вставки в `buildScene()`.

**Текущее влияние:** 25 объектов используют 1-2 шейдера. Потери ≈ 0.

**Будущий путь:** DrawList (Phase 3) с 32/64-bit sort key.

### 12.6 TierResourceView::T4: 30+ полей

**Проблема:** Ручная инициализация 30+ полей в конструкторе (tier_resource_view.h:114-126).
Добавление нового поля без обновления конструктора -- неинициализированный указатель.

**Смягчение:** Декомпозиция на sub-structs (см. 4.6). Phase 4+.

## 13. Анализ масштабируемости

### 13.1 Рост сцены до 100+ объектов

- **Uniform uploads:** 14 × 100 = 1400 `glUniform*` (~210 μs CPU). Не bottleneck.
- **Frustum culling:** 3 passes × 100 = 300 sphere-frustum tests (~0.5 μs). Пренебрежимо.
- **Shader sorting:** При >50 объектах DrawList sort окупается.
- **При 500 объектах:** ~7000 `glUniform*` → нужен UBO.

### 13.2 Более 4 тиров

- `DemoTierConfig` получит ещё ~15 полей → линейный рост, управляемо.
- `TierResourceView` получит struct T5 → нужна декомпозиция T4 перед добавлением T5.
- `DemoResources::prepare()` уже 1361 строк → при >1800 разбить на per-tier модули.
- `pipeline_builder.cpp` получит четвёртую ветку → PassRole enum масштабируется.

### 13.3 Более 10 compute pass'ов

- Barrier overhead: ~20 dispatches × ~2-5 μs = ~40-100 μs. Управляемо.
- При >15 compute'ов: рассмотреть grouped dispatch (несколько без barriers,
  один barrier в конце группы). Требует анализ RAW/WAR/WAW hazards.

### 13.4 RT count > 15

- Текущий memory footprint: 9 FBO × ~8 MB = ~72 MB VRAM при 1080p.
- При 15 FBO: ~120 MB. OK для дискретного GPU (T4 предполагает ≥2 GB VRAM).
- Свыше 20: рассмотреть resource aliasing (shared depth buffer, RT pooling).

### 13.5 Shader перестановки > 50

- ShaderCache: `unordered_map`, O(1) lookup. Масштабируется до 200+.
- Initial compile: 50 × ~5 ms = ~250 ms одноразово.
- **Bottleneck:** runtime switching. DrawList sort группирует по шейдеру.

## 14. Индустриальное сравнение

### 14.1 Frostbite FrameGraph (GDC 2017, Yuriy O'Donnell)

| Аспект | Frostbite | GPU Benchmark Engine | Решение |
|--------|-----------|---------------------|---------|
| Resource declarations | Runtime graph nodes | ResourceDecl enum | Упрощённая версия |
| Barrier coalescing | Автоматический merge | Per-dispatch BarrierFlags | Не берём: overhead от лишних < 1% на desktop GL |
| Resource aliasing | Memory aliasing между non-overlapping passes | Нет: каждый RT живёт весь кадр | Не берём: 72 MB VRAM при 9 FBO |
| Pass culling | Прунинг unreferenced pass'ов | `isEnabled()` + tier gating | Частично: ветки `has_hdr_writer`/`has_scene_writer` |
| Transient resources | Per-frame alloc/free | ScopedHandle RAII | Не берём: нет per-frame аллокатора |

### 14.2 DOOM 2016 / id Tech 6 (Tiago Sousa, GDC 2016)

- **State groups:** Полное описание состояния per draw call = наш `RenderState`
- **64-bit sort key:** `[layer:3][translucency:1][program:12][texture:12][depth:16][mesh:20]`.
  Наш DrawList: 32-bit `[shader:8][material:8][depth:16]` -- достаточно при <256 шейдерах
- **State change cost hierarchy:** RT > Program > Texture > State > Vertex.
  Применяется в StateCache и DrawList sort key design

### 14.3 bgfx (Бранимир Караджич)

- **View = PipelineNode:** RT + viewport + clear per view = наш `PassContext::beginRT()`
- **Sort-based rendering:** 64-bit key = наш DrawList (Phase 3)
- **Encoder pattern:** Multi-threaded recording. Не нужен для single-threaded GL
- **State bits:** 64-bit bitmask = наш `RenderState` (упрощённый, POD bools)

### 14.4 sokol_gfx (André Weißflog)

- **Pipeline State Objects на GL:** `sg_pipeline` = наш `RenderState` + `applyState()`
- **Shadow state:** Полный GL state shadow = наш `StateCache`
- **Pass model:** `sg_begin_pass()` / `sg_apply_pipeline()` / `sg_apply_bindings()`
  = `PassContext::beginRT()` / `applyState()` / `bindTexture()`
- **Правило "никогда glGet*":** Применяется (StateCache, строка ~609)

### 14.5 Molecular Matters (Stefan Reinalter)

- **Command Buckets:** Per-pass bucket + sort = DrawList (Phase 3)
- **Stateless rendering:** Каждый draw command содержит полное состояние
  = `RenderState` presets (atomic apply)
- **Sort-then-submit:** Не нужен при 25 объектах, ценен при >100

### 14.6 Godot: GL State Cache

- Полный state shadow с `gl_cache.depth_test` и подобными
- Наш StateCache -- идентичен по идее, минимален по scope (только используемые состояния)

### 14.7 Khronos Best Practices (OpenGL Insights, Chapter 28)

| Рекомендация | Текущий код | Engine layer |
|-------------|-------------|-------------|
| Shadow state (не glGet*) | Не используется (хорошо) | StateCache |
| Group draws by program | Нет сортировки | DrawList (Phase 3) |
| Fixed binding points | Hardcoded числа | TexSlot enum |
| Batch uniform updates | Отдельный glUniform per-value | UniformBlock (частично); UBO -- Phase 4+ |
| Use VAOs | last_drawn_mesh_ cache | Сохранено |

## Приложение A: Ссылки и источники

- [FrameGraph: Extensible Rendering Architecture in Frostbite (GDC 2017)](https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in) -- render graph, resource aliasing, pass culling
- [sokol_gfx.h Backend Tour: OpenGL](https://floooh.github.io/2020/02/17/sokol-gfx-backend-tour-gl.html) -- state groups, pipeline objects на GL
- [bgfx Internals](https://bkaradzic.github.io/bgfx/internals.html) -- 64-bit sort keys, command buffers
- [Stateless, Layered, Multi-Threaded Rendering (Molecular Matters)](https://blog.molecular-matters.com/2014/11/06/stateless-layered-multi-threaded-rendering-part-1/) -- command buckets, sort keys
- [Stingray Renderer Walkthrough #4: Sorting](http://bitsquid.blogspot.com/2017/02/stingray-renderer-walkthrough-4-sorting.html) -- 64-bit key layout
- [An Opinionated Post on Modern Rendering Abstraction Layers (Alex Tardif)](https://alextardif.com/RenderingAbstractionLayers.html) -- low-level vs high-level split
- [Order Your Graphics Draw Calls Around! (Christer Ericson)](https://realtimecollisiondetection.net/blog/?p=86) -- state change cost hierarchy
- [OpenGL State Manager (GameDev.net)](https://www.gamedev.net/forums/topic/664335-opengl-state-manager/) -- shadow state patterns
- [Khronos: Minimizing State Changes](https://www.opengl.org/archives/resources/code/samples/advanced/advanced98/notes/node266.html) -- state batching
- [glMemoryBarrier reference (docs.gl)](https://docs.gl/gl4/glMemoryBarrier) -- barrier bit semantics
- [DOOM 2016 - Graphics Study (Adrian Courrèges)](http://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/) -- state change hierarchy, compute-based post-processing
- [Frostbite: Barrier Coalescing](https://www.ea.com/frostbite/news/framegraph-extensible-rendering-architecture-in-frostbite) -- merge adjacent barriers, barrier scheduling

## Приложение B: Известные ограничения engine layer v1

Engine layer v1 **намеренно** не решает следующие задачи:

1. **Многопоточность** -- все GL-вызовы из одного потока. OpenGL
   context не thread-safe. Для MT rendering нужен Vulkan или command
   buffer abstraction (bgfx pattern). Текущий масштаб (23 pass'а,
   25 объектов) не требует MT.

2. **Resource aliasing** -- render targets не переиспользуются
   между pass'ами (Frostbite FrameGraph pattern). При 9 FBO и
   ~72 MB VRAM -- не оптимизируем.

3. **Pass culling** -- все enabled pass'ы выполняются, даже если
   их output не читается. Frostbite FrameGraph прунит unreferenced
   pass'ы. При 23 pass'ах -- overhead незначительный.

4. **Async compute** -- `QueueType::Compute` -- hint, не реализация.
   OpenGL не поддерживает async compute. Pass выполняется синхронно.

5. **Material system** -- материалы определяются полями SceneObject
   (MaterialDef POD). Нет material graph, нет shader permutation
   по материалу. Engine layer прокидывает material uniforms через `perObject()`.

6. **Render graph** -- engine layer НЕ является render graph.
   PipelineBuilder выполняет топологическую сортировку, но не
   автоматическую barrier insertion, resource aliasing, или pass
   reordering. Это convenience layer, не абстракция рендеринга.

7. **Dynamic resolution** -- viewport размеры фиксированы на кадр.
   Нет per-pass resolution scaling.

8. **Shader hot reload** -- ShaderCache компилирует при `prepare()`.
   Нет runtime recompilation.

## Приложение C: Бюджет производительности

### GL-вызовы: текущее vs целевое

| Категория | Текущее | Phase 1 (StateCache) | Phase 3 (+DrawList) |
|-----------|:---:|:---:|:---:|
| GL state calls | ~66 | ~33 (-50%) | ~33 |
| Texture binds (GL calls) | ~84 | ~60 (-30%) | ~48 (с кешем) |
| RT switches | ~28 | ~15 (-46%) | ~15 |
| Program switches | ~18 | ~15 (-17%) | ~12 (с сортировкой) |
| Uniform uploads | ~350 | ~350 | ~350 (UBO -- Phase 4) |
| Draw/Dispatch | ~40 | ~40 | ~40 |
| **Итого GL** | **~600** | **~520 (-13%)** | **~500 (-17%)** |

### CPU overhead от engine layer

| Компонент | Overhead per-frame | Примечание |
|-----------|:---:|:---:|
| PassContext construction | ~100 ns | 1x per frame (singleton) |
| StateCache checks | ~5 ns × 90 calls = ~450 ns | int comparison |
| RenderState copy | ~10 ns × 23 | memcpy struct |
| features<>() caching | saves ~23 × 50 ns = ~1150 ns | vs 23 virtual dispatches |
| **Net delta** | **-700 ns** (net saving) | StateCache saves > overhead |

### Вывод

При T4 Ultra pipeline и 25-28 объектах, rendering GPU-bound.
CPU overhead рендеринга < 0.5 ms. Engine layer не добавляет измеримого
CPU overhead и сокращает GL driver overhead на ~13%.

## Приложение D: Дополнительные улучшения (Khronos best practices)

По результатам анализа кодовой базы выявлены пробелы относительно
рекомендаций Khronos (OpenGL wiki, "Debugging Tools", "Common Mistakes",
"Performance" guides) и проверенных практик из id Tech, Frostbite, bgfx.

Эти улучшения **не входят** в основной scope engine layer (секции 3-7),
но дополняют его и рекомендуются к реализации параллельно или сразу после.

---

### D.1 GL Debug Output (Khronos KHR_debug)

**Проблема:** Проект не использует **ни одной** из рекомендаций Khronos
по отладке GL. Нет `GL_KHR_debug`, нет `glGetError`, нет debug context.
Ошибки GL (invalid enum, framebuffer incomplete, out of memory) проходят
**молча**. Единственная проверка -- `glGetShaderiv(GL_COMPILE_STATUS)`
при компиляции шейдеров (`gl2_renderer.cpp:176-190`).

**Рекомендация Khronos:**

> "Use KHR_debug. It is the single most useful tool for OpenGL development.
> There is no reason not to use it." -- OpenGL Wiki, "Debugging Tools"

**Решение:** Добавить `GLDebug` static class в `src/renderer/gl_debug.h`:

```cpp
#pragma once

// GL debug output wrapper (GL_KHR_debug / GL_ARB_debug_output).
// Khronos рекомендует включать ВСЕГДА при разработке.
// В release: noop (не запрашиваем debug context).
//
// Использование:
//   GLDebug::init();  // после создания GL context
//   GLDebug::pushGroup("ShadowPass");  // маркер для RenderDoc/Nsight
//   ... render ...
//   GLDebug::popGroup();
//
// Ref: https://www.khronos.org/opengl/wiki/Debug_Output
//      https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_debug.txt

class GLDebug {
public:
    // Инициализация: проверить GL_KHR_debug или GL_ARB_debug_output.
    // Если доступно -- установить callback, включить synchronous output.
    // Если нет -- noop, все методы ниже становятся пустыми.
    static void init();

    // Включить/выключить (для release builds или perf-sensitive участков).
    static void enable();
    static void disable();

    // Маркеры debug groups для RenderDoc, NVIDIA Nsight, Intel GPA.
    // Khronos: "Use glPushDebugGroup/glPopDebugGroup to annotate
    // your rendering for debugging and profiling tools."
    static void pushGroup(const char* name);
    static void popGroup();

    // Именование GL-объектов для отображения в debug tools.
    // Khronos: "Use glObjectLabel to name objects."
    static void labelBuffer(unsigned int id, const char* name);
    static void labelTexture(unsigned int id, const char* name);
    static void labelShader(unsigned int program, const char* name);
    static void labelFramebuffer(unsigned int fbo, const char* name);

    static bool available();

private:
    static bool available_;
    static bool enabled_;

    // GL_DEBUG_OUTPUT callback (Khronos signature):
    // source, type, id, severity, length, message, userParam
    static void APIENTRY debugCallback(
        unsigned int source, unsigned int type, unsigned int id,
        unsigned int severity, int length,
        const char* message, const void* userParam);
};
```

**Реализация `init()` (~50 строк):**

```cpp
void GLDebug::init() {
    // 1. Проверить GL_KHR_debug (GL 4.3+ core) или GL_ARB_debug_output
    // 2. Загрузить glDebugMessageCallback через SDL_GL_GetProcAddress
    // 3. glEnable(GL_DEBUG_OUTPUT)
    // 4. glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS) -- критично для точной диагностики
    // 5. glDebugMessageControl: включить WARNING + ERROR, отключить INFO + NOTIFICATION
    // 6. Установить debugCallback
}
```

**Callback фильтрация (из Khronos best practices):**

```cpp
void GLDebug::debugCallback(...) {
    // Фильтрация по severity:
    //   HIGH    → LOG_ERR (invalid op, undefined behavior, OOM)
    //   MEDIUM  → LOG_WRN (deprecated behavior, perf warnings)
    //   LOW     → LOG_DBG (redundant state change hints)
    //   NOTIFICATION → ignore (verbose driver info)
    //
    // Ref: Khronos "Debug Output" table of severity levels
}
```

**Интеграция debug groups в PassContext:**

```cpp
void PassContext::beginPass(const char* pass_name) {
    GLDebug::pushGroup(pass_name);  // видно в RenderDoc
}
void PassContext::endPass() {
    GLDebug::popGroup();
}
```

Каждый pass автоматически получает маркер в debug tools.
RenderDoc покажет: `ShadowPass → OpaquePass → SSAOPass → ...`
вместо безымянного потока GL-вызовов.

**Интеграция в контекст (gl_render_context.cpp):**

```cpp
// При --debug: запросить debug context
if (config.debug) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
}
// После создания контекста:
GLDebug::init();
```

**Object labeling (при создании ресурсов):**

```cpp
// В DemoResources::prepare():
GLDebug::labelTexture(shadow_depth_tex.id, "shadow_depth");
GLDebug::labelFramebuffer(shadow_rt.id, "shadow_fbo");
GLDebug::labelShader(sky_shader->program(), "sky_shader_t2");
```

**Масштаб:** ~120 строк (gl_debug.h + gl_debug.cpp). Нет внешних зависимостей.

**Фаза:** Phase 0 (подготовка). Включать до engine layer -- поможет при отладке.

---

### D.2 Frame Statistics Overlay

**Проблема:** DemoUI показывает только FPS и frame time graph
(`demo_ui.cpp:27-84`). Нет видимости: сколько GL-вызовов за кадр,
сколько draw calls, сколько state changes отсечено StateCache.

Без этих данных невозможно:
- Верифицировать эффективность StateCache
- Обнаружить regression при добавлении нового pass'а
- Оптимизировать конкретный pass

**Рекомендация Khronos:**

> "Measure everything. Use timer queries for per-pass timing.
> Count draw calls, state changes, and texture binds per frame."
> -- OpenGL Insights, Chapter 28: "Tracking the GL State Machine"

**Решение:** `FrameStats` struct + счётчики в PassContext и StateCache:

```cpp
// src/engine/frame_stats.h
#pragma once

// Per-frame GL call counters. Инкрементируются в PassContext/StateCache.
// Отображаются в DemoUI при --debug.
// Паттерн: Quake r_speeds, DOOM com_showFPS, Godot Performance Monitor.
struct FrameStats {
    // State management
    int state_applied;      // applyState() вызовов
    int state_skipped;      // отсечённых StateCache
    int state_total;        // state_applied + state_skipped

    // Resources
    int draw_calls;         // drawMesh() + drawMeshInstanced()
    int texture_binds;      // bindTextureUnit() реальных
    int texture_skipped;    // отсечённых texture cache
    int rt_switches;        // bindRenderTarget() реальных
    int rt_skipped;         // отсечённых FBO cache

    // Compute
    int compute_dispatches;
    int barriers_issued;    // реальных
    int barriers_skipped;   // отсечённых BarrierFlags

    // Shaders
    int shader_switches;    // useCustomShader() реальных
    int uniform_calls;      // glUniform* через UniformBlock

    // Scene
    int objects_drawn;      // прошли frustum culling
    int objects_culled;     // не прошли

    // Per-pass GPU timing (GL_ARB_timer_query)
    static const int MAX_PASS_TIMINGS = 32;
    struct PassTiming {
        const char* name;
        double gpu_ms;
    };
    PassTiming pass_timings[MAX_PASS_TIMINGS];
    int pass_timing_count;

    void reset() { *this = {}; }
};
```

**Инкрементирование (в PassContext):**

```cpp
void PassContext::applyState(const RenderState& state) {
    if (state_cache_.setDepthTest(state.depth_test)) {
        r_->setDepthTest(state.depth_test);
        stats_.state_applied++;
    } else {
        stats_.state_skipped++;
    }
    // ... аналогично для остальных
}
```

**Per-pass GPU timing (используя существующий GPUTimer):**

```cpp
void PassContext::beginPass(const char* name) {
    GLDebug::pushGroup(name);
    if (gpu_timer_.available()) gpu_timer_.begin();
    current_pass_name_ = name;
}

void PassContext::endPass() {
    if (gpu_timer_.available()) {
        gpu_timer_.end();
        double ms = gpu_timer_.elapsed_ms();
        stats_.pass_timings[stats_.pass_timing_count++] =
            { current_pass_name_, ms };
    }
    GLDebug::popGroup();
}
```

**Замечание:** GPUTimer (`gpu_timer.h`) уже реализован и протестирован в bench
mode. Для demo достаточно одного query object с begin/end вокруг каждого pass'а.
`elapsed_ms()` блокирует ожидание результата (до 10000 итераций polling,
`gpu_timer.cpp:82-106`), поэтому per-pass timing включается **только** при
`--debug` — иначе stall pipeline.

**Отрисовка в DemoUI (demo_ui.cpp):**

```
[F3 Debug Stats]
GL calls:  520 (78 skipped by StateCache)
Draws:     27 | Textures: 20 (8 cached) | RT: 15 (13 cached)
Compute:   12 dispatch | 9 barriers
Scene:     22 drawn / 6 culled
Shaders:   15 switches | 350 uniforms

Per-pass GPU time (ms):
  Shadow      0.42   ██
  Opaque      1.83   ████████
  SSAO        0.31   █
  Bloom       0.55   ██
  GTAO        0.28   █
  Composite   0.12   ▌
  Total       4.21
```

Toggle: F3 при `--debug`, или runtime через DemoDebugOverrides.

**Масштаб:** ~80 строк (frame_stats.h) + ~60 строк (счётчики в PassContext)
+ ~80 строк (отрисовка в DemoUI) = ~220 строк.

**Фаза:** Phase 1a (вместе со StateCache — счётчики инкрементируются там).

---

### D.3 Shader Validation Pipeline

**Проблема:** Текущая валидация шейдеров минимальна:
- `compileShader()` проверяет `GL_COMPILE_STATUS` и логирует ошибку
  (`gl2_renderer.cpp:176-190`)
- `linkProgram()` проверяет `GL_LINK_STATUS` (`gl2_renderer.cpp:192-213`)
- `ShaderCache::get()` возвращает `ShaderProgram*` или nullptr
- **Но pass'ы не проверяют nullptr.** `ub_.use()` вызовет `glUseProgram(0)`.

Отсутствуют:
- Валидация uniform'ов (шейдер ожидает `u_light_vp`, pass передаёт
  `u_lightVP` — location == -1, молча игнорируется)
- Проверка program validity перед draw (`glValidateProgram`)
- Раннее обнаружение: все шейдеры проверяются при prepare(), не при render

**Рекомендация Khronos:**

> "Use `glValidateProgram` before the first draw call with each program."
> -- OpenGL Wiki, "Shader Compilation"

> "Always check uniform locations. A location of -1 means the uniform
> was optimized out or misspelled." -- OpenGL Common Mistakes

**Решение:** Трёхуровневая валидация:

**Уровень 1: Compile-time (Phase 0, ~30 строк)**

В `DemoResources::prepare()` после всех шейдеров:

```cpp
bool DemoResources::validateShaders(DemoTier tier) {
    // Список обязательных шейдеров per tier:
    struct Required {
        ShaderProgram** ptr;
        const char* name;
    };
    Required core[] = {
        { &island_cache_[t], "island" },
        { &sky_cache_[t],    "sky"    },
        { &fur_cache_[t],    "fur"    },
    };
    for (auto& r : core) {
        if (!*r.ptr || !(*r.ptr)->valid()) {
            LOG_ERR("Missing required shader '%s' for tier %d", r.name, t);
            return false;
        }
    }
    return true;
}
```

**Уровень 2: Uniform validation при --debug (Phase 1a, ~40 строк)**

В `UniformBlock::set()` при первом обращении к location:

```cpp
int UniformBlock::loc(UniformId id) {
    if (locs_[id] == UNRESOLVED) {
        locs_[id] = shader_->loc(uniformName(id));
        // Khronos: "A location of -1 means the uniform was optimized out"
        if (locs_[id] < 0 && debug_mode_) {
            LOG_WRN("Uniform '%s' not found in shader '%s' — "
                    "optimized out or misspelled",
                    uniformName(id), shader_->name());
        }
    }
    return locs_[id];
}
```

Не ошибка (driver может оптимизировать неиспользуемый uniform), но предупреждение
помогает обнаружить опечатки: `u_fog_color` vs `u_fogColor`.

**Уровень 3: glValidateProgram при --debug (Phase 1a, ~20 строк)**

В `ShaderProgram::use()` при первом вызове:

```cpp
void ShaderProgram::use() {
    r_->useCustomShader(handle_);
    if (!validated_ && debug_) {
        glValidateProgram(program_);
        GLint ok = 0;
        glGetProgramiv(program_, GL_VALIDATE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            LOG_WRN("Program '%s' validation: %s", name_, log);
        }
        validated_ = true;
    }
}
```

Одноразовая проверка per-program. Ловит несовместимость текстурного
формата с sampler type, неинициализированные samplers, и другие ошибки
которые compile/link не обнаруживают.

**Масштаб:** ~90 строк. Все три уровня -- только при `--debug`, нулевой
overhead в release.

**Фаза:** Уровень 1 -- Phase 0. Уровни 2-3 -- Phase 1a.

---

### D.4 Data-Driven Scene Description

**Проблема:** `buildScene()` — 11 методов на C++ (`demo_scene.cpp:34-465`),
каждый хардкодит позиции, масштабы, материалы. Добавление объекта = C++,
перекомпиляция, ожидание.

Mesh и material не имеют строковых имён — только handles:
```cpp
obj.mesh = res_.core.rock_mesh;  // handle, не имя
obj.mat = Materials::rock(Vec3(0.55f, 0.5f, 0.45f));  // factory call
```

**Рекомендация из индустрии:**

Все major движки (id Tech, UE, Unity, Godot, Frostbite) используют
data-driven scene description. Уровень абстракции разный:
- id Tech: `.map` текстовый формат (entity + brushes)
- UE: `.umap` бинарный (actors + components)
- Godot: `.tscn` текстовый (nodes + properties)
- bgfx examples: JSON scene

Для нашего scope: минимальный формат, покрывающий текущие
placement-методы.

**Решение:** INI-подобный формат + loader (~200 строк):

```ini
# data/scenes/sanctuary.scene
# Формат: один объект на блок, пустая строка — разделитель

[model]
mesh = model
pos = 0.0 0.40 -0.20
scale = 1.0
material = skin
tint = 0.75 0.65 0.55
fur = true

[pedestal]
mesh = pedestal
pos = 0.0 -0.02 -0.20
scale = 0.45
material = stone
tint = 0.6 0.58 0.55

[rock_batch]
mesh = rock
pos = 0.0 0.0 0.0
scale = 1.0
material = rock
tint = 0.55 0.5 0.45

[tree]
mesh = tree_0
pos = -4.5 0.0 -2.0
scale = 0.7
material = foliage
tint = 0.25 0.45 0.15
min_tier = enhanced

[tree]
mesh = tree_1
pos = 3.8 0.0 -3.2
scale = 0.85
material = foliage
tint = 0.3 0.5 0.18
min_tier = enhanced
```

**Почему не JSON:** C++11 без внешних зависимостей. INI-парсер — 80 строк.
JSON парсер (даже header-only) — 2000+ строк (nlohmann, rapidjson).
`config.h/cpp` в проекте не содержит INI-парсинга, но формат тривиален.

**Loader архитектура:**

```cpp
// src/demo/scene_loader.h
#pragma once
#include "demo/scene_data.h"

class SceneLoader {
public:
    // Загрузить сцену из .scene файла.
    // mesh_registry: имя → MeshHandle (заполняется из DemoResources)
    // material_factory: имя → MaterialDef factory function
    struct MeshEntry { const char* name; MeshHandle handle; };
    struct MatEntry  { const char* name; MaterialDef (*factory)(Vec3 tint); };

    static bool load(const char* path,
                     const MeshEntry* meshes, int mesh_count,
                     const MatEntry* materials, int mat_count,
                     DemoTier current_tier,
                     std::vector<SceneObject>& out_opaque,
                     std::vector<SceneObject>& out_clouds);
};
```

**Регистрация mesh и material по имени (в DemoScene::setup):**

```cpp
SceneLoader::MeshEntry meshes[] = {
    { "model",    res_.core.model_mesh },
    { "pedestal", res_.pedestal_mesh_ },
    { "rock",     res_.core.rock_mesh },
    { "tree_0",   res_.tree_meshes_[0] },
    { "tree_1",   res_.tree_meshes_[1] },
    { "tree_2",   res_.tree_meshes_[2] },
    // ...
};

SceneLoader::MatEntry materials[] = {
    { "skin",    Materials::skin },
    { "stone",   Materials::stone },
    { "rock",    Materials::rock },
    { "foliage", Materials::foliage },
    { "terrain", [](Vec3 t) { return Materials::terrain(); } },
    // ...
};

SceneLoader::load("scenes/sanctuary.scene",
                  meshes, NVALS(meshes),
                  materials, NVALS(materials),
                  config_.tier, opaque_objects_, cloud_objects_);
```

**Fallback:** Если файл не найден (`getDataPath()` вернул пустую строку) —
`buildScene()` работает как раньше. Data-driven — opt-in, не обязательно.

**Camera keypoints** — аналогичный файл `data/scenes/sanctuary.camera`:
```ini
# radius, height, look_height
3.5 1.5 0.5
3.2 0.8 0.3
4.0 2.0 0.8
...
```

Загрузка в `CameraPath` вместо хардкодированных 10 keypoints
(`demo_camera.cpp:22-40`).

**Terrain height:** `sampleTerrainHeight()` остаётся в C++ (процедурная
функция, `demo_utils.h:73-80`). Scene loader вызывает её для объектов
с `snap_to_terrain = true`.

**Масштаб:** ~200 строк (scene_loader.h/cpp) + ~80 строк (INI parser)
+ ~20 строк (camera file loader) = ~300 строк.

**Фаза:** После Phase 2 (миграция pass'ов). buildScene() сохраняется
как fallback.

---

### D.5 Модернизация проверки расширений

**Проблема:** Проект использует устаревший подход к проверке расширений:

```cpp
// Текущий код (gl2_renderer.cpp:296):
const char* exts = (const char*)glGetString(GL_EXTENSIONS);
if (exts && strstr(exts, "GL_ARB_framebuffer_object")) { ... }
```

Этот подход:
1. Возвращает **одну строку** со всеми расширениями (может быть >10 KB)
2. **Deprecated** в core profile GL 3.1+ (может вернуть nullptr)
3. False positives: `strstr("GL_ARB_texture_gather", "GL_ARB_texture")` == true

**Рекомендация Khronos:**

> "As of OpenGL 3.0, `glGetString(GL_EXTENSIONS)` is deprecated.
> Use `glGetIntegerv(GL_NUM_EXTENSIONS)` and
> `glGetStringi(GL_EXTENSIONS, i)` instead."
> -- OpenGL Wiki, "Get Context Info"

**Решение:** Обёртка `GLExtensions` (~60 строк):

```cpp
// src/renderer/gl_extensions.h
#pragma once

// Modern extension checking (GL 3.0+ glGetStringi).
// Fallback: legacy glGetString(GL_EXTENSIONS) для GL 2.x.
// Ref: Khronos "Get Context Info" wiki.
class GLExtensions {
public:
    // Вызвать после GL context creation.
    static void init();

    // Точная проверка имени расширения (не substring).
    static bool has(const char* extension_name);

private:
    static bool use_modern_;  // GL 3.0+ path
    // Modern path: parsed into std::set or sorted vector for O(log N) lookup
    // Legacy path: cached glGetString result + word-boundary strstr
};
```

**Реализация:**

```cpp
void GLExtensions::init() {
    int major = GLLoader::glMajor();
    if (major >= 3 && cb_glGetStringi) {
        use_modern_ = true;
        int n = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        for (int i = 0; i < n; i++) {
            const char* ext = (const char*)cb_glGetStringi(GL_EXTENSIONS, i);
            extensions_.insert(ext);  // std::set<std::string>
        }
    } else {
        use_modern_ = false;
        legacy_string_ = (const char*)glGetString(GL_EXTENSIONS);
    }
}

bool GLExtensions::has(const char* name) {
    if (use_modern_)
        return extensions_.count(name) > 0;
    // Legacy: word-boundary matching (не просто strstr)
    if (!legacy_string_) return false;
    const char* p = legacy_string_;
    size_t len = strlen(name);
    while ((p = strstr(p, name)) != nullptr) {
        if ((p == legacy_string_ || p[-1] == ' ') &&
            (p[len] == '\0' || p[len] == ' '))
            return true;
        p += len;
    }
    return false;
}
```

**Интеграция:** Заменить все `strstr(exts, "GL_ARB_...")` в:
- `gl2_renderer.cpp` (~8 мест)
- `gl3_renderer.cpp` (~6 мест)
- `gl4_renderer.cpp` (~8 мест)
- `gpu_timer.cpp` (~1 место)

Механическая замена: `strstr(exts, "GL_ARB_xxx")` → `GLExtensions::has("GL_ARB_xxx")`.

**Масштаб:** ~60 строк (gl_extensions.h/cpp) + ~40 строк diff (замена вызовов).

**Фаза:** Phase 0 (подготовка). Не зависит от engine layer.

---

### Сводная таблица

| # | Что | Khronos reference | Строк | Фаза | Ценность |
|---|-----|------------------|:-----:|:----:|----------|
| D.1 | GL Debug Output (KHR_debug) | OpenGL Wiki "Debugging Tools" | ~120 | Phase 0 | Высокая: видимость ошибок GL |
| D.2 | Frame Statistics Overlay | OpenGL Insights Ch.28 | ~220 | Phase 1a | Высокая: верификация StateCache |
| D.3 | Shader Validation Pipeline | OpenGL Wiki "Shader Compilation", "Common Mistakes" | ~90 | Phase 0-1a | Средняя: раннее обнаружение |
| D.4 | Data-Driven Scene | Индустрия (id Tech .map, Godot .tscn) | ~300 | После Phase 2 | Средняя: итерация без перекомпиляции |
| D.5 | Modern Extension Check | OpenGL Wiki "Get Context Info" | ~100 | Phase 0 | Низкая: корректность на core profile |
| | **Итого** | | **~830** | | |

### Обновлённый Phase Plan

**Phase 0 (подготовка) -- дополнительные задачи:**
- [ ] `GLDebug::init()` + debug context при `--debug` (D.1)
- [ ] `GLExtensions::init()` + замена strstr (D.5)
- [ ] Shader validation level 1: `validateShaders()` в `prepare()` (D.3)

**Phase 1a (фундамент engine) -- дополнительные задачи:**
- [ ] `FrameStats` struct + счётчики в StateCache/PassContext (D.2)
- [ ] `beginPass()`/`endPass()` с debug groups и GPU timer (D.1 + D.2)
- [ ] Shader validation levels 2-3: uniform warning + glValidateProgram (D.3)
- [ ] Debug overlay в DemoUI (toggle F3) (D.2)

**После Phase 2 (миграция pass'ов):**
- [ ] Scene loader + .scene формат (D.4)
- [ ] Camera keypoint файл (D.4)
