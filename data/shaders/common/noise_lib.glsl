float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float noise3d(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float n = i.x + i.y * 157.0 + 113.0 * i.z;
    float a = hash31(vec3(n, n, n));
    float b = hash31(vec3(n + 1.0, n, n));
    float c = hash31(vec3(n + 157.0, n, n));
    float d = hash31(vec3(n + 158.0, n, n));
    float e = hash31(vec3(n + 113.0, n, n));
    float ff = hash31(vec3(n + 114.0, n, n));
    float g = hash31(vec3(n + 270.0, n, n));
    float h = hash31(vec3(n + 271.0, n, n));
    return mix(mix(mix(a,b,f.x), mix(c,d,f.x), f.y),
               mix(mix(e,ff,f.x), mix(g,h,f.x), f.y), f.z);
}

vec2 rotate2d(vec2 p, float a) {
    float s = sin(a);
    float c = cos(a);
    return vec2(p.x * c - p.y * s, p.x * s + p.y * c);
}

float fbm(vec2 p, int octaves) {
    float val = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 8; i++) {
        if (i >= octaves) break;
        val += amp * noise(p * freq);
        p = rotate2d(p, 1.5);
        freq *= 2.0;
        amp *= 0.5;
    }
    return val;
}

float ridgeNoise(vec2 p) {
    return 1.0 - abs(noise(p) * 2.0 - 1.0);
}

float worley(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float md = 1.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = vec2(hash21(i + neighbor));
            vec2 diff = neighbor + point - f;
            float d = dot(diff, diff);
            md = min(md, d);
        }
    }
    return sqrt(md);
}
