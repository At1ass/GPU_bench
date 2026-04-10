# Полный анализ проекта GPU Benchmark

**Дата**: 2026-03-23
**Анализатор**: Claude Opus 4.6

---

## I. АРХИТЕКТУРА — СИЛЬНЫЕ СТОРОНЫ

### 1. Renderer Abstraction (LLVM-style Feature Dispatch)

Архитектура рендерера — одно из лучших решений в проекте. Паттерн `features<T>()` с compile-time `FeatureTag<T>::id` и runtime `queryFeature(int)` — это промышленный подход уровня LLVM/Clang:

```cpp
auto gl3 = renderer->features<GL3Features>();  // nullptr если недоступно
if (gl3) gl3->drawMeshInstanced(mesh, count);
```

Иерархия наследования `GL2Renderer → GL3Renderer → GL4Renderer` с feature-интерфейсами обеспечивает чистое расширение без нарушения OCP. Compile-time проверки через `renderer_traits<R>` и `test_traits<T>` гарантируют корректность на этапе компиляции.

### 2. Handle System — Type-Safe Resource Management

Система типизированных хендлов с `Handle<Tag>` предотвращает смешивание типов ресурсов:

```cpp
static_assert(!std::is_same<MeshHandle, TextureHandle>::value, "");
```

RAII-обёртки `ScopedHandle<H>` с move-семантикой и кастомными `HandleTraits<H>::destroy()` — отличное решение, минимизирующее утечки ресурсов. Slot reuse через `free_mesh_slots_` предотвращает бесконечный рост векторов.

### 3. X-Macro Test Registry

Весь реестр из 36 тестов определён одной строкой на тест в `test_registry.def`:

```
X(Fillrate, FillrateTest, "Fillrate", "fillrate", Fill, "...", "MPix/s", fillrate, Cap_None)
```

Это генерирует enum `TestId`, таблицу метаданных, фабричные функции — одна точка изменения. Compile-time валидация `validate_test<cls, caps>` через `static_assert` предотвращает несоответствие capability-флагов базовому классу теста.

### 4. UIView/UIState/UIAction — Model-View-Update

Чистое разделение: `UIView` (read-only snapshot) → `UIState` (mutable ImGui state) → `UIAction` (enum событий). App диспетчеризирует action. Полная развязка UI от бизнес-логики.

### 5. Демо-режим: Resource Lifecycle

`DemoResources` управляет временем жизни всех GPU-ресурсов:
- `prepare()` — однократная загрузка
- `viewForTier()` — shallow-copy для каждого тира
- `destroy()` — полная очистка

Каскадная очистка при частичных ошибках в `compileTierShaders()` — пример правильного error recovery.

### 6. RAII повсюду

`FileGuard`, `PipeGuard`, `ScopedHandle`, `ScopedMesh`, `ScopedTexture` — последовательное применение RAII паттерна через весь проект. Кастомные делитеры для `fclose`/`pclose` — идиоматический C++.

### 7. Scoring: Geometric Mean + Bottleneck Detection

Геометрическое среднее по категориям (Fill, Geometry, Compute, Overhead) — корректный выбор: предотвращает доминирование outlier'ов. Многоуровневый bottleneck анализ (категории + попарные сравнения: DrawCall vs DrawCallRaw, ALU vs FMA, Fillrate vs FBO) — профессиональная диагностика.

---

## II. КРИТИЧЕСКИЕ ПРОБЛЕМЫ

### CRITICAL-1: Double-Deletion Depth Texture (GL3Renderer)

**Файл**: `gl3_renderer.cpp`, `getRTDepthTexture()`

```cpp
TextureHandle GL3Renderer::getRTDepthTexture(RenderTargetHandle rt) {
    GLuint tex_id = render_targets_[rt].depth_tex;
    GLTex gt; gt.id = tex_id; gt.valid = true;
    // Создаёт НОВУЮ запись в textures_[], ссылающуюся на ТОТ ЖЕ GL-объект
    textures_.push_back(gt);
    return handle;
}
```

