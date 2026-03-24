// Uber shader — version-portable via ShaderCache preamble.
ATTR_IN vec3 a_pos;
ATTR_IN vec3 a_normal;
ATTR_IN vec2 a_uv;
uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_light_vp;
uniform vec3 u_wind_dir;
uniform float u_time;
uniform float u_vertex_wind;
VS_OUT vec3 v_world_pos;
VS_OUT vec3 v_normal;
VS_OUT vec2 v_uv;
VS_OUT vec4 v_light_pos;
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
    v_light_pos = u_light_vp * world;
    gl_Position = u_proj * u_view * world;
}
