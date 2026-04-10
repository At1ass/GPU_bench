# Дизайн-документ демо-сцены v2: "Forgotten Sanctuary"

## Ревизия

Обновление на основе:
- Первой реализации и визуальной оценки (скриншоты T1–T4)
- Анализа: 3DMark (Fire Strike/Time Spy), Unigine (Heaven/Valley), GFXBench (Aztec Ruins), glmark2
- Анализа draw call overhead по исходникам render passes
- Учёта полного спектра поддерживаемого оборудования: GeForce 6000+ / Radeon HD 2000+ → RTX 4090

---

## 1. Выявленные проблемы текущей реализации

| # | Проблема | Причина | Приоритет |
|---|----------|---------|-----------|
| 1 | Арка = полное кольцо | Полный тор, нижняя часть не уходит достаточно глубоко | P0 |
| 2 | Объекты не следуют рельефу | Фиксированный Y=-1.0 | P0 |
| 3 | Чёрные области неба (T1) | Sky dome мал или перекрыт terrain | P0 |
| 4 | Трава растёт сквозь объекты | Нет exclusion zones | P1 |
| 5 | Нет глубинных слоёв для DoF | Все объекты в ~3 юнитах | P1 |
| 6 | Все каменные объекты = одинаковый PBR | roughness ~0.85-0.93 везде | P1 |
| 7 | Shadow map не покрывает колонны | Ortho [-6,6] | P2 |
| 8 | Камера слишком низко в KP1/KP2 | Y=0.2-0.3, ниже рельефа | P2 |
| 9 | Пьедестал скрыт травой | Высота 0.6 | ✅ Исправлен (0.9) |

---

## 2. Целевое оборудование и бюджеты

### Спектр поддерживаемых GPU

| Класс | Примеры GPU | Год | GL | Доступные тиры |
|-------|-------------|-----|-----|----------------|
| Legacy | GeForce 6600, Radeon X1600 | 2004-2006 | 2.1 | T1 only |
| Low | GeForce 8600 GT, HD 2600 | 2007-2008 | 3.0 | T1, T2 |
| Entry | GT 430, HD 5450 | 2009-2011 | 3.3 | T1–T3 |
| Mid-Low | GTX 560, HD 7770 | 2011-2013 | 4.3 | T1–T4 |
| Mid | GTX 750 Ti, RX 560 | 2014-2017 | 4.5 | T1–T4 |
| Mid-High | GTX 1050 Ti, GTX 1060, RX 580 | 2016-2018 | 4.6 | T1–T4 |
| High | RTX 2060, RX 5700 XT | 2019-2020 | 4.6 | T1–T4 |
| Ultra | RTX 3060+, RX 6700+ | 2021+ | 4.6 | T1–T4 |

### Целевой FPS по тирам и классам GPU

Бенчмарк должен быть **playable** на минимальном железе для данного тира. "Playable" = ≥15 FPS (достаточно чтобы визуально оценить сцену).

| Тир | Min GPU | FPS на min GPU | FPS на GTX 1050 Ti | FPS на RTX 3060 |
|-----|---------|---------------|--------------------|-----------------|
| T1 | GeForce 8600 GT | ≥15 | 300+ | 1000+ |
| T2 | GTX 560 | ≥15 | 100+ | 500+ |
| T3 | GTX 750 Ti | ≥15 | 50+ | 300+ |
| T4 | GTX 1050 Ti | ≥10 | 15-30 | 100+ |

**Критерий:** если T1 на GeForce 8600 GT даёт <15 FPS — сцена слишком тяжёлая.

### Draw call бюджет

Из анализа render passes: каждый объект = 1 draw call в opaque pass. Shadow pass дублирует все объекты. Fur на T1 без instancing = 23 отдельных draw calls.

| Тир | Opaque | Shadow | Fur | Sky+Particles | Post | **Итого** |
|-----|--------|--------|-----|---------------|------|-----------|
| T1 | 10 | — | 23 | 2 | — | **~35** |
| T2 | 14 | 14 | 1 (instanced) | 2 | ~6 | **~37** |
| T3 | 18 | 18 | 1 (instanced) | 2 | ~6 | **~45** |
| T4 | 15 | 15 | 1 (instanced) | 2 | ~10 | **~43** |

