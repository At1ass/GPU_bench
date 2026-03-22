vec3 triplanarNoise(vec3 pos, vec3 n, float scale) {
    vec3 an = abs(n);
    float sum = an.x + an.y + an.z;
    an /= sum;
    float nx = noise(pos.yz * scale);
    float ny = noise(pos.xz * scale);
    float nz = noise(pos.xy * scale);
    return vec3(an.x * nx + an.y * ny + an.z * nz);
}

vec3 terrainColor(vec3 pos, vec3 normal, vec2 uv) {
    float slope = 1.0 - abs(normal.y);
    float height = pos.y;
    float n1 = fbm(pos.xz * 0.3, 4);
    float n2 = noise(pos.xz * 0.1 + 42.0);
    float warp = noise(pos.xz * 0.05 + vec2(n2 * 3.0));

    // Bright lush grass (reference: vivid green tops)
    vec3 grass = vec3(0.35, 0.55, 0.18) * (0.85 + 0.3 * n1);
    // Rich dirt/earth
    vec3 dirt = vec3(0.40, 0.28, 0.14) * (0.8 + 0.3 * noise(pos.xz * 2.0));
    // Dark cliff rock with Worley cracks (reference: dark undersides)
    float rockN = worley(pos.xz * 1.5) * 0.5 + fbm(pos.xz * 1.0, 3) * 0.5;
    vec3 rock = vec3(0.22, 0.18, 0.15) * (0.6 + 0.6 * rockN);
    // Dark underside rock (very dark for stalactites)
    vec3 darkRock = vec3(0.10, 0.08, 0.06) * (0.7 + 0.4 * rockN);
    // Snow cap
    vec3 snow = vec3(0.90, 0.92, 0.95);
    // Moss in valleys
    vec3 moss = vec3(0.20, 0.38, 0.12);

    // Slope-based blending: flat=grass, steep=rock
    float grassMix = smoothstep(0.25, 0.5, slope);
    vec3 col = mix(grass, dirt, grassMix * 0.4 + warp * 0.25);
    col = mix(col, rock, smoothstep(0.4, 0.65, slope));

    // Height-based features
    col = mix(col, snow, smoothstep(8.0, 12.0, height) * (1.0 - slope));
    col = mix(col, moss, smoothstep(1.0, -0.5, height) * (1.0 - slope) * 0.5);

    // Underside: very dark rocky (below island center, normal pointing down)
    float underside = smoothstep(0.0, -3.0, height) * (1.0 - normal.y) * 0.5;
    underside += smoothstep(-0.2, -0.5, normal.y); // faces down = underside
    col = mix(col, darkRock, clamp(underside, 0.0, 1.0));

    // Micro-detail
    float micro = noise(pos.xz * 12.0) * 0.08 - 0.04;
    col += micro;

    return col;
}

vec3 surfaceColor(vec3 pos, vec3 normal, vec2 uv, vec3 mat_color, float proc_tex) {
    if (proc_tex > 0.5) {
        return terrainColor(pos, normal, uv) * mat_color * 2.0;
    }
    // Flat material: add subtle noise for surface detail
    float n = noise(pos.xz * 4.0) * 0.06 - 0.03;
    return mat_color + n;
}

vec3 perturbNormal(vec3 N, vec3 pos, vec2 uv, float strength) {
    vec3 dp1 = dFdx(pos);
    vec3 dp2 = dFdy(pos);
    vec3 T = normalize(dp1);
    vec3 B = normalize(cross(N, T));
    vec2 st = pos.xz * 3.0;
    float eps = 0.05;
    float h0 = fbm(st, 3);
    float hx = fbm(st + vec2(eps, 0.0), 3);
    float hy = fbm(st + vec2(0.0, eps), 3);
    float dx = (hx - h0) / eps * strength;
    float dy = (hy - h0) / eps * strength;
    return normalize(N + T * dx + B * dy);
}
