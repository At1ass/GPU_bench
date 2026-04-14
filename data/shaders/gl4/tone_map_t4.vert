#version 430
// Fullscreen triangle via gl_VertexID (no VBO needed)
out vec2 v_uv;
void main() {
    vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0;
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
