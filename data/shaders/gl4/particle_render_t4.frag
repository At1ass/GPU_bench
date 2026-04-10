#version 430
in vec2 v_quad_uv;
in vec4 v_color;

out vec4 FragColor;

void main() {
    vec2 center = v_quad_uv - 0.5;
    float dist = length(center) * 2.0;

    // Multi-layer glow
    float outer_glow = exp(-dist * dist * 3.0);
    float core = exp(-dist * dist * 12.0);
    float intensity = outer_glow * 0.6 + core * 0.8;

    // HDR color already set in compute shader
    vec3 color = v_color.rgb * intensity;
    float alpha = v_color.a * intensity;

    if (alpha < 0.005) discard;

    // Additive output (GL_ONE, GL_ONE): RGB added to scene
    FragColor = vec4(color * alpha, 1.0);
}