**T1 на GeForce 8600:** 35 draw calls — приемлемо. Основная нагрузка = 23 fur shell draws (каждый = полный redraw bunny mesh с alpha blend). На GL2 без instancing это unavoidable.

**Оптимизация draw calls (если нужно):**
- Merge мелких объектов T1 в один батч: плита + 2 сферы = 1 draw вместо 3 (экономия 2 calls)
- Merge камней и скажем сферы можно было бы, но они имеют разные материалы
- На практике 35 draw calls — норма даже для GeForce 8600 (driver overhead ~0.1ms per call на DX9-era GPU)

### Vertex бюджет

| Тир | Opaque verts | Fur verts (×shells) | Grass verts | Terrain verts | **Итого** |
|-----|-------------|---------------------|-------------|---------------|-----------|
| T1 | ~10K model + 3.5K scene | ~10K × 24 = 240K | ~4K batch | ~6.4K | **~254K** |
| T2 | + ~1.5K ruins | ~10K × 1 (instanced) | 30K × 5 = 150K | 6.4K | **~180K** |
| T3 | + ~1K more | ~10K × 1 | 50K × 5 = 250K | 6.4K | **~270K** |
| T4 | ~same | ~10K × 1 + tess ×36 | 80K × 5 = 400K | 6.4K | **~770K** |

**T1 vertex bottleneck:** 240K вершин от fur shells. Это ≈ 80K triangles × 24 прохода. На GeForce 8600 GT (vertex throughput ~150M tri/s) = ~0.5ms. Допустимо.

**T4 tessellation:** bunny mesh ×36 = ~360K patches ≈ 1M+ triangles. Сопоставимо с GFXBench Aztec Normal (220K tri/frame), но с tessellation overhead.

---

## 3. Индустриальное сравнение

### Сравнение подхода с коммерческими бенчмарками

| Аспект | 3DMark Fire Strike | Unigine Heaven | GFXBench Aztec | **Наш T4** |
|--------|-------------------|----------------|----------------|-----------|
| API | DX11 | DX11/GL4 | ES 3.1/Vulkan | GL 4.3 |
| Tri/frame | ~8M | ~2M | 220K–440K | ~770K |
| Shadow lights | 100 spot | CSM | Multiple | 1 dir PCSS |
| Post-FX | Bloom, DoF | SSAO, GI | Bloom, DoF, MB | Bloom, GTAO, DoF, SSR, Vol Fog, ACES |
| Particles | Half-res GPU | — | Fire/smoke | Compute 4K |
| Tessellation | — | All surfaces | PN-triangles | PN-tri на bunny |
| Scene objects | Сотни | Десятки зданий | ~50 | ~20 |

**Вывод:** Наш бенчмарк легче 3DMark по геометрии (770K vs 8M), но сопоставим с GFXBench по сложности. Post-processing pipeline в T4 сопоставим с Time Spy. Для нашего целевого железа (GTX 1050 Ti minimum для T4) это правильный баланс.

### Ключевые уроки из индустрии

**3DMark — каждый тест стрессирует конкретную подсистему:**
- GT1: geometry + shadows (100 shadow-casting lights)
- GT2: fill rate + compute (full-res particles + GPU sim)
- **Применение:** T1-T2 стрессируют vertex/fill. T3-T4 стрессируют compute/bandwidth.

**Unigine — vegetation exclusion:**
- Mesh Clutter: 2D grid, density per cell, radial exclusion от объектов
- Рекомендация: exclusionRadius = objectBoundingRadius × 1.5

**GFXBench Aztec — tier scaling:**
- Normal→High: +100% triangles, +27% shader work, +GI probes
- **Аналог:** Наш T3→T4: +60% grass, +tessellation, +PBR BRDF, +compute passes

**glmark2 — isolation:**
- Каждая сцена тестирует один feature. Наш progressive approach (одна сцена, 4 тира) ближе к 3DMark. Оба валидны.

**3DMark — камера:**
- Медленные ракурсы (2-3 сек) для showcase quality
- Быстрые пролёты для stress geometry throughput
- Обязательные ракурсы: сверху (тени/AO), eye-level (DoF), contre-jour (bloom/god rays)

---

