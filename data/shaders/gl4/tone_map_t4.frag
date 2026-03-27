#version 430
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_scene_tex;  // HDR scene
uniform sampler2D u_bloom_tex;  // Bloom buffer
uniform sampler2D u_ssao_tex;   // SSAO occlusion (single channel)
uniform sampler2D u_fog_tex;    // Volumetric fog (rgb = fog color, a = density)
uniform float u_has_ssao;
uniform float u_has_fog;
uniform float u_bloom_strength;
uniform vec2 u_viewport_size;
uniform float u_time;
uniform float u_exposure;
uniform float u_chromatic_strength;
uniform float u_grain_strength;
uniform sampler2D u_ssr_tex;   // SSR reflection (rgb = color, a = confidence)
uniform float u_has_ssr;
uniform sampler2D u_dof_tex;   // DoF result
uniform float u_has_dof;

// ACES filmic tone mapping (Narkowicz 2016 fit, pre-exposure corrected)
vec3 acesToneMap(vec3 x) {
    // Narkowicz 2016 reference fit (pre-exposed)
    x *= 0.6;
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Hash for film grain
float grainHash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Sanitize: clamp to valid HDR range, reject NaN/Inf
vec3 sanitizeHDR(vec3 c) {
    // NaN check: NaN != NaN
    if (c.r != c.r) c.r = 0.0;
    if (c.g != c.g) c.g = 0.0;
    if (c.b != c.b) c.b = 0.0;
    return clamp(c, vec3(0.0), vec3(100.0)); // cap extreme HDR values
}

void main() {
    vec3 hdr_color = sanitizeHDR(texture(u_scene_tex, v_uv).rgb);

    // Apply SSAO (darkens ambient-occluded areas)
    if (u_has_ssao > 0.5) {
        float ao = texture(u_ssao_tex, v_uv).r;
        ao = clamp(ao, 0.0, 1.0);
        hdr_color *= ao;
    }

    // Blend SSR reflections
    if (u_has_ssr > 0.5) {
        vec4 ssr = texture(u_ssr_tex, v_uv);
        ssr.rgb = sanitizeHDR(ssr.rgb);
        ssr.a = clamp(ssr.a, 0.0, 1.0);
        hdr_color = mix(hdr_color, ssr.rgb, ssr.a * 0.85);
    }

    // Blend volumetric fog
    if (u_has_fog > 0.5) {
        vec4 fog = texture(u_fog_tex, v_uv);
        fog.rgb = sanitizeHDR(fog.rgb);
        fog.a = clamp(fog.a, 0.0, 1.0);
        hdr_color = hdr_color * (1.0 - fog.a) + fog.rgb;
    }

    // Add bloom (additive, in HDR space before tone mapping)
    vec3 bloom = sanitizeHDR(texture(u_bloom_tex, v_uv).rgb);
    hdr_color += bloom * u_bloom_strength;

    // Apply DoF (blend focus-blurred version with composited result)
    if (u_has_dof > 0.5) {
        vec4 dof_data = texture(u_dof_tex, v_uv);
        dof_data.rgb = sanitizeHDR(dof_data.rgb);
        // Alpha channel carries blur amount (0 = sharp/in-focus, 1 = fully blurred)
        float blur_amount = clamp(dof_data.a, 0.0, 1.0);
        // Re-apply SSAO + fog to the blurred color for consistency
        if (u_has_ssao > 0.5) {
            float ao = clamp(texture(u_ssao_tex, v_uv).r, 0.0, 1.0);
            dof_data.rgb *= ao;
        }
        if (u_has_fog > 0.5) {
            vec4 fog = texture(u_fog_tex, v_uv);
            fog.rgb = sanitizeHDR(fog.rgb);
            fog.a = clamp(fog.a, 0.0, 1.0);
            dof_data.rgb = dof_data.rgb * (1.0 - fog.a) + fog.rgb;
        }
        // Blend: out-of-focus areas use DoF result, in-focus keeps composited
        hdr_color = mix(hdr_color, dof_data.rgb, blur_amount);
    }

    // Auto-exposure (u_exposure = 0 means disabled / use default 1.0)
    float exposure = u_exposure > 0.001 ? u_exposure : 1.0;
    hdr_color *= exposure;

    // ACES filmic tone mapping (HDR -> LDR)
    vec3 ldr = acesToneMap(hdr_color);

    // Gamma correction (linear -> sRGB)
    ldr = pow(ldr, vec3(1.0 / 2.2));

    // Chromatic aberration: approximate RGB channel shift using screen-space gradients
    // Uses dFdx/dFdy on the computed LDR to avoid re-sampling HDR (which caused
    // green tint due to inconsistent pipeline between R/B and G channels)
    if (u_chromatic_strength > 0.0) {
        vec2 ca_center = v_uv - 0.5;
        float ca_dist_sq = dot(ca_center, ca_center);
        vec2 ca_offset = ca_center * u_chromatic_strength * ca_dist_sq;

        // Scale offset to pixel units for gradient approximation
        vec2 offset_pixels = ca_offset * u_viewport_size;
        // LDR gradients per pixel (screen-space derivatives)
        vec3 dldx = dFdx(ldr);
        vec3 dldy = dFdy(ldr);
        // Shift R outward, B inward using Taylor approximation
        ldr.r += dot(offset_pixels, vec2(dldx.r, dldy.r));
        ldr.b -= dot(offset_pixels, vec2(dldx.b, dldy.b));
    }

    // Vignette: darken edges for cinematic feel
    vec2 center = v_uv - 0.5;
    float aspect = u_viewport_size.x / max(u_viewport_size.y, 1.0);
    center.x *= aspect;
    float vignette_dist = length(center);
    float vignette = 1.0 - smoothstep(0.4, 0.9, vignette_dist) * 0.45;
    ldr *= vignette;

    // Subtle color grading: warm highlights, cool shadows
    float luminance = dot(ldr, vec3(0.2126, 0.7152, 0.0722));
    vec3 warm_tint = vec3(1.02, 1.0, 0.96);
    vec3 cool_tint = vec3(0.96, 0.98, 1.04);
    vec3 grade = mix(cool_tint, warm_tint, smoothstep(0.2, 0.8, luminance));
    ldr *= grade;

    // Film grain
    if (u_grain_strength > 0.0) {
        float grain = grainHash(gl_FragCoord.xy + fract(u_time * 7.23) * 100.0);
        grain = grain * 2.0 - 1.0; // remap to [-1, 1]
        ldr += grain * u_grain_strength;
    }

    // Final clamp to valid LDR range
    FragColor = vec4(clamp(ldr, 0.0, 1.0), 1.0);
}
