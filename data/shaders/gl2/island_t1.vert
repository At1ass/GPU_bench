#version 120
attribute vec3 a_pos;
attribute vec3 a_normal;
attribute vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform vec3 u_wind_dir;
uniform float u_time;
uniform float u_vertex_wind;  // 1.0 = enable wind displacement (grass), 0.0 = off
varying vec3 v_world_pos;
varying vec3 v_normal;
varying vec2 v_uv;
void main() {
    vec3 pos = a_pos;

    // Grass wind: only when u_vertex_wind > 0.5
    if (u_vertex_wind > 0.5) {
        float ground_y = -1.0;
        float above_ground = max(pos.y - ground_y, 0.0);
        if (above_ground > 0.01) {
            float phase = dot(pos.xz, vec2(1.0, 0.7)) * 4.0;
            float wave = sin(u_time * 2.0 + phase) + sin(u_time * 5.3 + phase * 2.3) * 0.3;
            pos.xz += u_wind_dir.xz * wave * above_ground * above_ground * 0.3;
        }
    }

    vec4 world = u_model * vec4(pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = normalize(mat3(u_model) * a_normal);
    v_uv = a_uv;
    gl_Position = u_proj * u_view * world;
}
