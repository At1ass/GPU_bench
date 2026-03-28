// Material system uniforms — set per-object by render passes
uniform int u_material_id;
uniform float u_noise_scale;
uniform float u_noise_intensity;
uniform float u_warp_strength;
uniform float u_detail_scale;
uniform vec3 u_mat_color_b;

// ============================================================
// Triplanar noise for seamless 3D surface texturing
// ============================================================
vec3 triplanarNoise(vec3 pos, vec3 n, float scale) {
    vec3 an = abs(n);
    float sum = an.x + an.y + an.z;
    an /= sum;
    float nx = noise(pos.yz * scale);
    float ny = noise(pos.xz * scale);
    float nz = noise(pos.xy * scale);
    return vec3(an.x * nx + an.y * ny + an.z * nz);
}

float triplanarFbm(vec3 pos, vec3 n, float scale, int octaves) {
    vec3 an = abs(n);
    float sum = an.x + an.y + an.z;
    an /= sum;
    float nx = fbm(pos.yz * scale, octaves);
    float ny = fbm(pos.xz * scale, octaves);
    float nz = fbm(pos.xy * scale, octaves);
    return an.x * nx + an.y * ny + an.z * nz;
}

float triplanarWorley(vec3 pos, vec3 n, float scale) {
    vec3 an = abs(n);
    float sum = an.x + an.y + an.z;
    an /= sum;
    float nx = worley(pos.yz * scale);
    float ny = worley(pos.xz * scale);
    float nz = worley(pos.xy * scale);
    return an.x * nx + an.y * ny + an.z * nz;
}

// ============================================================
// Terrain: full slope/height procedural landscape (ProceduralType::Terrain = 1)
// ============================================================
vec3 terrainColor(vec3 pos, vec3 normal, vec2 uv) {
    float slope = 1.0 - abs(normal.y);
    float height = pos.y;
    float n1 = fbm(pos.xz * 0.3, 4);
    float n2 = noise(pos.xz * 0.1 + 42.0);
    float warp = noise(pos.xz * 0.05 + vec2(n2 * 3.0));

    // Natural grass
    vec3 grass = vec3(0.30, 0.45, 0.15) * (0.85 + 0.25 * n1);
    // Rich dirt/earth
    vec3 dirt = vec3(0.40, 0.28, 0.14) * (0.8 + 0.3 * noise(pos.xz * 2.0));
    // Dark cliff rock with Worley cracks
    float rockN = worley(pos.xz * 1.5) * 0.5 + fbm(pos.xz * 1.0, 3) * 0.5;
    vec3 rock = vec3(0.22, 0.18, 0.15) * (0.6 + 0.6 * rockN);
    // Dark underside rock
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

    // Moss growth: top-facing surfaces with domain-warped organic border
    float upFacing = max(normal.y, 0.0);
    float mossWarp = noise(pos.xz * 2.5 + vec2(noise(pos.xz * 0.8) * 2.0));
    float mossAmount = smoothstep(0.5, 0.9, upFacing)
                     * smoothstep(0.35, 0.6, mossWarp)
                     * smoothstep(-0.5, 2.0, height)
                     * (1.0 - smoothstep(6.0, 10.0, height));
    vec3 mossCol = vec3(0.15, 0.35, 0.08) * (0.85 + 0.2 * noise(pos.xz * 6.0));
    col = mix(col, mossCol, mossAmount * 0.65);

    // Underside: very dark rocky
    float underside = smoothstep(0.0, -3.0, height) * (1.0 - normal.y) * 0.5;
    underside += smoothstep(-0.2, -0.5, normal.y);
    col = mix(col, darkRock, clamp(underside, 0.0, 1.0));

    // Micro-detail
    float micro = noise(pos.xz * 12.0) * 0.08 - 0.04;
    col += micro;

    return col;
}

