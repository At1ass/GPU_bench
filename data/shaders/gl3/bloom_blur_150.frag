#version 150
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_tex;
uniform float u_horizontal;
uniform vec2 u_texel_size;
void main() {
    float weight[5];
    weight[0] = 0.227027;
    weight[1] = 0.1945946;
    weight[2] = 0.1216216;
    weight[3] = 0.054054;
    weight[4] = 0.016216;

    vec2 dir = u_horizontal > 0.5 ? vec2(u_texel_size.x, 0.0) : vec2(0.0, u_texel_size.y);
    vec3 result = texture(u_tex, v_uv).rgb * weight[0];
    for (int i = 1; i < 5; i++) {
        result += texture(u_tex, v_uv + dir * float(i)).rgb * weight[i];
        result += texture(u_tex, v_uv - dir * float(i)).rgb * weight[i];
    }
    FragColor = vec4(result, 1.0);
}