## 4. Half-Torus для арки (P0)

### Проблема
Полный тор виден как кольцо. Закапывание center на y=-0.2 помогает частично, но нижняя геометрия всё равно рендерится (тратит fill rate, может вызвать z-fighting с terrain).

### Решение: генерация half-torus

```cpp
// mesh_gen.h:
MeshData halfTorus(int ring_segments, int tube_segments,
                   float ring_radius, float tube_radius);

// mesh_gen.cpp:
// Ограничить u ∈ [0, PI] вместо [0, 2*PI]:
MeshData MeshGen::halfTorus(int ring_seg, int tube_seg, float R, float r) {
    MeshData m;
    for (int i = 0; i <= ring_seg; i++) {
        float u = static_cast<float>(i) / ring_seg * PI;  // [0, PI]
        float cu = cosf(u), su = sinf(u);
        for (int j = 0; j <= tube_seg; j++) {
            float v = static_cast<float>(j) / tube_seg * 2.0f * PI;
            float cv = cosf(v), sv = sinf(v);
            Vertex vt;
            vt.pos = Vec3((R + r * cv) * cu, (R + r * cv) * su, r * sv);
            vt.normal = Vec3(cv * cu, cv * su, sv);
            vt.uv = Vec2(static_cast<float>(i) / ring_seg,
                         static_cast<float>(j) / tube_seg);
            m.vertices.push_back(vt);
        }
    }
    // Indices: quads между соседними кольцами
    for (int i = 0; i < ring_seg; i++) {
        for (int j = 0; j < tube_seg; j++) {
            unsigned int a = i * (tube_seg + 1) + j;
            unsigned int b = a + tube_seg + 1;
            m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(a + 1);
            m.indices.push_back(a + 1); m.indices.push_back(b); m.indices.push_back(b + 1);
        }
    }
    return m;
}
```

**Результат:** арка от (R, 0, 0) через (0, R, 0) до (-R, 0, 0) — ровно полукруг. Основания лежат на y=0, вершина на y=R. При `translate(0, groundY, -1.8) * rotateZ(...)` — арка стоит на земле.

**Трансформ:** `translate(0.0f, -1.0f, -1.8f)` — основания на уровне земли, вершина на y = -1.0 + 1.2 = 0.2.

---

## 5. Terrain-Following Placement (P0)

### Реализация

```cpp
// В demo_scene.cpp — static utility:
static float sampleTerrainHeight(float x, float z) {
    float h = 0.0f;
    h += sinf(x * 0.4f) * cosf(z * 0.3f) * 0.6f;
    h += sinf(x * 0.7f + 1.3f) * sinf(z * 0.5f + 0.7f) * 0.3f;
    // Pond depression
    float pdx = x - 3.5f, pdz = z - 3.5f;
    float pd = sqrtf(pdx * pdx + pdz * pdz);
    float pt = pd < 3.0f ? (3.0f - pd) / 3.0f : 0.0f;
    pt = pt * pt * (3.0f - 2.0f * pt);
    h -= pt * 0.3f;
    // Center flattening
    float cd = sqrtf(x * x + z * z);
    float ft = cd < 2.5f ? (cd < 1.0f ? 1.0f : (2.5f - cd) / 1.5f) : 0.0f;
    ft = ft * ft * (3.0f - 2.0f * ft);
    h *= (1.0f - ft);
    return h - 1.0f;  // -1.0 = ground plane offset
}
```

**Использование в place-методах:**
```cpp
// Вместо: Mat4::translate(x, -1.0f, z)
// Теперь: Mat4::translate(x, sampleTerrainHeight(x, z), z)
```

**Объекты, которым нужно terrain-following:**
- Колонны A, B, C, D
- Упавшая колонна
- Замшелый блок
- Обелиск
- Каменные сферы
- Плита
- Чаша (у основания пьедестала — центр выровнен, h≈-1.0)

**Объекты на фиксированной высоте (центр выровнен):**
- Пьедестал (h=-1.0, центр всегда flat)
- Кольца (h≈-0.95/-0.97, центр flat)
- Пруд (h=-1.25, в депрессии)

---

## 6. Sky Dome Fix (P0)

### Проблема
На T1 (screenshot) видны чёрные области вверху экрана.

