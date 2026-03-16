#pragma once

// All GLSL shader source strings for demo mode (city scene).

namespace DemoShaders {

// Tier 1: forward Blinn-Phong + fog (GLSL 1.20 / 1.50)
extern const char* city_t1_vs_120;
extern const char* city_t1_fs_120;
extern const char* city_t1_vs_150;
extern const char* city_t1_fs_150;

// Tier 2: + shadow map (GLSL 1.50)
extern const char* city_t2_vs;
extern const char* city_t2_fs;

// Tier 2+: shadow depth pass (GLSL 1.50)
extern const char* city_shadow_vs;
extern const char* city_shadow_fs;

// Tier 3: + PCF shadows, SSS, rim lighting (GLSL 3.30)
extern const char* city_t3_vs;
extern const char* city_t3_fs;

// Tier 4: PBR Cook-Torrance (GLSL 4.30)
extern const char* city_t4_vs;
extern const char* city_t4_fs;

// Bloom blur (9-tap Gaussian, GLSL 1.50)
extern const char* bloom_blur_vs;
extern const char* bloom_blur_fs;

// Bloom composite + vignette (T2, GLSL 1.50)
extern const char* bloom_composite_vs;
extern const char* bloom_composite_fs;

// Bloom composite + chromatic aberration (T3, GLSL 3.30)
extern const char* bloom_composite_t3_vs;
extern const char* bloom_composite_t3_fs;

// Bloom composite + god rays + ACES + film grain (T4, GLSL 4.30)
extern const char* bloom_composite_t4_vs;
extern const char* bloom_composite_t4_fs;

} // namespace DemoShaders
