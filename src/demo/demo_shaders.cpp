#include "demo/demo_shaders.h"

namespace DemoShaders {

// ============================================================
// Tier 1: Forward Blinn-Phong + hemisphere ambient + fog
//         GLSL 1.20 (compatibility profile)
// ============================================================

const char* city_t1_vs_120 = R"(
#version 120
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
varying vec3 v_world_pos;
varying vec3 v_normal;
varying vec2 v_uv;
void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_uv = a_uv;
    gl_Position = u_proj * u_view * world;
}
)";

const char* city_t1_fs_120 = R"(
#version 120
varying vec3 v_world_pos;
varying vec3 v_normal;
varying vec2 v_uv;
uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_material_color;
uniform float u_material_specular;
uniform float u_alpha;
uniform float u_time;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);

    vec3 v = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * u_material_specular;

    // Procedural surface variation
    float noise = hash(floor(v_uv * 40.0));
    vec3 base_color = u_material_color * (0.85 + 0.15 * noise);

    // Hemisphere ambient
    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.22, 0.19, 0.16), vec3(0.42, 0.46, 0.54), up) * base_color;

    vec3 color = ambient + base_color * diff * 0.7 + vec3(spec);

    // Fog
    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, clamp(fog, 0.0, 1.0));

    gl_FragColor = vec4(color, u_alpha);
}
)";

// ============================================================
// Tier 1: GLSL 1.50 core profile
// ============================================================

const char* city_t1_vs_150 = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_uv = a_uv;
    gl_Position = u_proj * u_view * world;
}
)";

const char* city_t1_fs_150 = R"(
#version 150
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_material_color;
uniform float u_material_specular;
uniform float u_alpha;
uniform float u_time;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);

    vec3 v = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * u_material_specular;

    float noise = hash(floor(v_uv * 40.0));
    vec3 base_color = u_material_color * (0.85 + 0.15 * noise);

    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.22, 0.19, 0.16), vec3(0.42, 0.46, 0.54), up) * base_color;

    vec3 color = ambient + base_color * diff * 0.7 + vec3(spec);

    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, clamp(fog, 0.0, 1.0));

    fragColor = vec4(color, u_alpha);
}
)";

// ============================================================
// Tier 2: + shadow map, single-tap (GLSL 1.50)
// ============================================================

const char* city_t2_vs = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_light_vp;
out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_light_pos;
void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_uv = a_uv;
    v_light_pos = u_light_vp * world;
    gl_Position = u_proj * u_view * world;
}
)";

const char* city_t2_fs = R"(
#version 150
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_light_pos;
uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_material_color;
uniform float u_material_specular;
uniform float u_alpha;
uniform float u_time;
uniform sampler2DShadow u_shadow_map;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float shadowSample(vec4 lpos) {
    vec3 proj = lpos.xyz / lpos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    return mix(0.3, 1.0, texture(u_shadow_map, proj));
}

void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);

    vec3 v = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * u_material_specular;

    float shadow = shadowSample(v_light_pos);

    float noise = hash(floor(v_uv * 40.0));
    vec3 base_color = u_material_color * (0.85 + 0.15 * noise);

    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.22, 0.19, 0.16), vec3(0.42, 0.46, 0.54), up) * base_color;

    vec3 color = ambient + base_color * diff * 0.7 * shadow + vec3(spec) * shadow;

    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, clamp(fog, 0.0, 1.0));

    fragColor = vec4(color, u_alpha);
}
)";

// ============================================================
// Tier 2+: Shadow depth pass (GLSL 1.50)
// ============================================================

const char* city_shadow_vs = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_light_vp;
uniform mat4 u_model;
void main() {
    gl_Position = u_light_vp * u_model * vec4(a_pos, 1.0);
}
)";

const char* city_shadow_fs = R"(
#version 150
out vec4 fragColor;
void main() {
    fragColor = vec4(0.0);
}
)";

// ============================================================
// Tier 3: + 16-tap PCF, SSS, rim lighting (GLSL 3.30)
// ============================================================

const char* city_t3_vs = R"(
#version 330
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_light_vp;
out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_light_pos;
void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_uv = a_uv;
    v_light_pos = u_light_vp * world;
    gl_Position = u_proj * u_view * world;
}
)";

const char* city_t3_fs = R"(
#version 330
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_light_pos;
uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_material_color;
uniform float u_material_specular;
uniform float u_alpha;
uniform float u_time;
uniform sampler2DShadow u_shadow_map;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

