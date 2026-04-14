// Uber fullscreen vertex shader — version-portable via ShaderCache preamble.
// GL 2.1 (GLSL_120): reads position from VBO quad attributes.
// GL 3.0+ : generates fullscreen triangle from gl_VertexID (no VBO needed).
#ifdef GLSL_120
ATTR_IN vec3 a_pos;
ATTR_IN vec3 a_normal;
ATTR_IN vec2 a_uv;
#endif
VS_OUT vec2 v_uv;
void main() {
#ifdef GLSL_120
    v_uv = a_pos.xy * 0.5 + 0.5;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
#else
    vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0;
    v_uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
#endif
}
