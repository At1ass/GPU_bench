// Uber sky fragment shader — version-portable via ShaderCache preamble.
// No #version line — injected by ShaderCache with compat macros.

FS_IN vec3 v_dir;
uniform vec3 u_sun_dir;
uniform float u_time;

// Simple 2D hash for cloud noise
float hash2(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(443.897, 397.297, 491.187));
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

// 2D value noise with smooth interpolation
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash2(i);
    float b = hash2(i + vec2(1.0, 0.0));
    float c = hash2(i + vec2(0.0, 1.0));
    float d = hash2(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// FBM for clouds
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 5; i++) {
        if (i >= octaves) break;
        value += amplitude * vnoise(p * frequency);
        frequency *= 2.17;
        amplitude *= 0.48;
    }
    return value;
}

// ============================================================
// Physical atmosphere (T3+, wwwtyro/glsl-atmosphere reference)
// ============================================================
#ifdef HAS_PHYSICAL_SKY

vec2 raySphere(vec3 r0, vec3 rd, float sr) {
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - sr * sr;
    float d = b * b - 4.0 * a * c;
    if (d < 0.0) return vec2(1e5, -1e5);
    return vec2((-b - sqrt(d)) / (2.0 * a), (-b + sqrt(d)) / (2.0 * a));
}

vec3 atmosphere(vec3 dir, vec3 sun) {
    // Planet and atmosphere radii
    float planetR = 6371e3;
    float atmosR  = 6471e3;
    vec3 origin = vec3(0.0, planetR + 1.0, 0.0); // on surface

    // Rayleigh and Mie coefficients
    vec3 betaR = vec3(5.5e-6, 13.0e-6, 22.4e-6);
    float betaM = 21e-6;
    float hR = 7994.0; // Rayleigh scale height
    float hM = 1200.0; // Mie scale height
    float gMie = 0.758;

    vec2 atmoHit = raySphere(origin, dir, atmosR);
    float rayLen = atmoHit.y;

    int iSteps = 16;
    float stepLen = rayLen / float(iSteps);

    vec3 totalR = vec3(0.0);
    vec3 totalM = vec3(0.0);
    float odR = 0.0, odM = 0.0;

    for (int i = 0; i < 16; i++) {
        vec3 pos = origin + dir * (float(i) + 0.5) * stepLen;
        float h = length(pos) - planetR;

        float rhoR = exp(-h / hR) * stepLen;
        float rhoM = exp(-h / hM) * stepLen;
        odR += rhoR;
        odM += rhoM;

        // Secondary ray toward sun for shadow length
        vec2 sunHit = raySphere(pos, sun, atmosR);
        float sunLen = sunHit.y;
        int jSteps = 8;
        float jStep = sunLen / float(jSteps);
        float jodR = 0.0, jodM = 0.0;

        for (int j = 0; j < 8; j++) {
            vec3 jpos = pos + sun * (float(j) + 0.5) * jStep;
            float jh = length(jpos) - planetR;
            jodR += exp(-jh / hR) * jStep;
            jodM += exp(-jh / hM) * jStep;
        }

        vec3 attenuation = exp(-(betaR * (odR + jodR) + betaM * (odM + jodM)));
        totalR += rhoR * attenuation;
        totalM += rhoM * attenuation;
    }

    // Phase functions
    float mu = dot(dir, sun);
    float mu2 = mu * mu;
    float phR = 3.0 / (16.0 * 3.14159265) * (1.0 + mu2);
    float gMie2 = gMie * gMie;
    float phM = 3.0 / (8.0 * 3.14159265) * ((1.0 - gMie2) * (1.0 + mu2))
              / ((2.0 + gMie2) * pow(1.0 + gMie2 - 2.0 * gMie * mu, 1.5));

    vec3 col = 22.0 * (totalR * betaR * phR + totalM * betaM * phM);
    // iSun=22.0 is already calibrated for physically-correct output range
    // No extra multiplier needed — values are typically [0, 5] for clear sky
    // Minimum ambient sky brightness (prevent fully black sky at low sun angles)
    float sunAlt = max(sun.y, 0.0);
    vec3 ambient = mix(vec3(0.05, 0.07, 0.12), vec3(0.15, 0.25, 0.55), sunAlt);
    col = max(col, ambient);
    return col;
}

