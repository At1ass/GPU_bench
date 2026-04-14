# Engineering Roadmap

Инженерные задачи, не связанные с визуальным контентом сцены.
Обновлено: 2026-04-14.

---

## Выполнено

### Shader Infrastructure (2026-04-14)
- [x] ShaderBank + shader_registry.def — единый реестр шейдеров (28 записей)
- [x] ShaderCache → engine/ — data-driven FeatureDefine, ноль demo-знания
- [x] ShaderLoader → engine/ — загрузка GLSL + #pragma include
- [x] MeshPool → engine/
- [x] T4 upgrade chains — SHADER(Island, ..., IslandT4), auto-return в get()
- [x] Все string uniforms → U:: enum (0 строковых вызовов)
- [x] res.shader(ShaderBank::Id) API — пассы не знают о TierResourceView shader fields
- [x] ScopedSSBO → ScopedBuffer (HandleTraits<BufferHandle> + Renderer::destroyBuffer())
- [x] Partial allocation leak fixes (auto-exposure SSBOs, SSR snapshots)
- [x] Optional feature failure logging в prepare()

### Fullscreen Rendering (2026-04-14)
- [x] PassContext::drawFullscreen() — auto GL2 quad / GL3+ triangle
- [x] GL3Features::drawFullscreenTriangle() + empty VAO
- [x] Fullscreen quad → DemoResources (всегда доступен, любой тир)
- [x] scene_rt → DemoResources top-level (не bloom-specific)
- [x] Uber .vert dual-path: #ifdef GLSL_120 → VBO, else → gl_VertexID
- [x] GL4 .vert (tone_map_t4, volumetric_fog_t4) → gl_VertexID
- [x] FullscreenPass default: ctx.drawFullscreen() если quad_ не задан
- [x] BloomRes содержит только bloom-specific (bright_rt, blur_rt)

---

## В работе / Краткосрочно

### Auto-Exposure: убрать GPU→CPU stall
**Приоритет:** Medium
**Проблема:** `readSSBO()` копирует exposure value из GPU в CPU каждый кадр.
NVIDIA warning: `VIDEO → HOST memory copy`. CPU ждёт пока GPU закончит запись.

**Решение (по убыванию предпочтительности):**
1. **Не читать на CPU** — tone map shader читает exposure SSBO напрямую, CPU не видит значение (Filament/UE подход). Самый быстрый, но UI не может показать exposure.
2. **Persistent mapped buffer** — `glBufferStorage` с `MAP_PERSISTENT_BIT | MAP_COHERENT_BIT`. GPU пишет, CPU читает через pointer. `GL4Features::createPersistentBuffer` уже реализован. Нужен fence sync для определения готовности.
3. **Double-buffered SSBO** — два буфера, чередуются каждый кадр. CPU читает результат предыдущего кадра (задержка 1 frame, незаметна для exposure).

**Файлы:** `src/demo/passes/auto_exposure_pass.cpp`, `src/demo/passes/hdr_composite_pass.cpp`

### Testing Infrastructure: NullRenderer + Lifecycle
**Приоритет:** High
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

## Среднесрочно

### Generation Counters в Handle<Tag> (Filament pattern)
**Приоритет:** Medium
**Проблема:** Use-after-free handle не детектируется.
**Решение:** `Handle<Tag>` = `{ uint16_t index; uint16_t generation; }` (32 bit).
Slot map хранит текущую generation. При destroy — increment. При use — compare.
Mismatch = use-after-free, log + early return. Zero-cost для valid paths.

### Render Target Pooling
**Приоритет:** Low (станет High при Vulkan)
**Проблема:** RT создаются/удаляются при resize и tier change. GPU allocation stalls.
**Решение:** Pool keyed by (width, height, format). Freed RT → pool. Request → check pool first. Evict после N unused frames (Filament ResourceAllocator pattern).

### GLES 3.0 Renderer
**Приоритет:** Medium
**Проблема:** GLESRenderer = GLES 2.0. GLES 3.0 features (instancing, MRT, UBO) не используются на Android.
**Решение:** Поднять GLESRenderer до GLES 3.0+, интегрировать GL3Features.
**Файлы:** См. `project_gles3_gap.md` в memory

### UI Consolidation
**Приоритет:** Low
**Проблема:** Дублирование UI-констант и виджетов между BenchUI и DemoUI.
**Решение:** `ui_common.h` (shared constants) + `ui_widgets.h/cpp` (reusable widgets).

---

## Долгосрочно (Vulkan prep)

### Deferred Deletion Queue
**Приоритет:** Needed for Vulkan only
**Решение:** Per-frame `DeletionQueue`. На OpenGL flush сразу. На Vulkan flush когда fence frame N-2 signaled. bgfx/UE pattern.

### ResourceDecl → FrameGraph
**Приоритет:** Needed for Vulkan only
**Решение:** Эволюция текущего ResourceDecl + topological sort. FrameGraph аллоцирует transient ресурсы из pool, возвращает в pool по end-of-frame. Filament pattern.

### Vulkan Backend
**Приоритет:** May 2026 (отпуск)
**Состояние:** Архитектура 8/10 готова. Shader system больше не блокирует (ShaderCache engine-generic, FeatureDefine data-driven). Resource handles готовы (HandleTraits, ScopedHandle). Pipeline builder имеет ResourceDecl + topological sort.
**Блокеры:** Deferred deletion, RT pooling.

---

## Известные баги (не architectural)

- **Fur missing T2+** — Shell fur absent на bunny в тирах 2-4. Pre-existing.
- **T4 visual quality** — Green haze, desaturation, puddle rendering. Needs shader validation.

---

## Принятые архитектурные решения

| Решение | Дата | Паттерн | Обоснование |
|---------|------|---------|-------------|
| Typed opaque handles | existing | bgfx/Filament | Compile-time type safety |
| ScopedHandle RAII | existing | bgfx (explicit) → RAII | Automatic cleanup |
| ShaderBank + registry X-macro | 2026-04-14 | id Tech renderProg | Single source of truth |
| FeatureDefine table | 2026-04-14 | Data-driven preamble | Engine/demo separation |
| T4 upgrade chains | 2026-04-14 | Custom | Auto-return GL4 variant |
| ctx.drawFullscreen() | 2026-04-14 | Filament/bgfx | Tier-appropriate fullscreen |
| Centralized ownership | existing | All engines | DemoResources owns → TierResourceView borrows → Passes observe |
| No reference counting | — | id Tech/bgfx | Single-threaded, clear ownership |
| No deferred deletion | — | id Tech | OpenGL synchronous, not needed until Vulkan |