**Сценарий**: `destroyTexture(depth_tex)` удаляет GL-текстуру → `destroyRenderTarget(rt)` пытается удалить её повторно → **double-free**. Нет tracking'а ownership'а — оба entry считают себя владельцами.

**Решение**: Reference counting или запрет `destroyTexture()` для RT-owned текстур через флаг `owned_by_rt`.

### CRITICAL-2: Persistent Buffer Map Size = 0 (GL4Renderer)

**Файл**: `gl4_renderer.cpp`, строка 609

```cpp
pb.mapped = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 0, access);  // BUG: size=0!
```

`createPersistentBuffer()` выделяет `size_bytes` байт, но `mapPersistentBuffer()` маппит **0 байт**. Любая запись в `pb.mapped` — **undefined behavior**.

**Решение**: Сохранять `size_bytes` в `PersistentBuffer` и передавать в `glMapBufferRange()`.

### CRITICAL-3: P1%/P99% Swap в JSON Export

**Файл**: `demo_export.cpp`, строки 91-92

```cpp
fprintf(out, "      \"p1_fps\": %.2f,\n", t.p99_fps);    // ПЕРЕПУТАНО
fprintf(out, "      \"p99_fps\": %.2f,\n", t.p1_fps);    // ПЕРЕПУТАНО
```

P1% и P99% перцентили **поменяны местами** в JSON-выводе. Результаты бенчмарка некорректны.

---

## III. ПРОБЛЕМЫ ВЫСОКОГО ПРИОРИТЕТА

### HIGH-1: Неполное экранирование JSON

**Файл**: `results.cpp`, строки 80-87

```cpp
static void jsonEscape(FILE* out, const char* s) {
    if (*s == '"') fprintf(out, "\\\"");
    else if (*s == '\\') fprintf(out, "\\\\");
    else if (*s == '\n') fprintf(out, "\\n");
    else fputc(*s, out);  // ← Не экранирует \r, \t, \b, \f, 0x00-0x1F
}
```

Отсутствует экранирование управляющих символов (RFC 4627). GPU имена с `\t` или `\r` произведут **невалидный JSON**.

### HIGH-2: CSV Export без экранирования

**Файл**: `results.cpp`, строка 69

```cpp
fprintf(out, "%s,\"%s\",...", preset_name, hw.cpu_name.c_str(), ...);
```

Если GPU name содержит запятые или кавычки (`"GeForce GTX", Corp."`), CSV будет **malformed**. Нет CSV escaping'а по RFC 4180.

### HIGH-3: MRT Color Texture Leak (GL3Renderer)

**Файл**: `gl3_renderer.cpp`, `createMRTRenderTarget()`

```cpp
rt.color_tex = color_textures[0]; // Только первый attachment сохранён!
// color_textures[1..3] — orphaned GL objects
```

При `destroyRenderTarget()` удаляется только `color_tex`. Attachments 1-3 остаются как **GL object leak**.

### HIGH-4: Memory Leak в BenchRunner

**Файл**: `bench_runner.cpp`, строки 50-52

```cpp
BenchTest* test = g_tests[i].factory(preset);  // manual new
runTest(test, r, ctx, cfg, cb);                 // если исключение — leak
delete test;
```

Нет RAII-обёртки. При любом исключении в `runTest()` — утечка. Решение: `std::unique_ptr<BenchTest>`.

### HIGH-5: Path Traversal в DataPath

**Файл**: `data_path.cpp`, строка 58

```cpp
std::string p = std::string("data/") + relative_path;  // Нет проверки ".."
```

`getDataPath("../../etc/passwd")` может прочитать произвольный файл. Сейчас вызывается только с hardcoded путями, но при расширении функциональности станет уязвимостью.

### HIGH-6: Raw GL Calls в Demo Scene

**Файл**: `demo_scene.cpp`, строки 776, 1646

```cpp
glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, viewport_w_, viewport_h_);  // SSR
glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float), &exposure);     // Auto-exposure
```

Прямые GL-вызовы нарушают абстракцию Renderer. При смене бэкенда (GLES, Vulkan) потребуется переписывание.

