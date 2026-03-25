// Uber shader — procedural torch flame (T3+)
// Scrolling FBM noise + teardrop mask + fire color gradient.
// Additive blending (GL_ONE, GL_ONE) — output is added directly to scene.

FS_IN vec2 v_uv;         // [0,1] quad UV
FS_IN float v_torch_id;  // torch index for variation

uniform float u_time;

#pragma include "noise_lib.glsl"

// Fire color gradient (blackbody-inspired)
vec3 fireColor(float intensity) {
    vec3 c1 = vec3(0.2, 0.03, 0.0);    // dark red (tips)
    vec3 c2 = vec3(0.9, 0.3, 0.0);     // red-orange
    vec3 c3 = vec3(1.0, 0.7, 0.1);     // orange
    vec3 c4 = vec3(1.0, 0.95, 0.4);    // yellow
    vec3 c5 = vec3(1.0, 1.0, 0.85);    // white-hot core

    vec3 color = mix(c1, c2, smoothstep(0.0, 0.25, intensity));
    color = mix(color, c3, smoothstep(0.2, 0.5, intensity));
    color = mix(color, c4, smoothstep(0.45, 0.75, intensity));
    color = mix(color, c5, smoothstep(0.75, 1.0, intensity));
    return color;
}

void main() {
    vec2 uv = v_uv;
    float t_offset = v_torch_id * 2.37;

    // Teardrop shape mask: wide at bottom, narrow at top
    float height_t = uv.y; // 0 = bottom (hottest), 1 = top (tips)
    float taper = 1.0 - pow(height_t, 0.6);
    float center_dist = abs(uv.x - 0.5) / max(taper * 0.55, 0.001);
    float shape = smoothstep(1.0, 0.2, center_dist) * smoothstep(-0.05, 0.08, height_t);

    if (shape < 0.01) discard;

    // Scroll noise upward
    vec2 noise_uv = vec2((uv.x - 0.5) * 2.5, uv.y * 1.8);
    noise_uv.y -= (u_time + t_offset) * 1.8;

    // UV distortion for lateral flickering
    float distort = noise(vec2(uv.y * 4.0, (u_time + t_offset) * 2.5)) * 0.12 * height_t;
    noise_uv.x += distort;

    // 3-octave FBM
    float n = 0.0;
    float amp = 0.6;
    float freq = 2.5;
    for (int i = 0; i < 3; i++) {
        n += amp * noise(noise_uv * freq);
        freq *= 2.0;
        amp *= 0.45;
    }

    // Fire intensity: noise * shape, brighter at base
    float fire_intensity = n * shape;
    fire_intensity *= (1.0 - 0.5 * height_t);
    // Boost overall brightness
    fire_intensity = clamp(fire_intensity * 1.4, 0.0, 1.0);

    // Color from fire gradient
    vec3 color = fireColor(fire_intensity);

    // HDR boost for bloom pickup
    float hdr = 2.5 + fire_intensity * 4.0;
    color *= hdr;

    // Alpha controls contribution strength
    float alpha = shape * fire_intensity;
    alpha = clamp(alpha * 1.2, 0.0, 1.0);

    if (alpha < 0.005) discard;

    // Additive output: RGB added to scene, alpha unused by GL_ONE,GL_ONE
    FRAG_COLOR = vec4(color * alpha, 1.0);
}
