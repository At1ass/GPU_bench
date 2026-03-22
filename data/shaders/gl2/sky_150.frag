#version 150
in vec3 v_dir;
out vec4 FragColor;
uniform vec3 u_sun_dir;
uniform float u_time;

float hash2(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(443.897, 397.297, 491.187));
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

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

void main() {
    vec3 dir = normalize(v_dir);
    vec3 sun = normalize(u_sun_dir);

    float altitude = dir.y;

    vec3 betaR = vec3(5.8e-6, 13.5e-6, 33.1e-6) * 800.0;
    float cosTheta = dot(dir, sun);
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

    // Procedural cloud layer
    if (altitude > 0.02) {
        vec2 cloud_uv = dir.xz / max(dir.y, 0.05) * 0.8;
        cloud_uv += vec2(u_time * 0.015, u_time * 0.008);

        float cloud = fbm(cloud_uv * 1.2, 5);
        cloud = smoothstep(0.38, 0.7, cloud);

        float sunInfluence = max(cosTheta, 0.0) * 0.3;
        vec3 cloudBright = vec3(1.0, 0.98, 0.95);
        vec3 cloudDark = vec3(0.65, 0.68, 0.75);
        vec3 cloudColor = mix(cloudDark, cloudBright, 0.5 + sunInfluence);

        float horizonFade = smoothstep(0.02, 0.2, altitude);
        cloud *= horizonFade;

        skyColor = mix(skyColor, cloudColor, cloud * 0.85);
    }

    // Sun disc + glow + corona
    float sunAngle = acos(clamp(cosTheta, -1.0, 1.0));
    float sunDisc = smoothstep(0.035, 0.012, sunAngle);
    float sunGlow = exp(-sunAngle * 5.0) * 0.8;
    float sunCorona = exp(-sunAngle * 2.5) * 0.3;
    skyColor += vec3(1.0, 0.95, 0.8) * sunDisc * 3.0;
    skyColor += vec3(1.0, 0.9, 0.7) * sunGlow;
    skyColor += vec3(1.0, 0.85, 0.6) * sunCorona;

    skyColor *= smoothstep(-0.05, 0.1, altitude);

    FragColor = vec4(skyColor, 1.0);
}
