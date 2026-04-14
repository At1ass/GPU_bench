# Engineering Roadmap

Инженерные задачи, не связанные с визуальным контентом сцены.
Обновлено: 2026-04-14 (post engine/demo separation + deep architectural audit).

---

## Выполнено

### Engine/Demo Separation (2026-04-14)
- [x] Engine self-contained — zero dependencies on demo/ (proven by standalone examples)
- [x] Two-layer pass system — RenderPassBase (engine) + DemoPassBase adapter (demo)
- [x] PipelinePolicy interface — application controls RT routing, engine sorts + executes
- [x] RenderPipeline + buildPipeline() moved to engine/
- [x] minTier()/isEnabled() removed from engine (business logic → demo layer)
- [x] DemoTier moved from engine/ to demo/
- [x] Examples: spinning_cube.cpp, postprocess_demo.cpp

### Shader Infrastructure (2026-04-14)
- [x] ShaderBank + shader_registry.def — single source of truth (28 entries)
- [x] ShaderCache → engine/ — data-driven FeatureDefine, zero demo knowledge
- [x] ShaderCache::compileInline() — uber preamble for runtime shaders
- [x] ShaderLoader → engine/ — GLSL loading + #pragma include
- [x] MeshPool → engine/
- [x] T4 upgrade chains — SHADER(Island, ..., IslandT4), auto-return in get()
- [x] All string uniforms → U:: enum (100% type-safe, 0 string calls)
- [x] res.shader(ShaderBank::Id) API

### Fullscreen Rendering (2026-04-14)
- [x] PassContext::drawFullscreen() — GL2 VBO quad / GL3+ fullscreen triangle
- [x] scene_rt separated from BloomRes
- [x] Uber .vert dual-path: #ifdef GLSL_120 → VBO, else → gl_VertexID
- [x] FullscreenPass default: ctx.drawFullscreen() when quad_ not set

### Resource Lifecycle (2026-04-14)
- [x] ScopedBuffer via HandleTraits<BufferHandle>
- [x] Renderer::destroyBuffer() with GL4Renderer override
- [x] Atomic allocation for paired resources (SSBOs, SSR snapshots)
- [x] Optional feature failure logging in prepare()
- [x] RenderContext::getDrawableSize() for HiDPI
- [x] setDepthMask(true) before clear in RenderPipeline (depth mask fix)

---

## Tier 1 — High Priority (делать следующими)

### 1. Per-Frame UBO — убрать 150+ дублированных glUniform вызовов
**Приоритет:** Highest | **Сложность:** Medium | **ROI:** Highest

**Проблема:** Каждый пасс устанавливает Proj, View, CamPos, Time, FogColor и т.д.
индивидуально. 15+ uniform × 10+ passes = 150+ redundant `glUniform*` calls/frame.
Риск inconsistency: пасс забывает установить u_view → использует матрицу предыдущего.

**Решение:** `PerFrameUBO` struct (proj, view, cam_pos, time, fog, near/far, etc.),
upload один раз через `gl3->updateUBO()`, bind at binding point 0. Shaders объявляют
`uniform PerFrame { ... }`. GL2 fallback: individual uniforms (уже работает).

**Ключевой факт:** GL3Features::createUBO/updateUBO/bindUBO **уже реализованы**, но
не используются pipeline. Только bench test test_ubo_switch.cpp использует UBO.

