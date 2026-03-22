#version 150
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
out vec4 FragColor;
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
uniform vec2 u_viewport_size;
#pragma include "noise_lib.glsl"
#pragma include "terrain_color.glsl"
void main() {
    vec3 n = normalize(v_normal);
    if (u_normal_strength > 0.0)
        n = perturbNormal(n, v_world_pos, v_uv, u_normal_strength);

    vec3 l = normalize(u_light_dir);
    float diff = max(dot(n, l), 0.0);

    vec3 vd = normalize(u_cam_pos - v_world_pos);
    vec3 h = normalize(l + vd);
    float spec = pow(max(dot(n, h), 0.0), 32.0) * u_mat_spec;

    vec3 base_color = surfaceColor(v_world_pos, n, v_uv, u_mat_color, u_proc_tex);

    float up = dot(n, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.22, 0.19, 0.16), vec3(0.42, 0.46, 0.54), up) * base_color;

    vec3 color = ambient + base_color * diff * 0.7 + vec3(1.0) * spec;

    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // Vignette: darken edges
    vec2 screen_uv = gl_FragCoord.xy / u_viewport_size;
    float vignette = smoothstep(0.9, 0.35, length(screen_uv - 0.5));
    color *= vignette;

    // Color grade: warm shift + contrast
    color = pow(color, vec3(0.95, 1.0, 1.08));
    color = mix(vec3(0.5), color, 1.15);
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, u_alpha);
}
