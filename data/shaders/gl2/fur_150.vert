#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;

uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform float u_shell_index;
uniform float u_fur_length;
uniform float u_time;
uniform vec3 u_wind_dir;

out vec3 v_world_pos;
out vec3 v_world_normal;
out vec3 v_obj_pos;
out vec2 v_uv;
out float v_shell_index;

void main() {
    float h = u_shell_index;

    vec3 fur_dir = a_normal;

    // Gravity bend (quadratic, tips droop more)
    vec3 gravity_bend = vec3(0.0, -1.0, 0.0) * 0.35 * h * h;

    // Wind bend (multi-frequency, spatially varying)
    float wind_phase = dot(a_pos, vec3(1.0, 0.5, 0.7)) * 4.0;
    float wind_main = sin(u_time * 2.0 + wind_phase);
    float wind_gust = sin(u_time * 5.3 + wind_phase * 2.3) * 0.3;
    float wind_turbulence = sin(u_time * 8.7 + dot(a_pos, vec3(0.3, 0.0, 1.0)) * 11.0) * 0.1;
    float wind_strength = (wind_main + wind_gust + wind_turbulence) * h * h;
    vec3 wind_bend = u_wind_dir * wind_strength * 0.4;

    fur_dir += gravity_bend + wind_bend;
    fur_dir = normalize(fur_dir);

    vec3 offset = fur_dir * u_fur_length * h;
    vec3 displaced = a_pos + offset;

    vec4 world = u_model * vec4(displaced, 1.0);
    v_world_pos = world.xyz;
    v_world_normal = normalize(mat3(u_model) * a_normal);
    v_obj_pos = a_pos;
    v_uv = a_uv;
    v_shell_index = h;

    gl_Position = u_proj * u_view * world;
}