// ============================================================
// Stone: weathered sandstone/marble (ProceduralType::Stone = 2)
// Columns, arches, pedestals, obelisks, slabs
// ============================================================
vec3 stoneColor(vec3 pos, vec3 normal, vec3 colorA, vec3 colorB, float sc, float inten, float warp, float detail) {
    // Base: triplanar Worley cracks for stone structure
    float cracks = triplanarWorley(pos, normal, 3.0 * sc);
    float cracks2 = triplanarWorley(pos, normal, 6.0 * sc * detail);

    // Domain-warped grain for organic veining
    vec2 warped = pos.xz * 2.0 * sc + vec2(noise(pos.xz * 0.8) * warp * 4.0);
    float grain = fbm(warped, 3) * inten;

    // Vertical weathering streaks (rain erosion)
    float streak = fbm(vec2(pos.x * 1.5 * sc, pos.y * 4.0 * sc), 3);
    streak = smoothstep(0.4, 0.7, streak) * 0.12 * inten;

    // Blend primary and secondary colors with crack pattern
    vec3 col = mix(colorA, colorB, cracks * 0.5 + grain * 0.3);

    // Darken crevices (where Worley distance is small)
    float crevice = smoothstep(0.25, 0.05, cracks) * 0.2 * inten;
    col -= crevice;

    // Lighten exposed edges (where Worley is high = smooth surface)
    float edge_light = smoothstep(0.5, 0.85, cracks) * 0.06 * inten;
    col += edge_light;

    // Rain streak darkening
    col -= streak * smoothstep(0.3, 0.7, 1.0 - abs(normal.y));

    // Fine detail cracks (secondary scale)
    float fine = smoothstep(0.2, 0.05, cracks2) * 0.08 * inten;
    col -= fine;

    // Moss in upward-facing crevices
    float upFacing = max(normal.y, 0.0);
    float mossZone = smoothstep(0.6, 0.95, upFacing);
    float mossNoise = noise(pos.xz * 5.0 * sc + vec2(noise(pos.xz * 1.2) * 2.0));
    float mossMask = mossZone * smoothstep(0.3, 0.6, mossNoise) * (1.0 - smoothstep(0.5, 0.8, cracks));
    vec3 mossCol = vec3(0.12, 0.28, 0.06) * (0.85 + 0.3 * noise(pos.xz * 8.0));
    col = mix(col, mossCol, mossMask * 0.5);

    // Micro-detail granularity
    float micro = triplanarFbm(pos, normal, 15.0 * sc, 2) * 0.04 * inten - 0.02 * inten;
    col += micro;

    return col;
}

// ============================================================
// Rock: rough boulders (ProceduralType::Rock = 3)
// Scattered rocks, boulders
// ============================================================
vec3 rockColor(vec3 pos, vec3 normal, vec3 colorA, vec3 colorB, float sc, float inten, float warp, float detail) {
    // Stratified horizontal layers (sedimentary look)
    float layerY = pos.y * 8.0 * sc + noise(pos.xz * 0.5) * warp * 3.0;
    float strata = ridgeNoise(vec2(layerY, 0.0)) * 0.4 + 0.6;
    strata *= (0.8 + 0.4 * noise(vec2(layerY * 0.3, pos.x * 0.5)));

    // Triplanar rock grain
    float grain = triplanarFbm(pos, normal, 4.0 * sc * detail, 3);

    // Worley for mineral veins and fractures
    float veins = triplanarWorley(pos, normal, 2.5 * sc);
    float fractures = smoothstep(0.15, 0.02, veins) * 0.15 * inten;

    // Base color with strata blending
    vec3 col = mix(colorA, colorB, strata * 0.4 + grain * 0.3);

    // Fracture darkening
    col -= fractures;

    // Lichen patches (yellowish-green on exposed faces)
    float lichenZone = smoothstep(0.3, 0.7, normal.y + noise(pos.xz * 3.0) * 0.4);
    float lichenPattern = noise(pos.xz * 6.0 * sc + vec2(noise(pos.xz * 1.5) * warp * 2.0));
    float lichen = lichenZone * smoothstep(0.55, 0.75, lichenPattern) * inten;
    vec3 lichenCol = vec3(0.45, 0.48, 0.18) * (0.7 + 0.3 * noise(pos.xz * 12.0));
    col = mix(col, lichenCol, lichen * 0.4);

    // Granular surface texture
    float granular = triplanarFbm(pos, normal, 20.0 * sc, 2) * 0.06 * inten - 0.03 * inten;
    col += granular;

    return col;
}

