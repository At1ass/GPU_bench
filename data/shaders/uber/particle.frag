// Uber shader — version-portable via ShaderCache preamble.
FS_IN vec2 v_uv;
FS_IN float v_alpha;

void main() {
    // Soft circle
    float dist = length(v_uv - 0.5) * 2.0;
    float alpha = smoothstep(1.0, 0.3, dist) * v_alpha;
    if (alpha < 0.01) discard;

    // Warm dust color
    vec3 color = vec3(0.9, 0.85, 0.7);
    FRAG_COLOR = vec4(color, alpha);
}