### Решение (из индустрии — 3DMark, Unigine)

**Рекомендуемый подход:** рендерить sky без depth write.

```cpp
// В SkyPass::execute():
r->setDepthWrite(false);
r->setDepthFunc(DepthFunc::LessEqual);  // или Always
// ... draw sky mesh ...
r->setDepthWrite(true);
r->setDepthFunc(DepthFunc::Less);
```

Если Renderer API не поддерживает `setDepthWrite()` — увеличить sky mesh:
```cpp
// В loadSharedMeshes:
MeshData sd = MeshGen::sphere(32, 16);
// Scale to 50 units (>> terrain 20x20):
for (auto& v : sd.vertices) v.pos = v.pos * 50.0f;
```

---

## 7. Grass Exclusion Zones (P1)

### Индустриальный подход (Unigine Mesh Clutter)

**Для instanced grass (T2+):** в vertex shader — discard instances вблизи объектов.

```glsl
// grass.vert — добавить:
#ifdef HAS_EXCLUSION_ZONES
uniform vec4 u_exclusion[8];  // xyz = pos, w = radius
uniform int u_exclusion_count;
#endif

// В main() после вычисления world position:
#ifdef HAS_EXCLUSION_ZONES
for (int i = 0; i < u_exclusion_count; i++) {
    float d = length(worldPos.xz - u_exclusion[i].xz);
    if (d < u_exclusion[i].w) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0);  // behind near plane
        return;
    }
}
#endif
```

**Для batched grass (T1):** exclusion при генерации mesh в `scatteredGrass()`. Уже есть min distance от центра (1.2), достаточно увеличить до 2.0 для пьедестала.

**Зоны exclusion:**
| Объект | X | Z | Radius |
|--------|---|---|--------|
| Пьедестал | 0.0 | 0.0 | 1.2 |
| Колонна A | -1.8 | -1.5 | 0.4 |
| Колонна B | 1.8 | -1.5 | 0.4 |
| Пруд | 3.5 | 3.5 | 3.0 |

---

## 8. Процедурные деревья (P1)

### Цель
Depth layering для DoF (foreground/midground/background). Сейчас все объекты в ~3 юнитах от центра — DoF почти не виден.

### Реализация

```cpp
// mesh_gen.h:
MeshData simpleTree(float trunk_h, float trunk_r,
                    float crown_h, float crown_r, int segments = 12);

// mesh_gen.cpp:
// Ствол: cylinder
// Крона: 2 overlapping cones
MeshData MeshGen::simpleTree(float th, float tr, float ch, float cr, int seg) {
    MeshData m = cylinder(seg, th, tr);
    MeshData cone1 = cone(seg, ch, cr);
    MeshData cone2 = cone(seg, ch * 0.7f, cr * 0.7f);
    appendMesh(m, cone1, Mat4::translate(0, th, 0));
    appendMesh(m, cone2, Mat4::translate(0, th + ch * 0.4f, 0));
    return m;
}
```

**Размещение: 5 деревьев, T2+:**

| # | X | Z | trunk_h | crown_r | Scale |
|---|---|---|---------|---------|-------|
| 1 | -5.0 | -4.0 | 1.5 | 0.8 | 1.0 |
| 2 | -6.5 | 1.0 | 1.2 | 0.6 | 0.8 |
| 3 | 5.5 | -3.0 | 1.8 | 1.0 | 1.2 |
| 4 | -4.0 | 5.0 | 1.0 | 0.5 | 0.7 |
| 5 | 6.0 | 2.5 | 1.4 | 0.7 | 0.9 |

**Расстояние от камеры:** 5-8 юнитов → background для DoF blur.

**Тир:** T2+ (не T1 — на legacy GPU лишние draw calls не нужны, и деревья без теней бесполезны).

**Материалы:**
- Ствол: `MaterialType::Model`, color `(0.35, 0.25, 0.15)`, roughness 0.85
- Крона: `MaterialType::Model`, color `(0.20, 0.40, 0.15)`, roughness 0.80, `vertex_wind = true`

**Vertex count:** ~400-600 на дерево. 5 деревьев = ~2500 вершин.

