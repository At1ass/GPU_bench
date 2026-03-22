#version 150
in vec3 a_pos;
in vec3 a_normal;
in vec2 a_uv;
uniform mat4 u_light_vp;
uniform mat4 u_model;
void main() {
    gl_Position = u_light_vp * u_model * vec4(a_pos, 1.0);
}
