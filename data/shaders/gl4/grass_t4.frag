#version 430
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_light_pos;
in float v_color_t;

out vec4 FragColor;

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
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

float computeShadow() {
    if (u_has_shadow < 0.5) return 1.0;
    return computePCSSShadow(u_shadow_map, v_light_pos, u_shadow_texel_size, u_light_size);
}

// Subsurface scattering approximation for grass translucency
vec3 subsurfaceScattering(vec3 N, vec3 V, vec3 L, vec3 albedo, float thickness) {
    // Back-lit translucency: light passes through thin grass blades
    vec3 backLight = -L + N * 0.3; // distortion along normal
    float VdotBack = max(dot(V, normalize(backLight)), 0.0);
    float backSSS = pow(VdotBack, 4.0) * thickness;

    // Forward scatter component
    float NdotL_wrap = (dot(N, L) + 0.5) / 1.5;
    float forwardSSS = max(NdotL_wrap, 0.0) * thickness * 0.3;

    // SSS light is tinted by albedo (transmitted through the blade)
    vec3 sss_color = albedo * vec3(1.2, 1.1, 0.6); // warm transmitted light
    return sss_color * (backSSS + forwardSSS);
}

void main() {
    // Color gradient: dark green at base, lighter yellow-green at tip
    vec3 base_color = vec3(0.15, 0.32, 0.08);
    vec3 tip_color = vec3(0.35, 0.50, 0.18);
    vec3 albedo = mix(base_color, tip_color, v_color_t);

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cam_pos - v_world_pos);
    vec3 L = normalize(u_light_dir);

    // Grass: low metallic, moderate-high roughness
    float metallic = u_metallic * 0.05;
    float roughness = max(u_roughness, 0.5);

    // Hemisphere ambient
    float up = dot(N, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient_light = mix(vec3(0.15, 0.12, 0.08), vec3(0.35, 0.40, 0.45), up);

    vec3 F0 = vec3(0.04);
    vec3 kD_ambient = (vec3(1.0) - fresnelSchlick(max(dot(N, V), 0.0), F0)) * (1.0 - metallic);
    vec3 ambient = kD_ambient * albedo * ambient_light * 0.4;

    float shadow = computeShadow();

    // Sun light - PBR (moderate intensity for grass to avoid over-bright)
    vec3 sun_radiance = vec3(1.0, 0.95, 0.85) * 1.8;
    vec3 color = ambient + cookTorranceBRDF(N, V, L, albedo, metallic, roughness, sun_radiance) * shadow;

    // Subsurface scattering - grass blades are thin and translucent
    float sss_thickness = smoothstep(0.0, 0.8, v_color_t) * 0.4; // thinner at base
    vec3 sss = subsurfaceScattering(N, V, L, albedo, sss_thickness);
    color += sss * shadow * 0.5; // subtle HDR contribution

    // Point lights - PBR + SSS (moderate intensity to avoid bloom bleed)
    for (int i = 0; i < u_point_light_count && i < 3; i++) {
        vec3 to_light = u_point_lights[i] - v_world_pos;
        float d = length(to_light);
        vec3 pl_dir = to_light / d;
        float atten = 1.0 / (1.0 + 0.5 * d + 0.6 * d * d);

        vec3 pl_radiance = u_point_colors[i] * atten * 1.5;
        color += cookTorranceBRDF(N, V, pl_dir, albedo, metallic, roughness, pl_radiance);
        color += subsurfaceScattering(N, V, pl_dir, albedo, sss_thickness) * atten * 0.15;
    }

    // Fog + distance fade
    float cam_dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-cam_dist * u_fog_density);
    fog = max(fog, smoothstep(8.0, 16.0, cam_dist));
    color = mix(color, u_fog_color, fog);

    // Alpha: fade at very tip for soft edges
    float alpha = smoothstep(1.0, 0.85, v_color_t);
    if (alpha < 0.01) discard;

    // HDR output
    FragColor = vec4(color, alpha);
}