float pcfShadow(vec4 lpos) {
    vec3 proj = lpos.xyz / lpos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    float shadow = 0.0;
    float spread = 1.5 / 2048.0;
    for (int i = 0; i < 16; i++) {
        vec3 coord = vec3(proj.xy + poissonDisk[i] * spread, proj.z);
        shadow += texture(u_shadow_map, coord);
    }
    return mix(0.2, 1.0, shadow / 16.0);
}

void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);

    vec3 v = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), 64.0) * u_material_specular;

    // Rim lighting
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0) * 0.12;

    float shadow = pcfShadow(v_light_pos);

    float noise = hash(floor(v_uv * 40.0));
    vec3 base_color = u_material_color * (0.85 + 0.15 * noise);

    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.20, 0.17, 0.14), vec3(0.40, 0.44, 0.52), up) * base_color;

    vec3 color = ambient + base_color * diff * 0.72 * shadow
               + vec3(spec) * shadow + vec3(rim);

    // Subsurface scattering: light wrap-around
    float wrap = max(0.0, dot(-n, l) + 0.3) / 1.3;
    vec3 sss = base_color * vec3(1.0, 0.7, 0.3) * wrap * wrap * 0.15;
    color += sss;

    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, clamp(fog, 0.0, 1.0));

    fragColor = vec4(color, u_alpha);
}
)";

// ============================================================
// Tier 4: PBR Cook-Torrance (GLSL 4.30)
// ============================================================

const char* city_t4_vs = R"(
#version 430
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_light_vp;
out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_light_pos;
void main() {
    vec4 world = u_model * vec4(a_pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_uv = a_uv;
    v_light_pos = u_light_vp * world;
    gl_Position = u_proj * u_view * world;
}
)";

const char* city_t4_fs = R"(
#version 430
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_light_pos;
uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_material_color;
uniform float u_material_specular;
uniform float u_alpha;
uniform float u_time;
uniform float u_roughness;
uniform float u_metallic;
uniform sampler2DShadow u_shadow_map;
out vec4 fragColor;

const float PI = 3.14159265359;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

float pcfShadow(vec4 lpos) {
    vec3 proj = lpos.xyz / lpos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0) return 1.0;
    float shadow = 0.0;
    float spread = 1.5 / 4096.0;
    for (int i = 0; i < 16; i++) {
        vec3 coord = vec3(proj.xy + poissonDisk[i] * spread, proj.z);
        shadow += texture(u_shadow_map, coord);
    }
    return mix(0.15, 1.0, shadow / 16.0);
}

float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness) {
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    vec3 v = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + v);

    float NdotL = max(dot(n, l), 0.0);
    float NdotV = max(dot(n, v), 0.001);
    float NdotH = max(dot(n, h), 0.0);
    float HdotV = max(dot(h, v), 0.0);

    // Procedural surface variation
    float noise = hash(floor(v_uv * 40.0));
    vec3 albedo = u_material_color * (0.85 + 0.15 * noise);

    vec3 F0 = mix(vec3(0.04), albedo, u_metallic);

    float D = D_GGX(NdotH, u_roughness);
    float G = G_Smith(NdotV, NdotL, u_roughness);
    vec3 F = F_Schlick(HdotV, F0);

    vec3 spec = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - u_metallic);

    float shadow = pcfShadow(v_light_pos);

    vec3 radiance = vec3(3.0, 2.8, 2.5);
    vec3 Lo = (kD * albedo / PI + spec) * radiance * NdotL * shadow;

    // Hemisphere ambient
    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.18, 0.15, 0.12), vec3(0.36, 0.40, 0.48), up) * albedo;

    // Rim lighting
    float rim = pow(1.0 - NdotV, 4.0) * 0.10;

    // Subsurface scattering
    float wrap = max(0.0, dot(-n, l) + 0.3) / 1.3;
    vec3 sss = albedo * vec3(1.0, 0.7, 0.3) * wrap * wrap * 0.12;

    vec3 color = ambient + Lo + vec3(rim) + sss;

    // Fog
    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, clamp(fog, 0.0, 1.0));

    fragColor = vec4(color, u_alpha);
}
)";

// ============================================================
// Bloom blur (9-tap Gaussian, GLSL 1.50)
// ============================================================

const char* bloom_blur_vs = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

const char* bloom_blur_fs = R"(
#version 150
in vec2 v_uv;
uniform sampler2D u_tex;
uniform vec2 u_direction;
uniform float u_threshold;
out vec4 fragColor;

vec3 fetchSample(vec2 uv) {
    vec3 c = texture(u_tex, uv).rgb;
    if (u_threshold >= 0.0) {
        float b = dot(c, vec3(0.2126, 0.7152, 0.0722));
        c *= max(0.0, b - u_threshold);
    }
    return c;
}