**Draw call impact:**
- T2: +5 opaque + 5 shadow = +10 draw calls → итого ~47 (приемлемо для GTX 560)
- Можно merge все 5 деревьев в 2 batched mesh (стволы + кроны) → +2 вместо +5

### Влияние на слабые GPU
Деревья **не добавляются в T1**. На T2+ minimum GPU = GTX 560 (2011) — 47 draw calls не проблема.

---

## 9. PBR Material Variety (P1, только T4)

### Индустриальный подход (physically-based.info, Sebastien Lagarde)

Расширение roughness/metallic диапазона для T4 — Blinn-Phong (T1-T3) игнорирует эти параметры.

| Объект | Roughness T1-T3 | Roughness T4 | Metallic T4 | Visual T4 |
|--------|----------------|-------------|-------------|-----------|
| Пьедестал | 0.85 | 0.65 | 0.0 | Полированный камень |
| Колонны tall | 0.90 | 0.85 | 0.0 | Сухой камень |
| Колонны stump | 0.90 | 0.95 | 0.0 | Выветренный |
| Арка | 0.92 | 0.70 | 0.0 | Гладкий мрамор |
| Сферы | 0.82 | 0.35 | 0.0 | Отполированные шары (яркий Fresnel) |
| Обелиск | 0.85 | 0.50 | 0.15 | Полу-металлический минерал |
| Кольца | 0.88 | 0.25 | 0.8 | Бронзовые кольца |
| Чаша | 0.80 | 0.40 | 0.6 | Медь с патиной |
| Пруд | 0.02 | 0.02 | 0.0 | Вода (IOR 1.33) |

**Реализация:** в `placeRuins()` добавить условие:
```cpp
if (config_.enable_pbr) {
    obj.roughness = 0.35f;  // T4 polished value
    obj.metallic = 0.0f;
}
```

---

## 10. Volumetric Fog Improvements (P2, только T4)

### Индустриальный подход (Inigo Quilez, Frostbite)

**Height-based density:**
```glsl
float density = base_density * exp(-(pos.y + 1.0) * height_falloff);
// base_density = 0.15, height_falloff = 2.0
// y=-1.0 (земля): density = 0.15 (густой)
// y= 0.0: density ≈ 0.02 (разреженный)
// y= 1.0: density ≈ 0.003 (практически нет)
```

**Sun-colored inscattering (god rays):**
```glsl
float sunAmount = max(dot(rayDir, sunDir), 0.0);
vec3 fogColor = mix(
    vec3(0.5, 0.6, 0.7),   // cold blue (away from sun)
    vec3(1.0, 0.9, 0.7),   // warm gold (toward sun)
    pow(sunAmount, 8.0)
);
```

---

## 11. Shadow Map Coverage (P2)

### Текущее: ortho [-6, 6]. С деревьями на расстоянии ~6.5 юнитов — тень обрезается.

| Тир | Shadow map | Ortho bounds | Покрытие |
|-----|-----------|-------------|----------|
| T2 | 1024 | [-8, 8] | Все объекты включая деревья |
| T3 | 2048 | [-8, 8] | То же + point light тени |
| T4 | 4096 | [-10, 10] | Макс. покрытие |

**Будущее:** Cascaded Shadow Maps (2-3 каскада). Это отдельная задача.

---

## 12. Обновлённая камера (P2)

### Минимальная высота поднята до 0.5 (terrain hills до ~0.6)

```
r = 4.0

KP0: pos( r*0.7,  2.2, -r*0.5)  target(0, 0.2, 0)   — Обзор сверху: тени, layout
KP1: pos( r*0.6,  0.8,  r*0.6)  target(0, -0.2, 0)   — Пруд: SSR отражения (T4)
KP2: pos( r,      0.5,  0.0)    target(0, 0.1, 0)     — Через колонны: DoF
KP3: pos( 0.5,    1.5, -r*0.8)  target(0, 0, 0)       — Сквозь арку: SSAO
KP4: pos(-r*0.5,  3.2,  r*0.5)  target(0, 0, 0)       — Панорама: slope texturing
KP5: pos(-r*0.7,  0.6,  r*0.4)  target(0, 0, 0)       — Contre-jour: bloom, god rays
KP6: pos(-r,      0.5, -0.5)    target(0, 0.1, 0)     — Обелиск: point lights (T3)
KP7: pos(-r*0.5,  1.8, -r*0.6)  target(0, 0, 0)       — Разворот: деревья на фоне
KP8: pos( 0.3,    3.8,  0.3)    target(0, 0, 0)       — Зенит: кольца, GTAO
KP9: pos( r*0.7,  2.2, -r*0.5)  target(0, 0.2, 0)     — Возврат
```