// ============================================================
// Foliage: tree canopy (ProceduralType::Foliage = 4)
// Trees, bushes
// ============================================================
vec3 foliageColor(vec3 pos, vec3 normal, vec3 colorA, vec3 colorB, float sc, float inten, float warp, float detail) {
    // Bark zone: trunk near ground level (world y < 0.0 = below canopy)
    // Trees sit at terrain height (~-1.0), trunk extends ~1.5 units up
    float barkZone = smoothstep(0.5, -0.3, pos.y);
    // Horizontal normals = trunk cylinder; vertical normals = canopy sphere
    barkZone *= smoothstep(0.5, 0.2, abs(normal.y));

    if (barkZone > 0.01) {
        // Bark texture: vertical fibrous pattern + triplanar noise
        vec3 barkDark = vec3(0.18, 0.12, 0.07);
        vec3 barkLight = vec3(0.32, 0.22, 0.14);
        float barkFibres = fbm(vec2(pos.y * 8.0 * sc, atan(pos.x, pos.z) * 2.0), 3);
        float barkRidge = ridgeNoise(vec2(pos.y * 4.0 * sc, atan(pos.x, pos.z) * 3.0));
        vec3 bark = mix(barkDark, barkLight, barkFibres * 0.6 + barkRidge * 0.3);
        // Moss patches on bark (north-facing, near base)
        float mossPatch = smoothstep(0.3, 0.7, noise(pos.xz * 4.0 + pos.y * 2.0)) * 0.3;
        bark = mix(bark, vec3(0.15, 0.28, 0.08), mossPatch * barkZone);
        // Micro-roughness
        bark += (noise(vec2(pos.y * 20.0, atan(pos.x, pos.z) * 8.0)) * 0.04 - 0.02) * inten;

        // Blend bark with canopy at transition zone
        if (barkZone > 0.95) return bark;
    }

    // Height-dependent color shift: darker at base, brighter at top
    float heightFactor = smoothstep(-1.0, 3.0, pos.y);

    // Canopy density variation with domain warping
    vec2 warped = pos.xz * 2.0 * sc + vec2(noise(pos.xz * 0.6) * warp * 3.0);
    float canopy = fbm(warped, 3);

    // Leafy micro-pattern (high frequency for leaf impression)
    float leafy = noise(pos.xz * 12.0 * sc * detail + pos.y * 3.0);
    float leafEdge = smoothstep(0.35, 0.55, leafy);

    // Blend dark base to bright canopy top
    vec3 col = mix(colorA, colorB, heightFactor * 0.6 + canopy * 0.3);

    // Leaf-scale color variation
    vec3 leafDark = col * 0.75;
    vec3 leafBright = col * 1.15;
    col = mix(leafDark, leafBright, leafEdge);

    // Translucency hint: lighter where normal faces away (backlit leaves)
    float translucency = (1.0 - abs(normal.y)) * 0.15 * inten;
    col += vec3(0.08, 0.12, 0.03) * translucency;

    // Subsurface warmth on top-facing surfaces (sun-facing canopy)
    float sunSide = max(normal.y, 0.0);
    col += vec3(0.04, 0.06, 0.01) * sunSide * canopy * inten;

    // Micro-variation
    float micro = noise(pos.xz * 25.0 * sc + pos.y * 5.0) * 0.05 * inten - 0.025 * inten;
    col += micro;

    // Blend bark→canopy at transition
    if (barkZone > 0.01) {
        vec3 barkDark = vec3(0.18, 0.12, 0.07);
        vec3 barkLight = vec3(0.32, 0.22, 0.14);
        float bf = fbm(vec2(pos.y * 8.0 * sc, atan(pos.x, pos.z) * 2.0), 3);
        vec3 bark = mix(barkDark, barkLight, bf * 0.6);
        col = mix(col, bark, barkZone);
    }

    return col;
}

