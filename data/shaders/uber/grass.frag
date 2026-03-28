// Uber grass fragment shader — version-portable via ShaderCache preamble.
// Feature guards: HAS_SHADOWS, HAS_SHADOW_PCF5, HAS_POINT_LIGHTS
FS_IN vec3 v_world_pos;
FS_IN vec3 v_normal;
FS_IN vec2 v_uv;
FS_IN float v_color_t;
#ifdef HAS_SHADOWS
FS_IN vec4 v_light_pos;
#endif

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;

#ifdef HAS_SHADOWS
uniform sampler2D u_shadow_map;
uniform vec2 u_shadow_texel_size;
uniform float u_has_shadow;
#endif

#ifdef HAS_POINT_LIGHTS
uniform vec3 u_point_lights[3];
uniform vec3 u_point_colors[3];
uniform int u_point_light_count;
#endif

#ifdef HAS_SHADOWS
float computeShadow() {
    if (u_has_shadow < 0.5) return 1.0;
    vec3 proj = v_light_pos.xyz / v_light_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float bias = 0.003;
    float shadow = 0.0;
#ifdef HAS_SHADOW_PCF5
    float texel = u_shadow_texel_size.x;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            float d = COMPAT_TEX2D(u_shadow_map, proj.xy + vec2(float(x), float(y)) * texel).r;
            shadow += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    }
    shadow /= 25.0;
#else
    float texel = u_shadow_texel_size.x;
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

// Simple hash for grass color variation (no noise_lib include in uber grass)
float grassHash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    // Color gradient: dark green at base, lighter yellow-green at tip
    // World-space variation: each blade cluster gets a unique tint
    vec2 cell = floor(v_world_pos.xz * 2.0);
    float var = grassHash(cell);
    float var2 = grassHash(cell + vec2(17.0, 31.0));

    // Vary base between dark forest green and warm olive
    vec3 base_color = mix(vec3(0.12, 0.30, 0.06), vec3(0.20, 0.32, 0.10), var);
    // Vary tip between yellow-green and bright green
    vec3 tip_color = mix(vec3(0.35, 0.55, 0.15), vec3(0.45, 0.62, 0.22), var2);
    vec3 grass_color = mix(base_color, tip_color, v_color_t);

    // Simple lighting
    vec3 normal = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    vec3 vd = normalize(u_cam_pos - v_world_pos);
    float diff = max(0.0, (dot(normal, l) + 0.5) / 1.5); // wrapped diffuse

    // Hemisphere ambient
    float up = dot(normal, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.15, 0.12, 0.08), vec3(0.35, 0.40, 0.45), up) * 0.4;

#ifdef HAS_SHADOWS
    float shadow = computeShadow();
#else
    float shadow = 1.0;
#endif
    vec3 color = ambient * grass_color + grass_color * diff * 0.7 * shadow;

    // Point lights
#ifdef HAS_POINT_LIGHTS
    for (int i = 0; i < u_point_light_count && i < 3; i++) {
        vec3 to_light = u_point_lights[i] - v_world_pos;
        float d = length(to_light);
        vec3 pl_dir = to_light / d;
        float atten = 1.0 / (1.0 + 0.3 * d + 0.4 * d * d);

        float pl_diff = max(0.0, (dot(normal, pl_dir) + 0.5) / 1.5);
        color += grass_color * pl_diff * 0.7 * u_point_colors[i] * atten;
    }
#endif

    // Fog
    float cam_dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-cam_dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // Alpha: fade at very tip for soft edges
    float alpha = smoothstep(1.0, 0.85, v_color_t);
    if (alpha < 0.01) discard;

    FRAG_COLOR = vec4(color, alpha);
}
