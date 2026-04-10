# План реализации: Forgotten Sanctuary — по тирам

Основан на `docs/scene_design_v2.md`. Реализация последовательная: довести T1 до нужного уровня, затем T2, T3, T4. Каждый тир считается завершённым после визуальной проверки.

---

## Фаза 1: T1 Basic

**Цель:** Все T1-объекты корректно размещены на рельефе, небо без артефактов, трава не прорастает через пьедестал. Визуально — "каменное святилище на холмистой поляне".

**Целевое железо:** GeForce 8600 GT (2007), ≥15 FPS.
**Draw call бюджет:** ~35 (10 objects + 23 fur shells + sky + particles).

### Задача 1.1: Sky dome fix (P0)

**Проблема:** `MeshGen::sphere(32, 16)` = unit sphere (radius 1.0). Камера на расстоянии 3.8 — она СНАРУЖИ сферы. Sky pass рисует с `setDepthTest(false)`, но вершины sphere за камерой → фрагменты не покрывают экран.

**Решение:** Увеличить sky mesh до radius 50.

**Файл:** `src/demo/demo_resources.cpp`, функция `loadSharedMeshes()`.

**Изменение:**
```cpp
// Было:
MeshData sd = MeshGen::sphere(32, 16);
sky_mesh_.assign(r, r->createMesh(sd));

// Стало:
MeshData sd = MeshGen::sphere(32, 16);
for (size_t i = 0; i < sd.vertices.size(); i++)
    sd.vertices[i].pos = sd.vertices[i].pos * 50.0f;
sky_mesh_.assign(r, r->createMesh(sd));
```

**Проверка:** T1 demo — небо полностью покрывает фон, нет чёрных областей.

---

### Задача 1.2: Terrain-following placement (P0)

**Проблема:** Все объекты размещены на `Y=-1.0`. Terrain имеет heightmap (холмы до ±0.6). Объекты висят в воздухе или проваливаются в склоны.

**Решение:** Добавить `sampleTerrainHeight(x, z)` и использовать в place-методах.

**Файл:** `src/demo/demo_scene.cpp`

