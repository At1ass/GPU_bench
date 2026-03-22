#version 150
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_ssao_tex;
uniform vec2 u_texel_size;

void main() {
    float result = 0.0;
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x), float(y)) * u_texel_size;
            result += texture(u_ssao_tex, v_uv + offset).r;
        }
    }
    result /= 25.0;
    FragColor = vec4(result, result, result, 1.0);
}
