#version 150
in vec3 a_pos;    // quad corner offset (-1..1 in XY)
in vec3 a_normal; // particle seed position (encoded in normal)
in vec2 a_uv;     // UV: x = particle ID (0..1), y = quad v

uniform mat4 u_proj;
uniform mat4 u_view;
uniform float u_time;

out vec2 v_uv;
out float v_alpha;

void main() {
    // Particle center: animated from seed position
    vec3 seed = a_normal;
    float id = a_uv.x;

    // Animated position: gentle floating motion
    vec3 center = seed;
    center.y += sin(u_time * 0.8 + id * 6.28) * 0.3;
    center.x += sin(u_time * 0.5 + id * 12.57) * 0.15;
    center.z += cos(u_time * 0.6 + id * 9.42) * 0.15;

    // Billboard: expand quad corners in screen space
    float size = 0.012 + 0.008 * sin(id * 43.7);
    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
    vec3 world_pos = center + right * a_pos.x * size + up * a_pos.y * size;

    // Fade based on particle ID and time
    v_alpha = 0.3 + 0.2 * sin(id * 17.3 + u_time * 0.3);
    v_uv = a_pos.xy * 0.5 + 0.5;

    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}
