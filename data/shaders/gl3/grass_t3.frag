#version 330
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_light_pos;
in float v_color_t;

out vec4 FragColor;

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform sampler2D u_shadow_map;
uniform float u_has_shadow;
uniform vec3 u_point_lights[3];
uniform vec3 u_point_colors[3];
uniform int u_point_light_count;

float computeShadow() {
    if (u_has_shadow < 0.5) return 1.0;
    vec3 proj = v_light_pos.xyz / v_light_pos.w;
    proj = proj * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float bias = 0.003;
    float texel = 1.0 / 2048.0;
    float shadow = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            float d = texture(u_shadow_map, proj.xy + vec2(float(x), float(y)) * texel).r;
            shadow += (proj.z - bias > d) ? 0.0 : 1.0;
        }
    }
    shadow /= 25.0;
    return mix(0.3, 1.0, shadow);
}

void main() {
    // Color gradient: dark green at base, lighter yellow-green at tip
    vec3 base_color = vec3(0.15, 0.35, 0.08);
    vec3 tip_color = vec3(0.40, 0.60, 0.20);
    vec3 grass_color = mix(base_color, tip_color, v_color_t);

    // Simple lighting
    vec3 normal = normalize(v_normal);
    vec3 l = normalize(u_light_dir);
    vec3 vd = normalize(u_cam_pos - v_world_pos);
    float diff = max(0.0, (dot(normal, l) + 0.5) / 1.5); // wrapped diffuse

    // Hemisphere ambient
    float up = dot(normal, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.15, 0.12, 0.08), vec3(0.35, 0.40, 0.45), up) * 0.4;

    float shadow = computeShadow();
    vec3 color = ambient * grass_color + grass_color * diff * 0.7 * shadow;

    // Point lights
    for (int i = 0; i < u_point_light_count && i < 3; i++) {
        vec3 to_light = u_point_lights[i] - v_world_pos;
        float d = length(to_light);
        vec3 pl_dir = to_light / d;
        float atten = 1.0 / (1.0 + 0.3 * d + 0.4 * d * d);

        float pl_diff = max(0.0, (dot(normal, pl_dir) + 0.5) / 1.5);
        color += grass_color * pl_diff * 0.7 * u_point_colors[i] * atten;
    }

    // Fog
    float cam_dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-cam_dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // Alpha: fade at very tip for soft edges
    float alpha = smoothstep(1.0, 0.85, v_color_t);
    if (alpha < 0.01) discard;

    FragColor = vec4(color, alpha);
}