---

## 13. Обновлённая сводная таблица объектов

| # | Объект | Тир | Verts | Draws | GPU-задача | Статус |
|---|--------|-----|-------|-------|-----------|--------|
| 1 | Bunny | T1+ | ~10K | 1 | Fur, PBR, SSS, tess | ✅ |
| 2 | Terrain | T1+ | 6.4K | 1 | Slope texturing | ✅ |
| 3 | Пьедестал | T1+ | ~84 | 1 | Contact shadow, SSAO | ✅ |
| 4-5 | Колонны A,B | T1+ | ~600 ea | 2 | Long shadows, vol fog | ✅ |
| 6 | Каменная плита | T1+ | 24 | 1 | Shadow bias | ✅ |
| 7-8 | Каменные сферы | T1+ | ~320 ea | 2 | PBR Fresnel | ✅ |
| 9 | Камни (batch) | T1+ | ~3K | 1 | Fill | ✅ |
| 10 | Трава (batch) | T1 | ~4K | 1 | Wind | ✅ |
| 11 | Арка | T2+ | ~400 | 1 | SSAO concavity | ⚠️ half-torus |
| 12 | Упавшая колонна | T2+ | ~500 | 1 | SSAO contact | ✅ |
| 13 | Замшелый блок | T2+ | 24 | 1 | Island material | ✅ |
| 14 | Чаша | T2+ | ~160 | 1 | SSAO concavity | ✅ |
| 15 | Трава (instanced) | T2+ | 5 templ | 1 | Instancing, wind | ✅ |
| 16-20 | Деревья (5 шт.) | T2+ | ~500 ea | 5(или 2 batch) | DoF depth layer | ❌ Новые |
| 21-22 | Колонны C,D | T3+ | ~400 ea | 2 | Point lights | ✅ |
| 23 | Обелиск | T3+ | ~100 | 1 | Light anchor | ✅ |
| 24-25 | Кольца | T3+ | ~192 ea | 2 | Shadow precision | ✅ |
| 26 | Пруд | T4 | ~130 | 1 | SSR, Fresnel | ✅ |

**Итого draw calls (с деревьями, batched):**
- T1: ~35 (без изменений)
- T2: ~39 (+2 tree batches)
- T3: ~47 (+2 tree batches + ruins)
- T4: ~45

---

## 14. Per-Tier визуальная прогрессия

### T1: Basic — "Каменный страж" (GL 2.1)

**Целевое железо:** GeForce 8600 GT (2007), ≥15 FPS
**Атмосфера:** Честный forward-рендеринг. Один свет, hemisphere ambient, exponential fog.

**Активные эффекты:**
- Blinn-Phong (spec pow 32)
- Hemisphere ambient
- Exponential fog (density 0.045, color blue-grey)
- Shell fur (24 слоя, без instancing)
- Procedural terrain coloring (slope/height)
- Vignette + color grade (per-fragment)
- Wind animation (мех, трава)

**Объекты:** Bunny, terrain, пьедестал, 2 колонны, плита, 2 сферы, камни (15), трава (800 batch), пыль (200)

**Чего нет:** Теней, SSAO, bloom, post-processing, instancing. Это T1 — показывает "что GPU рисует без трюков".

---

### T2: Enhanced — "Первый свет" (GL 3.0)

**Целевое железо:** GTX 560 (2011), ≥15 FPS
**Атмосфера:** Тени заземляют объекты. SSAO добавляет весомость. Bloom — тёплое сияние. Руины появляются.

**Дельта от T1:**
- + Shadow map (1024, PCF 3×3)
- + SSAO (16-sample, half-res, bilateral blur)
- + Bloom (extract + Gaussian blur + composite)
- + Instanced grass (30K blades → 1 draw call)
- + Instanced fur (24 shells → 1 draw call)
- + Scene-to-FBO → post-processing pipeline
- + Арка (half-torus), упавшая колонна, замшелый блок, чаша
- + 5 деревьев на дальнем плане