**Изменение 1:** Добавить static функцию в начало файла (после #include):
```cpp
static float sampleTerrainHeight(float x, float z) {
    float h = 0.0f;
    h += sinf(x * 0.4f) * cosf(z * 0.3f) * 0.6f;
    h += sinf(x * 0.7f + 1.3f) * sinf(z * 0.5f + 0.7f) * 0.3f;
    float pdx = x - 3.5f, pdz = z - 3.5f;
    float pd = sqrtf(pdx * pdx + pdz * pdz);
    float pt = pd < 3.0f ? (3.0f - pd) / 3.0f : 0.0f;
    pt = pt * pt * (3.0f - 2.0f * pt);
    h -= pt * 0.3f;
    float cd = sqrtf(x * x + z * z);
    float ft = cd < 2.5f ? (cd < 1.0f ? 1.0f : (2.5f - cd) / 1.5f) : 0.0f;
    ft = ft * ft * (3.0f - 2.0f * ft);
    h *= (1.0f - ft);
    return h - 1.0f;
}
```

**Изменение 2:** Обновить place-методы для T1-объектов:
- `placeColumns()`: `translate(±1.8, sampleTerrainHeight(±1.8, -1.5), -1.5)`
- `placeRuins()` — плита: `translate(1.2, sampleTerrainHeight(1.2, 0.8) + 0.1, 0.8)` (+0.1 чтобы не в земле)
- `placeRuins()` — сферы: аналогично, Y = sampleTerrainHeight + offset
- `placePedestal()`: центр (0,0) — terrain flattened, h≈-1.0, не требует изменений

**Не трогать:**
- Пьедестал (центр выровнен в heightmap, h=-1.0)
- Кольца (центр, T3+)
- Пруд (фиксированная депрессия, T4)

**Проверка:** Запустить demo T1, визуально убедиться что колонны и камни стоят на рельефе.

---

### Задача 1.3: Grass exclusion вокруг пьедестала (P1)

**Проблема:** Батчевая трава (T1) генерируется в `MeshGen::scatteredGrass()` с min distance от центра 1.2. Пьедестал radius 0.8, высота 0.9 — трава прорастает у основания.

**Решение:** Увеличить min distance в `scatteredGrass()` или в вызове `loadSharedMeshes()`.

**Файл:** `src/demo/demo_resources.cpp`

**Изменение:** В генерации scattered grass — увеличить min distance, чтобы трава не росла ближе 2.0 от центра (пьедестал 0.8 + колонны):
```cpp
// Уже используется seed 251u и area 20.0
// В scatteredGrass функции, min_dist_from_center нужно передать или увеличить
// Проверить: посмотреть текущую реализацию scatteredGrass — есть ли exclusion
```

Нужно проверить `MeshGen::scatteredGrass()` — если там захардкожен min distance, увеличить его.

**Проверка:** T1 — трава не растёт на пьедестале и у его основания.

---

### Задача 1.4: Камера — поднять минимальную высоту (P2)

**Проблема:** KP1 (Y=0.3) и KP2 (Y=0.2) — камера ниже terrain hills (до ~0.6 над base).

**Решение:** Поднять минимальную высоту камеры до ≥0.5.

**Файл:** `src/demo/demo_camera.cpp`

**Изменение:** Обновить keypoints с низкой высотой:
```
KP1: Y 0.3 → 0.8
KP2: Y 0.2 → 0.5
KP5: Y 0.4 → 0.6
KP6: Y 0.3 → 0.5
```

**Проверка:** T1 — камера не проваливается под terrain ни в одной точке орбиты.

---

### Задача 1.5: Визуальная верификация T1

**Чеклист:**
- [ ] Небо без чёрных областей при всех ракурсах камеры
- [ ] Пьедестал виден над травой (шестигранник)
- [ ] Колонны A, B стоят на рельефе (не висят в воздухе)
- [ ] Плита и сферы на terrain
- [ ] Трава не прорастает через пьедестал
- [ ] Terrain slope coloring работает (трава на плоском, камень на крутом)
- [ ] Мех кролика шевелится от ветра
- [ ] Пыльные частицы видны
- [ ] Vignette + color grade
- [ ] FPS приемлемый (на текущем железе)
- [ ] Камера не проваливается под terrain

**Критерий завершения:** Все чекбоксы отмечены. T1 визуально выглядит как "каменный страж на холмистой поляне" — цельная сцена, не набор случайных объектов.

---

## Фаза 2: T2 Enhanced

**Предусловие:** T1 завершён и визуально проверен.

**Цель:** Тени от колонн на terrain, SSAO у пьедестала, арка = полуарка, bloom. Деревья на дальнем плане. Instanced grass с exclusion zones.

### Задача 2.1: Half-torus для арки (P0)

**Файлы:** `src/geometry/mesh_gen.h/.cpp`, `src/demo/demo_resources.cpp`
- Добавить `MeshGen::halfTorus(ring_seg, tube_seg, R, r)` — u ∈ [0, PI]
- Заменить `MeshGen::torus(32, 12, 1.2, 0.12)` на `MeshGen::halfTorus(32, 12, 1.2, 0.12)` в demo_resources
- Обновить трансформ арки: основания на ground level, вершина на y = R = 1.2 над землёй

### Задача 2.2: Terrain-following для T2-объектов

**Файл:** `src/demo/demo_scene.cpp`
- Упавшая колонна: Y = sampleTerrainHeight(-2.0, 0.5) + offset
- Замшелый блок: Y = sampleTerrainHeight(2.5, 1.5) + offset
- Чаша: Y ≈ -1.0 (у основания пьедестала, центр выровнен)

### Задача 2.3: Деревья (5 шт.)

**Файлы:** `src/geometry/mesh_gen.h/.cpp`, `src/demo/demo_resources.h/.cpp`, `src/demo/demo_scene.cpp`, `src/demo/tier_resource_view.h`
- Добавить `MeshGen::simpleTree(trunk_h, trunk_r, crown_h, crown_r, segments)` — cylinder + 2 cones через appendMesh
- Генерировать 1-2 tree mesh варианта (разный seed для формы)
- 5 деревьев на расстоянии 5-7 юнитов от центра
- Gate: T2+ (`config_.enable_shadows`)
- Материалы: ствол brown, крона green + vertex_wind

### Задача 2.4: Instanced grass exclusion zones

**Файлы:** `data/shaders/uber/grass.vert`, `src/demo/passes/grass_instanced_pass.cpp`
- Добавить `#ifdef HAS_EXCLUSION_ZONES` в grass vertex shader
- Uniform array `u_exclusion[8]` (xyz=pos, w=radius)
- Discard instances ближе exclusion radius (gl_Position behind near plane)
- Set uniforms в GrassInstancedPass для пьедестала, колонн, пруда

### Задача 2.5: Shadow map bounds

**Файл:** `src/demo/passes/shadow_pass.cpp` или `src/demo/demo_utils.h`
- Расширить ortho bounds с [-6,6] до [-8,8] для покрытия деревьев

### Задача 2.6: Визуальная верификация T2

**Чеклист:**
- [ ] Арка — полуарка (основания на земле, дуга вверх)
- [ ] Тени от колонн на terrain
- [ ] Тени от арки
- [ ] Тени от деревьев
- [ ] SSAO у основания пьедестала (тёмная полоса)
- [ ] SSAO внутри арки (затемнение вогнутости)
- [ ] Bloom на ярких точках (солнечный край арки)
- [ ] Instanced grass (30K) НЕ растёт через пьедестал, колонны, пруд
- [ ] Упавшая колонна, замшелый блок, чаша видны и стоят на рельефе
- [ ] 5 деревьев на дальнем плане (тёмные силуэты с кронами)
- [ ] Камера не проваливается под terrain

---

## Фаза 3: T3 Quality

**Предусловие:** T2 завершён.

**Цель:** 3 цветных point light, normal maps, обломки колонн, обелиск, кольца. Густой мех и трава.

### Задача 3.1: Terrain-following для T3-объектов

- Колонны C, D: Y = sampleTerrainHeight(±2.8, -0.5)
- Обелиск: Y = sampleTerrainHeight(-3.5, -2.0)
- Кольца: центр (0,0) — terrain flat, Y≈-0.95/-0.97 (поднять чуть выше ground)

### Задача 3.2: Point light positions

**Файл:** `src/demo/demo_utils.h`
- Привязать орбиты point lights к новым объектам:
  - Light 0 (оранжевый): орбита вокруг обелиска (-3.5, -2.0)
  - Light 1 (синий): орбита вокруг арки (0, -1.8)
  - Light 2 (зелёный): орбита вокруг пруда (3.5, 3.5) — для подготовки T4

### Задача 3.3: Визуальная верификация T3

**Чеклист:**
- [ ] 3 цветных пятна light двигаются по сцене
- [ ] Normal map текстура на камне видна (шершавость)
- [ ] Обелиск, кольца, обломки колонн видны
- [ ] PCF 5×5 — мягкие тени (визуально мягче чем T2)
- [ ] 48 fur shells — мех заметно плотнее чем T2
- [ ] 50K instanced grass — заметно гуще
- [ ] Камни 25 шт (больше чем T2)

---

## Фаза 4: T4 Ultra

**Предусловие:** T3 завершён.

**Цель:** PBR material variety, пруд с SSR, volumetric fog с height falloff, compute particles. Кинематографическая картинка.

### Задача 4.1: PBR material variety

**Файл:** `src/demo/demo_scene.cpp`
- В `placeRuins()`: если `config_.enable_pbr` — перезаписать roughness/metallic для каждого объекта:
  - Сферы: roughness 0.35 (отполированные, яркий Fresnel)
  - Кольца: metallic 0.8, roughness 0.25 (бронза)
  - Чаша: metallic 0.6, roughness 0.40 (медь)
  - Обелиск: metallic 0.15, roughness 0.50 (полу-металл)
  - Пьедестал: roughness 0.65 (полированный камень)

### Задача 4.2: Height-based volumetric fog

**Файл:** `data/shaders/gl4/volumetric_fog_t4.frag`
- Заменить однородную density на `base * exp(-(y+1) * falloff)`
- Добавить sun-colored inscattering: cold blue vs warm gold по dot(rayDir, sunDir)

### Задача 4.3: Compute particles emitter position

**Файл:** `src/demo/passes/compute_particles_pass.cpp`
- Обновить emitter position если нужно
- Убедиться что 4096 particles не просаживают GTX 1050 Ti ниже 10 FPS

### Задача 4.4: Визуальная верификация T4

**Чеклист:**
- [ ] PBR: разные roughness видны (сферы блестят, камень матовый)
- [ ] Металлические кольца (Fresnel sheen, бронзовый цвет)
- [ ] Медная чаша (patina look)
- [ ] Пруд с SSR (отражение колонн, арки, неба)
- [ ] Volumetric fog в низинах (густой у земли, прозрачный выше)
- [ ] God rays к солнцу (через vol fog)
- [ ] Compute particles (светлячки/угольки)
- [ ] DoF: деревья на фоне размыты
- [ ] Tessellation на bunny (гладкие edges)
- [ ] ACES tone mapping (правильный цветовой баланс)
- [ ] SSS на ушах bunny при контровом свете

---

## Порядок работы

```
Фаза 1 (T1):  1.1 → 1.2 → 1.3 → 1.4 → 1.5 (verify)
                ↓ T1 OK
Фаза 2 (T2):  2.1 → 2.2 → 2.3 → 2.4 → 2.5 → 2.6 (verify)
                ↓ T2 OK
Фаза 3 (T3):  3.1 → 3.2 → 3.3 (verify)
                ↓ T3 OK
Фаза 4 (T4):  4.1 → 4.2 → 4.3 → 4.4 (verify)
                ↓ T4 OK
```

Каждая фаза заканчивается скриншотом + визуальной проверкой по чеклисту. Переход к следующей фазе только после подтверждения.
