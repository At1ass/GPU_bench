#version 430
in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_light_pos;

out vec4 FragColor;

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_mat_color;
uniform float u_mat_spec;
uniform float u_alpha;
uniform float u_time;
uniform float u_normal_strength;
uniform float u_proc_tex;
uniform float u_metallic;
uniform float u_roughness;
uniform sampler2D u_shadow_map;
uniform float u_has_shadow;
uniform sampler2D u_normal_map;
uniform float u_has_normal_map;
uniform vec3 u_point_lights[3];
uniform vec3 u_point_colors[3];
uniform int u_point_light_count;
uniform vec2 u_shadow_texel_size;
uniform float u_light_size;
uniform float u_sss_strength;

#pragma include "noise_lib.glsl"
#pragma include "terrain_color.glsl"
#pragma include "pbr_lib.glsl"
#pragma include "pcss_lib.glsl"

// Subsurface scattering approximation
vec3 computeSSS(vec3 N, vec3 V, vec3 L, vec3 albedo, float strength) {
    vec3 backLight = -L + N * 0.3;
    float VdotBack = pow(max(dot(V, normalize(backLight)), 0.0), 3.0);

    // Wrap lighting for broader SSS
    float NdotL_wrap = (dot(N, L) + 0.6) / 1.6;
    float wrapSSS = max(NdotL_wrap, 0.0) * 0.4;

    // Thickness: thin at silhouettes, ears (high Y)
    float thickness = 1.0 - abs(dot(N, V));
    float heightFactor = smoothstep(0.2, 1.0, v_world_pos.y);
    thickness = thickness * 0.4 + heightFactor * 0.6;

    // Warm SSS color (light through skin — pinkish-red)
    vec3 sss_color = vec3(1.0, 0.35, 0.25);
    return sss_color * (VdotBack + wrapSSS) * thickness * strength;
}

void main() {
    vec3 N = normalize(v_normal);

    // Normal map blending
    if (u_has_normal_map > 0.5) {
        vec3 nm = texture(u_normal_map, v_uv).rgb * 2.0 - 1.0;
        N = normalize(N + nm * 0.5);
    }

    if (u_normal_strength > 0.0)
        N = perturbNormal(N, v_world_pos, v_uv, u_normal_strength);

    vec3 V = normalize(u_cam_pos - v_world_pos);
    vec3 L = normalize(u_light_dir);

    // Procedural terrain coloring for tessellated mesh
    vec3 albedo = surfaceColor(v_world_pos, N, v_uv, u_mat_color, u_proc_tex);

    float metallic = u_metallic;
    float roughness = u_roughness;

    // Hemisphere ambient (IBL approximation)
    float up = dot(N, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 sky_col = vec3(0.42, 0.46, 0.54);
    vec3 ground_col = vec3(0.22, 0.19, 0.16);
    vec3 ambient_light = mix(ground_col, sky_col, up);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 kD_ambient = (vec3(1.0) - fresnelSchlick(max(dot(N, V), 0.0), F0)) * (1.0 - metallic);
    vec3 ambient = kD_ambient * albedo * ambient_light;

    // PCSS Shadow
    float shadow = 1.0;
    if (u_has_shadow > 0.5) {
        shadow = computePCSSShadow(u_shadow_map, v_light_pos, u_shadow_texel_size, u_light_size);
    }

    // Directional light (sun) - PBR Cook-Torrance
    vec3 sun_radiance = vec3(1.0, 0.95, 0.85) * 2.5;
    vec3 color = ambient + cookTorranceBRDF(N, V, L, albedo, metallic, roughness, sun_radiance) * shadow;

    // SSS (visible pink glow through thin parts like ears)
    if (u_sss_strength > 0.0) {
        color += computeSSS(N, V, L, albedo, u_sss_strength) * shadow * 3.0;
    }

    // Point lights - PBR + SSS
    for (int i = 0; i < u_point_light_count && i < 3; i++) {
        vec3 to_light = u_point_lights[i] - v_world_pos;
        float dist = length(to_light);
        vec3 pl_dir = to_light / dist;
        float atten = 1.0 / (1.0 + 0.3 * dist + 0.4 * dist * dist);

        vec3 pl_radiance = u_point_colors[i] * atten * 3.0;
        color += cookTorranceBRDF(N, V, pl_dir, albedo, metallic, roughness, pl_radiance);
        if (u_sss_strength > 0.0) {
            color += computeSSS(N, V, pl_dir, albedo, u_sss_strength) * atten * 1.5;
        }
    }

    // Fog
    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // HDR output
    FragColor = vec4(color, u_alpha);
}