**Файлы:** engine/pass_context.cpp (upload), demo/passes/*.cpp (remove redundant sets),
data/shaders/uber/*.vert/*.frag (add uniform block)

**Паттерн:** Vulkan descriptor set 0, bgfx setViewTransform(), Filament PerViewUib.

---

### 2. Error Shader + Fallback Textures
**Приоритет:** High | **Сложность:** Low | **ROI:** High

**Проблема:** ShaderCache::get() возвращает nullptr при ошибке → pass crash или
тихо не рисует. Нет визуальной индикации что шейдер сломан.

**Решение:**
- Magenta error shader (trivial vs+fs) — compiled первым, returned вместо nullptr
- 1×1 white fallback texture + 1×1 flat normal map (0.5, 0.5, 1.0)
- `ShaderProgram::is_error_shader_` flag для UI warning

**Паттерн:** Unity pink shader, UE default material, Filament default material.

**Файлы:** engine/shader_cache.cpp (error shader), engine/pass_context.cpp (fallback textures)

---

### 3. Per-Pass GPU Timing
**Приоритет:** High | **Сложность:** Low | **ROI:** High

**Проблема:** GPUTimer используется только для bench. В demo mode — невидимо кто из
20 пассов ест GPU время. Оптимизация вслепую.

**Решение:** Обернуть каждый PipelineNode в timer queries (double-buffered, no stall).
Результаты в `FrameTimingReport`. Показывать в ImGui overlay.

**Файлы:** engine/render_pipeline.h (execute loop), demo/demo_ui.cpp (overlay)

**Паттерн:** bgfx view timing, Filament FrameGraph timing, RenderDoc GPU profiling.

---

### 4. StateCache Invalidation на FBO Switch
**Приоритет:** High | **Сложность:** Low | **ROI:** High

**Проблема:** StateCache кеширует GL state, но не invalidate-ит при bindRenderTarget().
FBO switch может неявно изменить state → cache stale → applyState() пропускает
нужные GL calls → фликер, чёрный экран. Подтверждено вживую в postprocess_demo.

**Решение:** `StateCache::reset()` (уже существует) вызывается в `bindRenderTarget()`.
Или: invalidate только depth/stencil-related state при FBO switch.

**Файлы:** engine/state_cache.h, renderer/backend/gl2_renderer.cpp (bindRenderTarget)

---

## Tier 2 — Medium Priority

### 5. PassContext как единственный pass↔GPU интерфейс
**Приоритет:** Medium | **Сложность:** Low

**Проблема:** Пассы получают `Renderer*` через `ctx.renderer()` и обходят PassContext
напрямую (`r->setDepthTest()`, `r->bindTextureUnit()` и т.д.). Два конкурирующих API.

**Решение:** Либо добавить все недостающие методы в PassContext (setDepthTest,
setCullFace, bindTextureUnit, bindRenderTarget), либо документировать что прямой
доступ — intentional design.

---

### 6. Pluggable Uniform Registry
**Приоритет:** Medium | **Сложность:** Medium

**Проблема:** uniform_registry.def содержит demo-specific entries (ProcTex, Metallic,
FurLength и т.д.). Standalone app наследует demo uniforms. Engine не должен знать
про demo-specific uniform names.

**Решение:** App предоставляет свой .def файл. Engine предоставляет механизм (U::Id enum,
UniformBlock), app предоставляет данные (конкретные uniform names).

---

### 7. MaterialInstance — pre-resolved uniforms
**Приоритет:** Medium | **Сложность:** Medium | **ROI:** Medium-High

**Проблема:** 12 `ub.set()` вызовов на каждый объект в `perObject()`. Забыл установить
uniform → тихий баг (используется значение предыдущего объекта).

**Решение:** `MaterialInstance` bundlёт (shader program ptr, pre-resolved locations,
material params as flat byte buffer). Один `material.apply(ctx)` вместо 12 вызовов.
Eliminates "забыл uniform" class of bugs.

**Паттерн:** Filament MaterialInstance, Unity Material, bgfx material.

---

### 8. Shader Hot Reload (dev builds)
**Приоритет:** Medium | **Сложность:** Medium | **ROI:** Medium-High

**Проблема:** Изменение шейдера требует перезапуска. Медленная итерация.

**Решение:**
- File watcher (inotify на Linux, polling на других платформах)
- `ShaderCache::reloadAll()` — recompile, atomic swap ShaderProgram internals
- Generation counter в ShaderProgram для invalidation UniformBlock cache
- Guard `#ifdef CB_DEV_BUILD`

**Паттерн:** Filament shader watcher, Unity live recompile, The Witness hot reload.

---

### 9. Runtime Pass Enable/Disable API
**Приоритет:** Medium | **Сложность:** Low

**Проблема:** Toggle "Enable Bloom" в UI требует full pipeline rebuild.
RenderPipeline не имеет `setPassEnabled("bloom", false)`.

**Решение:** Name-based lookup map (built during buildPipeline), toggle enabled flag.
`void RenderPipeline::setPassEnabled(const char* name, bool enable)`.

---

### 10. Testing Infrastructure: NullRenderer + Lifecycle
**Приоритет:** High | **Сложность:** Medium

**Состояние:** Priority 1 тесты выполнены (87 TEST_CASE, 605 assertions).
Priority 2-3 ждут NullRenderer.

**Задачи:**
- [ ] NullRenderer — implements all Renderer virtuals (no-op + counters)
- [ ] BenchTest lifecycle tests (setup/render/cleanup, no resource leaks)
- [ ] ScopedHandle RAII tests
- [ ] StateCache tests
- [ ] BenchRunner flow test
- [ ] Demo scene construction test

**Файлы:** См. `docs/testing_plan.md`

---

## Tier 3 — Low Priority / Polish

### 11. TextureBindSet — atomic texture slot binding
**Проблема:** Пассы дублируют slot number: `ctx.bindTexture(3, tex)` + `ub.set(U::ShadowMap, 3)`.
**Решение:** `TextureBindSet` pairs (slot, handle), auto-sets sampler uniforms.

### 12. ResourceId extensibility
**Проблема:** Closed enum — standalone app не может добавить свои resource IDs.
**Решение:** Custom0..CustomN range, или int-based с named constants.

### 13. Const-correctness fixes
**Проблема:** `ShaderProgram::loc()` не const (мутирует cache), `PassContext::renderer()` не const.
**Решение:** `mutable` на cache, const на accessors.

### 14. Auto-Exposure GPU→CPU stall
**Проблема:** `readSSBO()` VIDEO→HOST каждый кадр. NVIDIA warning.
**Решение:** Persistent mapped buffer или GPU-only (tone map читает SSBO напрямую).

### 15. Generation Counters в Handle<Tag> (Filament pattern)
**Проблема:** Use-after-free handle не детектируется.
**Решение:** `{ uint16_t index; uint16_t generation; }` — zero-cost для valid paths.

### 16. GLES 3.0 Renderer
**Проблема:** GLESRenderer = GLES 2.0. GLES 3.0 features не используются на Android.
**Решение:** Поднять GLESRenderer до GLES 3.0+, интегрировать GL3Features.

### 17. Render Target Pooling
**Проблема:** RT создаются/удаляются при resize. GPU allocation stalls.
**Решение:** Pool keyed by (width, height, format). Filament ResourceAllocator pattern.

### 18. Backend Code Deduplication
**Проблема:** GL2/GL3/GL4 дублируют texture/FBO/state логику.
**Решение:** Извлечь shared logic в base helpers. Критично для Vulkan backend.

### 19. UI Consolidation
**Проблема:** Дублирование UI-констант между BenchUI и DemoUI.
**Решение:** `ui_common.h` + `ui_widgets.h/cpp`.

---

## Долгосрочно (Vulkan prep)

### RenderState as PSO Key
`RenderState` + shader → hash → pipeline state object cache.
На GL — no-op. На Vulkan — VkPipeline. Подготовка без breaking changes.

### Deferred Deletion Queue
Per-frame `DeletionQueue`. На OpenGL flush сразу. На Vulkan — fence frame N-2.

### Vulkan Backend
**May 2026.** Блокеры: deferred deletion, RT pooling, PSO cache.
Архитектура готова: Renderer + features<T>(), PipelinePolicy, ShaderCache engine-generic.

---

## Отклонённые предложения (исследовано, не нужно)

| Предложение | Причина отклонения |
|-------------|-------------------|
| Command Buffer / Deferred Submission | RenderPipeline уже является pre-built command buffer |
| Per-Frame Render Graph (Filament FrameGraph) | Overkill для 23 passes со static activation |
| Backend Abstraction Rewrite | Текущий Renderer + features<T>() корректен для GL |

---

## Известные баги (не architectural)

- **Fur missing T2+** — Shell fur absent на bunny в тирах 2-4. Pre-existing.
- **T4 visual quality** — Green haze, desaturation, puddle rendering.

---

## Принятые архитектурные решения

| Решение | Дата | Паттерн | Обоснование |
|---------|------|---------|-------------|
| Typed opaque handles | existing | bgfx/Filament | Compile-time type safety |
| ScopedHandle RAII | existing | bgfx → RAII | Automatic cleanup |
| ShaderBank + registry X-macro | 2026-04-14 | id Tech renderProg | Single source of truth |
| FeatureDefine table | 2026-04-14 | Data-driven preamble | Engine/demo separation |
| T4 upgrade chains | 2026-04-14 | Custom | Auto-return GL4 variant |
| ctx.drawFullscreen() | 2026-04-14 | Filament/bgfx | Tier-appropriate fullscreen |
| Two-layer pass system | 2026-04-14 | Unity SRP Feature/Pass | Engine generic, demo typed |
| PipelinePolicy interface | 2026-04-14 | Unity RenderGraph + Filament | App controls RT routing |
| compileInline() | 2026-04-14 | Custom | Runtime uber shaders without files |
| Centralized ownership | existing | All engines | DemoResources owns, passes observe |
| No reference counting | — | id Tech/bgfx | Single-threaded, clear ownership |
| No deferred deletion | — | id Tech | OpenGL synchronous, until Vulkan |
| No render graph | 2026-04-14 | Static pipeline | 23 passes, rebuild per tier change |
| No command buffer | 2026-04-14 | RenderPipeline sufficient | Single-threaded GL, pre-built plan |