---

## IV. ПРОБЛЕМЫ СРЕДНЕГО ПРИОРИТЕТА

### MED-1: Population vs Sample Stddev

**Файл**: `bench.cpp`, строка 141

```cpp
double stddev = sqrt(variance / sorted.size());  // Делит на N, не N-1
```

Использует **population stddev** вместо sample stddev. При малых выборках (60 warmup frames) CV занижен.

### MED-2: OBJ Loader — Buffer Overflow

**Файл**: `obj_loader.cpp`, строка 26

```cpp
char line[512];  // Face-строка с 100+ вершинами превысит буфер
```

512-байтный буфер для строки OBJ-файла. Длинные face-строки **молча обрезаются**, теряя геометрию.

### MED-3: OBJ Loader — sscanf + uninitialized consumed

**Файл**: `obj_loader.cpp`, строки 60-63

```cpp
int consumed = 0;
if (sscanf(p, "%d/%d/%d%n", &fv.vi, &fv.ti, &fv.ni, &consumed) >= 3 && consumed > 0)
```

Если sscanf возвращает < 3, `consumed` может быть не инициализирован (зависит от реализации `%n`). Потенциальный бесконечный цикл.

### MED-4: Config Parsing без валидации

**Файл**: `preset_io.cpp`

```cpp
if (key == "warmup_frames") p.warmup_frames = atoi(val.c_str());
```

`atoi()` без проверки границ. `warmup_frames=-1` или `warmup_frames=999999999` — невалидный пресет без ошибки.

### MED-5: Silent Validation Failures в Renderer

**Файл**: `gl2_renderer.cpp`, все `isValid*()` проверки

```cpp
void GL2Renderer::drawMesh(MeshHandle h) {
    if (!isValidMesh(h)) return;  // Silent — нет логирования!
}
```

Невалидные хендлы молча игнорируются. Баги вызывающего кода **невидимы** при отладке.

### MED-6: Hardcoded Texel Size в GL3 Shaders

**Файлы**: `fur_t2.frag`, `island_t3.frag`

```glsl
float texel = 1.0 / 1024.0;  // Hardcoded! Должно быть uniform
```

Шейдер предполагает фиксированный размер shadow map. Если config изменит разрешение — артефакты.

### MED-7: ShaderProgram Move Assignment Cache Leak

**Файл**: `shader_program.h`

```cpp
ShaderProgram& operator=(ShaderProgram&& o) = default;
```

Default move не очищает `cache_` (unordered_map uniform locations). При повторном перемещении — stale entries.

### MED-8: Double glFinish() Per Frame

**Файл**: `bench_runner.cpp`, строки 148, 154

```cpp
r->finish();                    // ← Sync #1
frame_t.reset();
test->render(r);
r->finish();                    // ← Sync #2
double ms = frame_t.elapsed_ms();
```

Двойной `glFinish()` на каждом фрейме скрывает эффект pipelining'а GPU. Замеры не отражают реальную производительность в пайплайновом режиме.

---

## V. ПРОБЛЕМЫ НИЗКОГО ПРИОРИТЕТА

| # | Проблема | Файл |
|---|----------|------|
| L1 | `assert()` в Transform Feedback (disabled в Release) | `gl3_renderer.cpp:553` |
| L2 | String-based test lookup O(n²) в composite score | `bench.cpp:151-156` |
| L3 | Нет `-Wshadow -Wpedantic` в CMake | `CMakeLists.txt` |
| L4 | Нет GLSL precision qualifiers для GLES | Все шейдеры |
| L5 | Нет circular include detection в ShaderLoader | `shader_loader.cpp` |
| L6 | Отсутствуют `.tesc`/`.tese` файлы для тесселяции | `data/shaders/gl4/` |
| L7 | Registry readlink() на Windows: 256 bytes без null-term guard | `hwinfo.cpp` |
| L8 | Camera path discontinuity на t=1.0 | `demo_camera.cpp:59-62` |
| L9 | `atoi()` вместо `strtol()` в CLI parsing | `main.cpp` |
| L10 | `PI` как `static const float` вместо `M_PI` | `mesh_gen.cpp:9` |

