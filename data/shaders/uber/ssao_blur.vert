// Uber shader — version-portable via ShaderCache preamble.
ATTR_IN vec3 a_pos;
ATTR_IN vec2 a_uv;
VS_OUT vec2 v_uv;
void main() {
    v_uv = a_pos.xy * 0.5 + 0.5;
    gl_Position = vec4(a_pos.xy, 0.0, 1.0);
}