#endif

void main() {
    vec3 dir = normalize(v_dir);
    vec3 sun = normalize(u_sun_dir);
    float altitude = dir.y;
    float cosTheta = dot(dir, sun);

    // --- Sky color: physical or simplified ---
#ifdef HAS_PHYSICAL_SKY
    vec3 skyColor = atmosphere(dir, sun);
    // Horizon haze
    float haze = exp(-abs(altitude) * 5.0);
    skyColor = mix(skyColor, vec3(0.7, 0.75, 0.8), haze * 0.3);
#else
    // Simplified Rayleigh + Mie (T1/T2)
    vec3 betaR = vec3(5.8e-6, 13.5e-6, 33.1e-6) * 800.0;
    float phaseR = 0.75 * (1.0 + cosTheta * cosTheta);
    float g = 0.76;
    float g2 = g * g;
    float phaseMie = 1.5 * ((1.0 - g2) / (2.0 + g2)) *
        (1.0 + cosTheta * cosTheta) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
    float opticalDepth = exp(-clamp(altitude, 0.0, 10.0) * 3.0) + 0.1;
    vec3 scatter = betaR * phaseR * opticalDepth;
    vec3 mieColor = vec3(0.8, 0.7, 0.5) * phaseMie * 0.01 * opticalDepth;
    vec3 skyColor = vec3(0.3, 0.5, 0.9) * (1.0 - exp(-scatter * 50.0));
    skyColor += mieColor;
    float haze = exp(-abs(altitude) * 5.0);
    skyColor = mix(skyColor, vec3(0.7, 0.75, 0.8), haze * 0.5);
#endif

    // --- Procedural cloud layer ---
    if (altitude > 0.02) {
        vec2 cloud_uv = dir.xz / max(dir.y, 0.05) * 0.8;
        cloud_uv += vec2(u_time * 0.015, u_time * 0.008);

#ifdef HAS_DOMAIN_WARP
        // IQ double domain warp: organic, swirling cloud shapes
        vec2 q = vec2(fbm(cloud_uv * 1.2, 4), fbm(cloud_uv * 1.2 + vec2(5.2, 1.3), 4));
        vec2 r = vec2(fbm(cloud_uv * 1.2 + 4.0 * q + vec2(1.7, 9.2), 4),
                      fbm(cloud_uv * 1.2 + 4.0 * q + vec2(8.3, 2.8), 4));
        float cloud = fbm(cloud_uv * 1.2 + 4.0 * r, 5);
#else
        float cloud = fbm(cloud_uv * 1.2, 5);
#endif
        cloud = smoothstep(0.38, 0.7, cloud);

        float sunInfluence = max(cosTheta, 0.0) * 0.3;
        vec3 cloudBright = vec3(1.0, 0.98, 0.95);
        vec3 cloudDark = vec3(0.65, 0.68, 0.75);
        vec3 cloudColor = mix(cloudDark, cloudBright, 0.5 + sunInfluence);

        float horizonFade = smoothstep(0.02, 0.2, altitude);
        cloud *= horizonFade;

        skyColor = mix(skyColor, cloudColor, cloud * 0.85);
    }

    // --- Sun disc + glow + corona ---
    float sunAngle = acos(clamp(cosTheta, -1.0, 1.0));
    float sunDisc = smoothstep(0.035, 0.012, sunAngle);
    float sunGlow = exp(-sunAngle * 5.0) * 0.8;
    float sunCorona = exp(-sunAngle * 2.5) * 0.3;
    skyColor += vec3(1.0, 0.95, 0.8) * sunDisc * 3.0;
    skyColor += vec3(1.0, 0.9, 0.7) * sunGlow;
    skyColor += vec3(1.0, 0.85, 0.6) * sunCorona;

    // Below horizon: blend to ground/fog color
    vec3 groundColor = vec3(0.35, 0.40, 0.32);
    float horizonBlend = smoothstep(-0.15, 0.05, altitude);
    skyColor = mix(groundColor, skyColor, horizonBlend);

    FRAG_COLOR = vec4(skyColor, 1.0);
}