void main() {
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 result = fetchSample(v_uv) * weights[0];
    for (int i = 1; i < 5; i++) {
        vec2 off = u_direction * float(i);
        result += fetchSample(v_uv + off) * weights[i];
        result += fetchSample(v_uv - off) * weights[i];
    }
    fragColor = vec4(result, 1.0);
}
)";

// ============================================================
// T2: Bloom composite + Reinhard + vignette
// ============================================================

const char* bloom_composite_vs = R"(
#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

const char* bloom_composite_fs = R"(
#version 150
in vec2 v_uv;
uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_bloom_intensity;
out vec4 fragColor;
void main() {
    vec3 scene = texture(u_scene, v_uv).rgb;
    vec3 bloom = texture(u_bloom, v_uv).rgb;
    vec3 color = scene + bloom * u_bloom_intensity;

    // Reinhard tone mapping
    color = color / (color + vec3(1.0));

    // Vignette
    vec2 q = v_uv - 0.5;
    float vignette = 1.0 - dot(q, q) * 1.5;
    color *= clamp(vignette, 0.0, 1.0);

    fragColor = vec4(color, 1.0);
}
)";

// ============================================================
// T3: Bloom composite + chromatic aberration
// ============================================================

const char* bloom_composite_t3_vs = R"(
#version 330
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

const char* bloom_composite_t3_fs = R"(
#version 330
in vec2 v_uv;
uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_bloom_intensity;
out vec4 fragColor;
void main() {
    vec2 center = v_uv - 0.5;
    float ca_amount = 0.003 * length(center);
    vec2 ca_dir = normalize(center + 0.0001);
    float r = texture(u_scene, v_uv + ca_dir * ca_amount).r;
    float g = texture(u_scene, v_uv).g;
    float b = texture(u_scene, v_uv - ca_dir * ca_amount).b;
    vec3 scene = vec3(r, g, b);

    vec3 bloom = texture(u_bloom, v_uv).rgb;
    vec3 color = scene + bloom * u_bloom_intensity;

    color = color / (color + vec3(1.0));

    float vignette = 1.0 - dot(center, center) * 1.5;
    color *= clamp(vignette, 0.0, 1.0);

    fragColor = vec4(color, 1.0);
}
)";

// ============================================================
// T4: Bloom composite + CA + god rays + ACES + film grain
// ============================================================

const char* bloom_composite_t4_vs = R"(
#version 430
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
)";

const char* bloom_composite_t4_fs = R"(
#version 430
in vec2 v_uv;
uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_bloom_intensity;
uniform vec2 u_sun_pos;
uniform float u_ray_density;
uniform float u_ray_decay;
uniform float u_time;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    // Chromatic aberration
    vec2 center = v_uv - 0.5;
    float ca_amount = 0.004 * length(center);
    vec2 ca_dir = normalize(center + 0.0001);
    float r = texture(u_scene, v_uv + ca_dir * ca_amount).r;
    float g = texture(u_scene, v_uv).g;
    float b = texture(u_scene, v_uv - ca_dir * ca_amount).b;
    vec3 scene = vec3(r, g, b);

    vec3 bloom = texture(u_bloom, v_uv).rgb;

    // God rays: radial blur from sun position
    vec2 dir = v_uv - u_sun_pos;
    float dist_to_sun = length(dir);
    vec2 ray_dir = dir / max(dist_to_sun, 0.001);
    vec3 rays = vec3(0.0);
    float decay = 1.0;
    const int NUM_SAMPLES = 32;
    float step_size = u_ray_density / float(NUM_SAMPLES);
    vec2 uv = v_uv;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        uv -= ray_dir * step_size;
        vec3 samp = texture(u_scene, clamp(uv, 0.0, 1.0)).rgb;
        float brightness = dot(samp, vec3(0.2126, 0.7152, 0.0722));
        rays += samp * brightness * decay;
        decay *= u_ray_decay;
    }
    rays /= float(NUM_SAMPLES);

    vec3 color = scene + bloom * u_bloom_intensity + rays * 0.4;

    // ACES tone mapping
    color = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
    color = clamp(color, 0.0, 1.0);

    // Vignette
    float vignette = 1.0 - dot(center, center) * 1.5;
    color *= clamp(vignette, 0.0, 1.0);

    // Film grain
    float grain = (hash(v_uv * 1000.0 + vec2(u_time * 137.0, u_time * 219.0)) - 0.5) * 0.04;
    color += grain;

    fragColor = vec4(color, 1.0);
}
)";

} // namespace DemoShaders