---

## VI. ДЕМО-РЕЖИМ — ДЕТАЛЬНЫЙ РАЗБОР

### Архитектура: A

Четырёхтировая прогрессия (Basic → Enhanced → Quality → Ultra) — отличная структура. Каждый тир добавляет конкретные техники:

| Тир | GL | Техники |
|-----|-----|---------|
| T1 | 2.1 | Blinn-Phong, fur shells (24), sky, particles |
| T2 | 3.0 | +Shadow map 1024, SSAO, bloom, instanced grass |
| T3 | 3.3 | +PCF 5×5, 3 point lights, normal mapping, DoF, 48 shells |
| T4 | 4.3 | +PBR, compute particles, tessellation, vol fog, HDR, PCSS, GTAO, SSR, 64 shells |

### Rendering Pipeline T4 (10 passes)

```
1. Auto-exposure compute
2. Compute particles
3. Shadow pass
4. HDR scene FBO
5. Scene copy (SSR)
6. GTAO/SSAO
7. Volumetric fog
8. Compute bloom
9. Depth of Field
10. HDR composite + tone mapping
```

Пайплайн архитектурно корректен, memory barriers расставлены правильно после compute dispatches.

### Catmull-Rom Camera

Реализация математически корректна:

```
0.5 × [2P₁ + (-P₀+P₂)t + (2P₀-5P₁+4P₂-P₃)t² + (-P₀+3P₁-3P₂+P₃)t³]
```

10 keypoint'ов с clamped neighbor indexing на границах.

### Scoring

```cpp
results.demo_score = pow(product, 1.0 / count) * 10000.0;
```

Геометрическое среднее normalized scores × 10000 — стандартный подход (аналог 3DMark).

### Resource Lifecycle Diagram

```
DemoRunner::run()
├─ DemoResources::prepare()
│  ├─ loadSharedMeshes()      → MeshPool (sky, model, ground, rocks, grass)
│  ├─ loadSharedTextures()    → ScopedTexture (fur, normal map)
│  ├─ compileSkyShader()      → ShaderProgram
│  ├─ compileTierShaders()    → island_shaders[4], fur_shaders[4]
│  ├─ createShadowResources() → ScopedRenderTarget + shader [T2+]
│  ├─ createBloomResources()  → 3× ScopedRenderTarget + 3 shaders [T2+]
│  ├─ createSSAOResources()   → 2× ScopedRenderTarget + noise tex [T2+]
│  └─ createT4Resources()     → SSBO, HDR RTs, compute shaders [T4]
│
├─ For each tier:
│  ├─ viewForTier()           → TierResourceView (shallow copy)
│  ├─ DemoScene::setup()      → buildScene (place objects)
│  ├─ runTier()
│  │  ├─ Warmup (1 sec)
│  │  ├─ Measurement loop
│  │  └─ Statistics (avg, min, p1, p99 FPS)
│  └─ DemoScene::cleanup()    → Clear object lists
│
└─ DemoResources::destroy()
   ├─ ScopedHandle::reset()   → RAII cleanup
   └─ Manual SSBO cleanup     → via ComputeFeatures
```

### Проблемы демо-режима

Все critical/high проблемы описаны выше (CRITICAL-3: P1/P99 swap, HIGH-6: raw GL calls). Дополнительно:

- **Depth texture lifetime coupling**: `shadow_depth_tex_`, `scene_depth_tex_`, `hdr_depth_tex_` — raw handles без ScopedTexture. Безопасно только благодаря порядку уничтожения, но **хрупко**.
- **Warmup фаза** (1 секунда) может быть недостаточной для мобильных GPU с thermal throttling.

---

## VII. БЕЗОПАСНОСТЬ — СВОДКА

