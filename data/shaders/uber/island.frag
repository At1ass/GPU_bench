// Uber island fragment shader — T1/T2/T3 unified via ShaderCache preamble defines.
// Active features controlled by: HAS_SHADOWS, HAS_SHADOW_PCF5, HAS_NORMAL_MAP,
//                                 HAS_POINT_LIGHTS, HAS_VIGNETTE
#pragma include "noise_lib.glsl"
#pragma include "terrain_color.glsl"

FS_IN vec3 v_world_pos;
FS_IN vec3 v_normal;
FS_IN vec2 v_uv;

#ifdef HAS_SHADOWS
FS_IN vec4 v_light_pos;
uniform sampler2D u_shadow_map;
uniform vec2 u_shadow_texel_size;
uniform float u_has_shadow;
#endif

#ifdef HAS_NORMAL_MAP
uniform sampler2D u_normal_map;
uniform float u_has_normal_map;
#endif

#ifdef HAS_POINT_LIGHTS
uniform vec3 u_point_lights[3];
uniform vec3 u_point_colors[3];
uniform int u_point_light_count;
#endif

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_mat_color;
uniform float u_mat_spec;
uniform float u_alpha;
uniform float u_time;
uniform float u_normal_strength;
uniform float u_proc_tex;

#ifdef HAS_VIGNETTE
uniform vec2 u_viewport_size;
#endif

// ---------- Shadow computation ----------
#ifdef HAS_SHADOWS
float computeShadow() {
    if (u_has_shadow < 0.5) return 1.0;
    vec3 proj = v_light_pos.xyz / v_light_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float bias = 0.003;
    float texel = u_shadow_texel_size.x;
    float shadow = 0.0;
#ifdef HAS_SHADOW_PCF5
    // 5x5 PCF kernel (T3)
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            float d = COMPAT_TEX2D(u_shadow_map, proj.xy + vec2(float(x), float(y)) * texel).r;
            shadow += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    }
    shadow /= 25.0;
#else
    // 3x3 PCF kernel (T2 default)
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float d = COMPAT_TEX2D(u_shadow_map, proj.xy + vec2(float(x), float(y)) * texel).r;
            shadow += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;
#endif
    return mix(0.3, 1.0, shadow);
}
#endif

// ---------- Main ----------
void main() {
    vec3 n = normalize(v_normal);

    // Normal map blending (T3+)
#ifdef HAS_NORMAL_MAP
    if (u_has_normal_map > 0.5) {
        vec3 nm = COMPAT_TEX2D(u_normal_map, v_uv).rgb * 2.0 - 1.0;
        // Simple tangent-space approximation: blend normal map with geometric normal
        n = normalize(n + nm * 0.5);
    }
#endif

    if (u_normal_strength > 0.0)
        n = perturbNormal(n, v_world_pos, v_uv, u_normal_strength);

    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);

    vec3 vd = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + vd);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * u_mat_spec;

    // Procedural terrain coloring
    vec3 base_color = surfaceColor(v_world_pos, n, v_uv, u_mat_color, u_proc_tex);

    // Hemisphere ambient
    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 sky_col = vec3(0.42, 0.46, 0.54);
    vec3 ground_col = vec3(0.22, 0.19, 0.16);
    vec3 ambient = mix(ground_col, sky_col, up) * base_color;

    // Lighting
#ifdef HAS_SHADOWS
    float shadow = computeShadow();
    vec3 color = ambient + (base_color * diff * 0.7 + vec3(1.0) * spec) * shadow;
#else
    vec3 color = ambient + base_color * diff * 0.7 + vec3(1.0) * spec;
#endif

    // Point lights (T3+)
#ifdef HAS_POINT_LIGHTS
    for (int i = 0; i < u_point_light_count && i < 3; i++) {
        vec3 to_light = u_point_lights[i] - v_world_pos;
        float d = length(to_light);
        vec3 pl_dir = to_light / d;
        float atten = 1.0 / (1.0 + 0.3 * d + 0.4 * d * d);

        float pl_diff = max(dot(n, pl_dir), 0.0);
        vec3 pl_h = normalize(pl_dir + vd);
        float pl_spec = pow(max(dot(n, pl_h), 0.0), 32.0) * u_mat_spec;

        color += (base_color * pl_diff * 0.7 + vec3(1.0) * pl_spec) * u_point_colors[i] * atten;
    }
#endif

    // Fog
    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // Vignette + color grading (T1)
#ifdef HAS_VIGNETTE
    // Vignette: darken edges
    vec2 screen_uv = gl_FragCoord.xy / u_viewport_size;
    float vignette = smoothstep(0.9, 0.35, length(screen_uv - 0.5));
    color *= vignette;

    // Color grade: warm shift + contrast
    color = pow(color, vec3(0.95, 1.0, 1.08));
    color = mix(vec3(0.5), color, 1.15);
    color = clamp(color, 0.0, 1.0);
#endif

    FRAG_COLOR = vec4(color, u_alpha);
}
