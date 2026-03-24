// PCSS (Percentage Closer Soft Shadows)
// Uses textureGather for efficient blocker search on GL4.3+
// Produces variable-width penumbra: sharp near contact, soft far away

// Poisson disk samples (16 points, well-distributed)
const vec2 poissonDisk[16] = vec2[16](
    vec2(-0.9425, -0.0341), vec2(-0.3980,  0.7341),
    vec2( 0.1749, -0.6821), vec2( 0.8540,  0.1569),
    vec2(-0.6168, -0.5694), vec2( 0.4276,  0.7850),
    vec2(-0.1563, -0.2447), vec2( 0.6297, -0.6310),
    vec2(-0.7527,  0.4234), vec2( 0.0191, -0.9710),
    vec2( 0.9586, -0.2065), vec2(-0.2740,  0.2104),
    vec2( 0.3390, -0.0254), vec2(-0.5118,  0.1387),
    vec2( 0.7113,  0.4884), vec2(-0.0687,  0.9568)
);

// Rotate a sample point by an angle derived from screen position (reduces banding)
vec2 rotateSample(vec2 s, float angle) {
    float c = cos(angle);
    float sn = sin(angle);
    return vec2(s.x * c - s.y * sn, s.x * sn + s.y * c);
}

// Find average blocker depth using textureGather (4 depth samples per call)
float findBlockerDepth(sampler2D shadowMap, vec2 uv, float receiverDepth,
                       float searchRadius, vec2 texelSize, float rotAngle) {
    float blockerSum = 0.0;
    int blockerCount = 0;

    for (int i = 0; i < 16; i++) {
        vec2 offset = rotateSample(poissonDisk[i], rotAngle) * searchRadius;
        vec2 sampleUV = uv + offset * texelSize;

        // textureGather fetches 4 texels at once (2x2 block)
        vec4 depths = textureGather(shadowMap, sampleUV, 0);

        for (int j = 0; j < 4; j++) {
            if (depths[j] < receiverDepth - 0.002) {
                blockerSum += depths[j];
                blockerCount++;
            }
        }
    }

    if (blockerCount == 0) return -1.0; // no blockers found
    return blockerSum / float(blockerCount);
}

// PCSS shadow with variable penumbra
float computePCSSShadow(sampler2D shadowMap, vec4 lightSpacePos,
                        vec2 shadowTexelSize, float lightSize) {
    vec3 proj = lightSpacePos.xyz / lightSpacePos.w;
    proj = proj * 0.5 + 0.5;

    // Outside shadow map bounds
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;

    float receiverDepth = proj.z;
    float bias = 0.002;

    // Per-pixel rotation angle to break up banding
    float rotAngle = fract(dot(gl_FragCoord.xy, vec2(0.75487766, 0.56984029))) * 6.2832;

    // Step 1: Blocker search with textureGather
    float searchRadius = lightSize * 20.0; // search area proportional to light size
    float avgBlockerDepth = findBlockerDepth(shadowMap, proj.xy, receiverDepth,
                                              searchRadius, shadowTexelSize, rotAngle);

    // No blockers = fully lit
    if (avgBlockerDepth < 0.0) return 1.0;

    // Step 2: Penumbra estimation
    float penumbraWidth = (receiverDepth - avgBlockerDepth) * lightSize / max(avgBlockerDepth, 0.001);
    penumbraWidth = clamp(penumbraWidth, 1.0, 30.0); // clamp to reasonable range

    // Step 3: PCF filtering with penumbra-dependent radius
    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 offset = rotateSample(poissonDisk[i], rotAngle) * penumbraWidth;
        vec2 sampleUV = proj.xy + offset * shadowTexelSize;
        float d = texture(shadowMap, sampleUV).r;
        shadow += (receiverDepth - bias > d) ? 0.0 : 1.0;
    }
    shadow /= 16.0;

    return mix(0.25, 1.0, shadow); // ambient shadow floor (prevents pure black)
}
