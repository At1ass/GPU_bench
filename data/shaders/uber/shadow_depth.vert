// Uber shader — version-portable via ShaderCache preamble.
ATTR_IN vec3 a_pos;
ATTR_IN vec3 a_normal;
ATTR_IN vec2 a_uv;
uniform mat4 u_light_vp;
uniform mat4 u_model;
void main() {
    gl_Position = u_light_vp * u_model * vec4(a_pos, 1.0);
}
