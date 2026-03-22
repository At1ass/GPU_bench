#version 430
in vec3 v_world_pos;
in vec3 v_world_normal;
in vec3 v_obj_pos;
in vec2 v_uv;
in float v_shell_index;
in vec4 v_light_pos;

out vec4 FragColor;

uniform sampler2D u_fur_tex;
uniform sampler2D u_fur_mask;
uniform float u_fur_tex_scale;
uniform float u_has_fur_mask;

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_fur_color_root;
uniform vec3 u_fur_color_tip;
uniform float u_fur_ao_power;
uniform float u_metallic;
uniform float u_roughness;
uniform sampler2D u_shadow_map;
uniform float u_has_shadow;
uniform vec3 u_point_lights[3];
uniform vec3 u_point_colors[3];
uniform int u_point_light_count;
uniform vec2 u_shadow_texel_size;
uniform float u_light_size;

#pragma include "pbr_lib.glsl"
#pragma include "pcss_lib.glsl"

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(443.8975, 397.2973, 491.1871));
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

float computeShadow() {
    if (u_has_shadow < 0.5) return 1.0;
    return computePCSSShadow(u_shadow_map, v_light_pos, u_shadow_texel_size, u_light_size);
}

// Sheen term for fur tips - approximates forward/back scattering through fibers
vec3 sheenBRDF(vec3 N, vec3 V, vec3 L, vec3 sheenColor, float sheenRoughness) {
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Charlie sheen distribution (softer than GGX, suited for cloth/fur)
    float invR = 1.0 / max(sheenRoughness, 0.001);
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;
    float sheen = (2.0 + invR) * pow(sin2h, invR * 0.5) / (2.0 * PI);

    return sheenColor * sheen * NdotL;
}

void main() {
    float h = v_shell_index;

    if (h < 0.01) {
        FragColor = vec4(u_fur_color_root, 1.0);
        return;
    }

    // --- Fur intensity from mask texture (model UVs) ---
    float fur_intensity = 1.0;
    if (u_has_fur_mask > 0.5) {
        fur_intensity = texture(u_fur_mask, v_uv).r;
    }

    // No fur where mask is black
    if (fur_intensity < 0.02) discard;

    // --- Strand pattern: smooth tri-planar (uniform density) ---
    vec3 an = abs(normalize(v_world_normal));
    vec3 w = an * an;
    w /= (w.x + w.y + w.z + 0.001);
    float scale = u_fur_tex_scale;
    float h_yz = texture(u_fur_tex, v_obj_pos.yz * scale).a;
    float h_xz = texture(u_fur_tex, v_obj_pos.xz * scale).a;
    float h_xy = texture(u_fur_tex, v_obj_pos.xy * scale).a;
    float strand_height = h_yz * w.x + h_xz * w.y + h_xy * w.z;
    float max_w = max(w.x, max(w.y, w.z));
    strand_height *= min(1.0 / max(max_w, 0.45), 2.2);
    strand_height = min(strand_height, 1.0);

    // Scale strand height by fur intensity (gray areas = shorter fur)
    strand_height *= fur_intensity;

    // Strand visible if this shell is below the strand's max height
    float alpha = 1.0 - smoothstep(strand_height - 0.08, strand_height, h);

    if (alpha < 0.01) discard;

    // --- Fur color ---
    float color_t = smoothstep(0.0, 0.9, h);
    vec3 fur_color = mix(u_fur_color_root, u_fur_color_tip, color_t);

    // Per-position color variation
    float color_var = hash21(floor(v_obj_pos.xz * scale * 0.5) + 53.0);
    fur_color *= 0.82 + 0.36 * color_var;

    // --- Self-occlusion (AO): darker at root ---
    float ao = pow(h, 1.0 / u_fur_ao_power);
    ao = mix(0.3, 1.0, ao);

    // --- PBR Lighting ---
    vec3 N = normalize(v_world_normal);
    vec3 V = normalize(u_cam_pos - v_world_pos);
    vec3 L = normalize(u_light_dir);

    // Fur uses low metallic, high roughness (matte, fibrous surface)
    float metallic = u_metallic * 0.05; // fur is almost entirely dielectric
    float roughness = max(u_roughness, 0.7); // fur is rough and fibrous

    // Shadow
    float shadow = computeShadow();

    // Hemisphere ambient (IBL approximation)
    float up = dot(N, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient_light = mix(vec3(0.22, 0.18, 0.14), vec3(0.48, 0.52, 0.62), up);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, fur_color, metallic);
    vec3 kD_ambient = (vec3(1.0) - fresnelSchlick(max(dot(N, V), 0.0), F0)) * (1.0 - metallic);
    vec3 ambient = kD_ambient * fur_color * ao * ambient_light * 0.3;

    // Sun light - Cook-Torrance
    vec3 sun_radiance = vec3(1.0, 0.95, 0.85) * 2.5;
    vec3 color = ambient + cookTorranceBRDF(N, V, L, fur_color * ao, metallic, roughness, sun_radiance) * shadow;

    // Sheen for fur tips (subtle, visible on outer shells only)
    if (h > 0.5) {
        float sheen_strength = smoothstep(0.5, 0.95, h) * 0.15;
        vec3 sheen_color = mix(fur_color, vec3(1.0, 0.98, 0.92), 0.4);
        color += sheenBRDF(N, V, L, sheen_color, 0.8) * sheen_strength * shadow;
    }

    // Fill light for depth
    if (h > 0.5) {
        vec3 fill_dir = normalize(vec3(-0.4, 0.3, -0.5));
        float fill = max(dot(N, fill_dir), 0.0) * 0.15;
        color += fur_color * ao * fill;
    }

    // Rim light (Fresnel-based)
    if (h > 0.5) {
        float NdotV = max(dot(N, V), 0.0);
        float rim = pow(1.0 - NdotV, 3.0) * 0.2;
        color += vec3(0.55, 0.60, 0.70) * rim * ao * shadow;
    }

    // Point lights - PBR
    for (int i = 0; i < u_point_light_count && i < 3; i++) {
        vec3 to_light = u_point_lights[i] - v_world_pos;
        float d = length(to_light);
        vec3 pl_dir = to_light / d;
        float atten = 1.0 / (1.0 + 0.3 * d + 0.4 * d * d);

        vec3 pl_radiance = u_point_colors[i] * atten * 3.0;
        color += cookTorranceBRDF(N, V, pl_dir, fur_color * ao, metallic, roughness, pl_radiance);

        // Subtle sheen from point lights on tip shells
        if (h > 0.5) {
            float sheen_strength = smoothstep(0.5, 0.95, h) * 0.08;
            vec3 sheen_color = mix(fur_color, vec3(1.0, 0.98, 0.92), 0.4);
            color += sheenBRDF(N, V, pl_dir, sheen_color, 0.8) * sheen_strength * atten;
        }
    }

    // Fog
    float cam_dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-cam_dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // HDR output
    FragColor = vec4(color, alpha);
}