// ============================================================
// Moss: organic moss/lichen surface (ProceduralType::Moss = 5)
// Mossy blocks, overgrown ruins
// ============================================================
vec3 mossColor(vec3 pos, vec3 normal, vec3 colorA, vec3 colorB, float sc, float inten, float warp, float detail) {
    // IQ double domain warp for organic, flowing moss pattern
    vec2 p = pos.xz * 2.0 * sc;
    vec2 q = vec2(fbm(p, 3), fbm(p + vec2(5.2, 1.3), 3));
    vec2 r = vec2(fbm(p + warp * 4.0 * q + vec2(1.7, 9.2), 3),
                  fbm(p + warp * 4.0 * q + vec2(8.3, 2.8), 3));
    float pattern = fbm(p + warp * 4.0 * r, 4);

    // Stone substrate visible in gaps
    float stoneShow = smoothstep(0.4, 0.25, pattern);
    vec3 stoneCol = colorA * (0.7 + 0.3 * triplanarWorley(pos, normal, 3.0 * sc));

    // Moss: deep green with variation
    float mossVariation = noise(pos.xz * 6.0 * sc * detail);
    vec3 mossCol = mix(colorB, colorB * 1.3 + vec3(0.02, 0.05, 0.0), mossVariation);

    // Blend stone substrate with moss coverage
    vec3 col = mix(mossCol, stoneCol, stoneShow);

    // Height-based moss density (more moss on top)
    float topMoss = smoothstep(0.3, 0.8, normal.y);
    col = mix(stoneCol, col, topMoss * 0.7 + 0.3);

    // Micro-texture: tiny moss bumps
    float micro = noise(pos.xz * 18.0 * sc) * 0.05 * inten - 0.025 * inten;
    col += micro;

    return col;
}

// ============================================================
// Surface color dispatcher — called by all fragment shaders
// ============================================================
vec3 surfaceColor(vec3 pos, vec3 normal, vec2 uv, vec3 mat_color, float proc_tex_legacy) {
    int id = u_material_id;
    float sc = u_noise_scale;
    float inten = u_noise_intensity;
    float warp = u_warp_strength;
    float detail = u_detail_scale;
    vec3 colorB = u_mat_color_b;

    if (id == 1) return terrainColor(pos, normal, uv) * mat_color * 1.5;
    if (id == 2) return stoneColor(pos, normal, mat_color, colorB, sc, inten, warp, detail);
    if (id == 3) return rockColor(pos, normal, mat_color, colorB, sc, inten, warp, detail);
    if (id == 4) return foliageColor(pos, normal, mat_color, colorB, sc, inten, warp, detail);
    if (id == 5) return mossColor(pos, normal, mat_color, colorB, sc, inten, warp, detail);

    // Legacy fallback or ProceduralType::Flat: flat color + micro noise
    if (proc_tex_legacy > 0.5) return terrainColor(pos, normal, uv) * mat_color * 1.5;
    float n = noise(pos.xz * 4.0) * 0.06 - 0.03;
    return mat_color + n;
}

// Normal perturbation using analytical FBM derivatives (3x faster than finite differences)
vec3 perturbNormal(vec3 N, vec3 pos, vec2 uv, float strength) {
    vec3 dp1 = dFdx(pos);
    vec3 T = normalize(dp1);
    vec3 B = normalize(cross(N, T));
    vec3 nd = fbmD(pos.xz * 3.0, 3); // value + analytical dv/dx, dv/dy
    return normalize(N - (T * nd.y + B * nd.z) * strength);
}
