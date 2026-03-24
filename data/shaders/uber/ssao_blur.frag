// Uber shader — version-portable via ShaderCache preamble.
FS_IN vec2 v_uv;

uniform sampler2D u_ssao_tex;
uniform vec2 u_texel_size;  // 1.0/width, 1.0/height

void main() {
    float result = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * u_texel_size;
            result += COMPAT_TEX2D(u_ssao_tex, v_uv + offset).r;
        }
    }
    result /= 25.0;
    FRAG_COLOR = vec4(result, result, result, 1.0);
}
