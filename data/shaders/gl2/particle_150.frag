#version 150
in vec2 v_uv;
in float v_alpha;

out vec4 FragColor;

void main() {
    // Soft circle
    float dist = length(v_uv - 0.5) * 2.0;
    float alpha = smoothstep(1.0, 0.3, dist) * v_alpha;
    if (alpha < 0.01) discard;

    // Warm dust color
    vec3 color = vec3(0.9, 0.85, 0.7);
    FragColor = vec4(color, alpha);
}
