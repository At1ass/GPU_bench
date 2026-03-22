#version 430
in vec2 v_quad_uv;
in vec4 v_color;
in float v_life_ratio;

out vec4 FragColor;

void main() {
    // Soft circular glow centered on quad
    vec2 center = v_quad_uv - 0.5;
    float dist = length(center) * 2.0; // 0 at center, 1 at edge

    // Tighter falloff for smaller, subtler glow
    float glow = exp(-dist * dist * 5.0);

    // Core hotspot (dim center)
    float core = exp(-dist * dist * 16.0) * 0.3;

    float intensity = (glow + core) * 0.8; // moderate intensity, HDR color does the bloom

    // Modulate by particle color and alpha
    vec3 color = v_color.rgb * intensity;
    float alpha = v_color.a * intensity;

    // Discard nearly invisible fragments
    if (alpha < 0.01) discard;

    // Output for additive blending (premultiplied alpha)
    FragColor = vec4(color * alpha, alpha);
}
