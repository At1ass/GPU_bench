#version 430
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;

out vec3 v_pos;
out vec3 v_normal;
out vec2 v_uv;

void main() {
    // Pass-through to tessellation control shader
    v_pos = a_pos;
    v_normal = a_normal;
    v_uv = a_uv;
}
