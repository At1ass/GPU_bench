// Uber shader — version-portable via ShaderCache preamble.
ATTR_IN vec3 a_pos;
ATTR_IN vec3 a_normal;
ATTR_IN vec2 a_uv;

uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_light_vp;
uniform float u_shell_index;
uniform float u_fur_length;
uniform float u_time;
uniform vec3 u_wind_dir;
uniform int u_use_instancing;
uniform int u_fur_shells;

VS_OUT vec3 v_world_pos;
VS_OUT vec3 v_world_normal;
VS_OUT vec3 v_obj_pos;
VS_OUT vec2 v_uv;
VS_OUT float v_shell_index;
VS_OUT vec4 v_light_pos;

void main() {
    float h;
    if (u_use_instancing > 0) {
        h = float(gl_InstanceID + 1) / float(u_fur_shells - 1);
    } else {
        h = u_shell_index;
    }

    // --- Fur direction: start with surface normal ---
    vec3 fur_dir = a_normal;

    // Gravity: bends fur downward, tips bend more (quadratic)
    vec3 gravity_bend = vec3(0.0, -1.0, 0.0) * 0.35 * h * h;

    // Wind: multi-frequency for natural look, spatially varying phase
    float wind_phase = dot(a_pos, vec3(1.0, 0.5, 0.7)) * 4.0;
    float wind_main = sin(u_time * 2.0 + wind_phase);
    float wind_gust = sin(u_time * 5.3 + wind_phase * 2.3) * 0.3;
    float wind_turbulence = sin(u_time * 8.7 + dot(a_pos, vec3(0.3, 0.0, 1.0)) * 11.0) * 0.1;
    float wind_strength = (wind_main + wind_gust + wind_turbulence) * h * h;
    vec3 wind_bend = u_wind_dir * wind_strength * 0.4;

    // Apply forces as BENDING (change direction, not length)
    fur_dir += gravity_bend + wind_bend;

    // Re-normalize: fur length stays EXACTLY u_fur_length * h
    fur_dir = normalize(fur_dir);

    // Shell offset with constant length
    vec3 offset = fur_dir * u_fur_length * h;

    vec3 displaced = a_pos + offset;

    vec4 world = u_model * vec4(displaced, 1.0);
    v_world_pos = world.xyz;
    v_world_normal = normalize(mat3(u_model) * a_normal);
    v_obj_pos = a_pos;
    v_uv = a_uv;
    v_shell_index = h;
    v_light_pos = u_light_vp * u_model * vec4(a_pos, 1.0);

    gl_Position = u_proj * u_view * world;
}
