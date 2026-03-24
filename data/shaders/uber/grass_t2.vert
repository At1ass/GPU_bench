// Uber shader — version-portable via ShaderCache preamble.
ATTR_IN vec3 a_pos;
ATTR_IN vec3 a_normal;
ATTR_IN vec2 a_uv;

uniform mat4 u_proj;
uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_light_vp;
uniform float u_time;
uniform vec3 u_wind_dir;
uniform int u_grass_count;
uniform float u_area_size;

VS_OUT vec3 v_world_pos;
VS_OUT vec3 v_normal;
VS_OUT vec2 v_uv;
VS_OUT vec4 v_light_pos;
VS_OUT float v_color_t;  // 0=base, 1=tip for color gradient

// Robust integer-style hash (no sin, stable for large inputs)
float hash(float n) {
    n = fract(n * 0.1031);
    n *= n + 33.33;
    n *= n + n;
    return fract(n);
}

void main() {
    float id = float(gl_InstanceID);

    // Procedural position from instance ID (use distinct seeds)
    float px = hash(id + 0.5) * u_area_size - u_area_size * 0.5;
    float pz = hash(id * 1.37 + 7.13) * u_area_size - u_area_size * 0.5;

    // Hide grass too close to center (bunny area)
    float dist_from_center = sqrt(px * px + pz * pz);
    float vis = smoothstep(0.6, 1.2, dist_from_center);

    // Random rotation per blade
    float angle = hash(id * 2.51 + 3.17) * 6.2832;
    float cs = cos(angle);
    float sn = sin(angle);

    // Random scale variation
    float blade_height = (0.20 + hash(id * 3.17 + 5.31) * 0.25) * vis;
    float blade_width = 0.07 + hash(id * 3.97 + 9.41) * 0.05;

    // Transform blade template
    vec3 pos = a_pos;

    // Scale
    pos.x *= blade_width;
    pos.z *= blade_width;
    pos.y *= blade_height;

    // Rotate around Y axis
    float rx = pos.x * cs - pos.z * sn;
    float rz = pos.x * sn + pos.z * cs;
    pos.x = rx;
    pos.z = rz;

    // Wind animation (same formula as fur, using vertex height)
    float above_ground = pos.y;
    if (above_ground > 0.001 && length(u_wind_dir) > 0.01) {
        float wind_phase = (px + pz * 0.7) * 4.0;
        float wave = sin(u_time * 2.0 + wind_phase)
                   + sin(u_time * 5.3 + wind_phase * 2.3) * 0.3;
        pos.xz += u_wind_dir.xz * wave * above_ground * above_ground * 2.0;
    }

    // Translate to world position (ground at y=-1)
    pos.x += px;
    pos.y += -1.0;
    pos.z += pz;

    vec4 world = u_model * vec4(pos, 1.0);
    v_world_pos = world.xyz;
    v_normal = vec3(0.0, 1.0, 0.0); // grass normals point up
    v_uv = a_uv;
    v_color_t = a_pos.y; // 0 at base, 1 at tip (before scaling)
    v_light_pos = u_light_vp * world;

    gl_Position = u_proj * u_view * world;
}