---

### T3: Quality — "Зачарованные сумерки" (GL 3.3)

**Целевое железо:** GTX 750 Ti (2014), ≥15 FPS
**Атмосфера:** Сцена оживает. 3 анимированных цветных point light. Normal maps. Густой мех и трава.

**Дельта от T2:**
- Shadow map → 2048, PCF 5×5
- + 3 animated point lights (orange/blue/green, orbiting)
- + Normal map texture
- + Колонны C,D (обломки), обелиск, 2 кольца
- + Камни 15 → 25
- Fur 24 → 48 shells
- Grass 30K → 50K
- Particles 300 → 500

---

### T4: Ultra — "Пробуждение святилища" (GL 4.3)

**Целевое железо:** GTX 1050 Ti (2016), ≥10 FPS
**Атмосфера:** Генерационный скачок. PBR, PCSS, SSS, volumetric fog, compute particles, tessellation, SSR, HDR, ACES.

**Замены:**
- Blinn-Phong → PBR (Cook-Torrance GGX + SSS)
- PCF → PCSS (variable penumbra, textureGather)
- SSAO → Compute GTAO (full-res, 12 dir × 6 steps)
- Fragment bloom → Compute bloom (6-level mip chain)
- Billboard particles → Compute particles (4096 GPU-sim)
- LDR composite → HDR (ACES + auto-exposure + chromatic + grain)

**Новое:**
- Tessellation (PN-tri level 6, displacement 0.03)
- Volumetric fog (64 steps, height-based, HG phase, god rays)
- SSR + Water (пруд с Fresnel, waves)
- DoF (compute, Poisson disk, focal=3.5)
- PBR material variety (металлические кольца, медная чаша, полированные сферы)

---

## 15. Приоритеты реализации

### P0 — Критические

| # | Задача | Файлы | Effort |
|---|--------|-------|--------|
| 1 | Half-torus для арки | mesh_gen.h/cpp, demo_resources.cpp | ~50 LOC |
| 2 | Terrain-following placement | demo_scene.cpp | ~30 LOC |
| 3 | Sky dome fix | sky_pass.cpp или demo_resources.cpp | ~5 LOC |

### P1 — Важные

| # | Задача | Файлы | Effort |
|---|--------|-------|--------|
| 4 | Grass exclusion zones | grass.vert, grass_instanced_pass.cpp | ~30 LOC |
| 5 | Деревья (5 шт.) | mesh_gen.h/cpp, demo_resources.cpp, demo_scene.cpp | ~80 LOC |
| 6 | PBR material variety (T4) | demo_scene.cpp (placeRuins) | ~20 LOC |

### P2 — Polish

| # | Задача | Файлы | Effort |
|---|--------|-------|--------|
| 7 | Height-based vol fog | volumetric_fog_t4.frag | ~10 LOC |
| 8 | Shadow map bounds | shadow_pass.cpp или demo_utils.h | ~5 LOC |
| 9 | Camera tuning | demo_camera.cpp | ~20 LOC |
| 10 | Terrain edge blend | island.frag | ~5 LOC |

---

## 16. Что НЕ делать (YAGNI)

Следующие фичи НЕ нужны на текущем этапе, несмотря на наличие в индустриальных решениях:

- **Cascaded Shadow Maps** — single shadow map покрывает нашу 20×20 сцену. CSM нужен для open world.
- **LOD для деревьев** — 5 деревьев по 500 вершин = 2500 вершин. LOD оправдан от сотен объектов.
- **Impostor billboards** — при 5 деревьях не нужно.
- **Tessellation на всех поверхностях** (Unigine Heaven) — только на bunny. Камень не выигрывает от PN-tri.
- **100+ light sources** (3DMark Fire Strike) — 1 dir + 3 point достаточно для showcase. Больше = другой тест.
- **Deferred rendering** — forward pipeline с progressive post-FX правильно для GL 2.1 baseline.
- **Half-res particles** (3DMark) — при 4K particles half-res даёт 2.4× speedup, но усложняет pipeline. Возможная оптимизация если T4 FPS на GTX 1050 Ti окажется <10.