| Категория | Статус | Детали |
|-----------|--------|--------|
| Buffer Overflow | **Нет** | Все буферы с bounds-check'ами |
| Format String | **Нет** | Format строки hardcoded |
| Command Injection | **Нет** | popen() с hardcoded командами |
| Path Traversal | **Частично** | DataPath не фильтрует `..`, но вызовы hardcoded |
| Integer Overflow | **Минимально** | `atoi()` без проверок, OBJ relative indices |
| Use-After-Free | **Да (потенциально)** | Depth texture double-delete (CRITICAL-1) |
| Double-Free | **Да** | CRITICAL-1 |
| CSV/JSON Injection | **Да** | HIGH-1, HIGH-2 |
| UB (Undefined Behavior) | **Да** | CRITICAL-2 (persistent buffer map size=0) |

---

## VIII. TYPE-SAFETY — СВОДКА

| Механизм | Оценка | Комментарий |
|----------|--------|-------------|
| Handle\<Tag\> | **A** | Предотвращает cross-type confusion |
| static_assert (POD) | **A** | Все preset params валидируются |
| FeatureTag compile-time | **A** | Мэтчинг test↔renderer в compile-time |
| validate_test\<\> | **A** | Caps↔base class enforcement |
| Implicit Handle→uint | **B** | `operator unsigned int()` позволяет неявное преобразование |
| ScopedHandle RAII | **A** | Move-only, proper reset on reassign |
| Config parsing | **D** | `atoi()` без range validation |

---

## IX. ШЕЙДЕРНАЯ СИСТЕМА

### Организация

- **Common Libraries** (`data/shaders/common/`): noise_lib, terrain_color, pbr_lib, pcss_lib
- **GL2 Path** (GLSL 1.20/1.50): Blinn-Phong, basic fur shells
- **GL3 Path** (GLSL 1.30-3.30): Shadows, SSAO, bloom pipeline
- **GL4 Path** (GLSL 4.30): PBR, compute particles, volumetric fog, tessellation

### #pragma include System

- Line-by-line parsing, includes from `data/shaders/common/` only
- Нет circular include detection
- Нет duplicate include guard'ов
- Нет caching mechanism'а

### Проблемы

- Hardcoded texel sizes в GL3 шейдерах (MED-6)
- Отсутствуют `.tesc`/`.tese` файлы для тесселяции (L6)
- Нет precision qualifiers для GLES (L4)
- NaN check через `c.r != c.r` вместо `isnan()` (стилистически)

---

## X. ИТОГОВАЯ ОЦЕНКА

| Аспект | Оценка | Комментарий |
|--------|--------|-------------|
| **Архитектура** | 9/10 | Профессиональные паттерны, чистое разделение |
| **Type Safety** | 8.5/10 | Compile-time валидация, Handle system |
| **Resource Management** | 8/10 | RAII, но depth texture coupling хрупок |
| **Error Handling** | 7/10 | Каскадная очистка, но silent failures |
| **Безопасность** | 7/10 | Нет критических уязвимостей в текущем use-case |
| **Демо режим** | 8.5/10 | Профессиональный пайплайн, 3 critical бага |
| **Шейдеры** | 8/10 | Корректная математика, hardcoded constants |
| **Портируемость** | 8/10 | 4 бэкенда, platform-specific разделение |
| **Код** | 8/10 | Чистый C++11, consistent style |
| **Общая** | **8/10** | Продакшн-качество с исправимыми дефектами |

---

## XI. ТОП-5 РЕКОМЕНДАЦИЙ (по приоритету)

1. **Исправить CRITICAL-1, 2, 3** — double-free, UB в persistent buffer, P1/P99 swap
2. **Добавить ownership tracking** для depth текстур (флаг `owned_by_rt` или refcount)
3. **Исправить JSON/CSV escaping** — полное экранирование по RFC 4627/4180
4. **Заменить `new`/`delete`** на `unique_ptr` в BenchRunner
5. **Добавить debug-logging** в `isValid*()` проверки для диагностики handle lifecycle

---

Проект демонстрирует **высокое качество архитектуры** и **профессиональные паттерны** (LLVM-style dispatch, X-macro registry, RAII handles, Model-View-Update). Основные проблемы — точечные баги в ресурсном менеджменте и сериализации, а не системные дефекты.
