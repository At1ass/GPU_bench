#version 120
attribute vec3 a_pos;
uniform mat4 u_proj;
uniform mat4 u_view;
varying vec3 v_dir;
void main() {
    // Remove translation from view matrix
    mat4 rotView = u_view;
    rotView[3] = vec4(0.0, 0.0, 0.0, 1.0);
    v_dir = a_pos;
    vec4 pos = u_proj * rotView * vec4(a_pos * 500.0, 1.0);
    gl_Position = pos.xyww;
}
