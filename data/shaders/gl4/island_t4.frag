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
uniform float u_is_water;
uniform mat4 u_proj;
uniform mat4 u_view;

// Water reflection uniforms (scene copy for SSR)
uniform sampler2D u_reflection_tex;
uniform sampler2D u_depth_tex;
uniform float u_has_reflection;
uniform vec2 u_screen_size;
uniform float u_near;
uniform float u_far;

#pragma include "noise_lib.glsl"
#pragma include "terrain_color.glsl"
#pragma include "pbr_lib.glsl"
#pragma include "pcss_lib.glsl"

// Water: smooth wave height for ripples (sin-based, no texture)
float waterWave(vec2 p, float time) {
    float w = 0.0;
    // Large gentle swell
    w += sin(p.x * 2.1 + time * 0.6) * sin(p.y * 1.8 + time * 0.4) * 0.04;
    // Medium wind chop
    w += sin(p.x * 5.3 - time * 1.1 + p.y * 2.7) * 0.02;
    w += sin(p.y * 6.1 + time * 0.9 - p.x * 1.3) * 0.015;
    // Fine ripples
    w += sin(p.x * 12.7 + time * 1.8 + p.y * 8.3) * 0.008;
    w += sin(p.y * 14.1 - time * 2.1 + p.x * 9.7) * 0.006;
    // Very fine detail
    w += sin(p.x * 23.0 + time * 2.5) * sin(p.y * 19.0 - time * 1.7) * 0.003;
    return w;
}

// Compute water surface normal from wave derivatives
vec3 waterNormal(vec3 worldPos, float time) {
    vec2 p = worldPos.xz;
    float eps = 0.03;
    float h  = waterWave(p, time);
    float hx = waterWave(p + vec2(eps, 0.0), time);
    float hz = waterWave(p + vec2(0.0, eps), time);
    float dx = (hx - h) / eps;
    float dz = (hz - h) / eps;
    // Strength controls how pronounced the ripples are
    return normalize(vec3(-dx, 1.0, -dz));
}

// Fresnel for puddle water (IOR 1.33 -> F0 = 0.02; boosted to 0.04 for visual clarity)
float waterFresnel(float NdotV) {
    float f0 = 0.04;
    return f0 + (1.0 - f0) * pow(1.0 - NdotV, 5.0);
}

// Screen-space reflection for water: ray march through scene depth buffer
float waterLinearizeDepth(float d) {
    return u_near * u_far / (u_far - d * (u_far - u_near));
}

vec4 waterSSR(vec3 fragPos, vec3 V, vec3 N, mat4 proj, mat4 view) {
    if (u_has_reflection < 0.5) return vec4(0.0);

    vec3 reflDir = reflect(-V, N);
    // Don't reflect downward
    if (reflDir.y < 0.05) return vec4(0.0);

    float step_size = 0.15;
    vec3 ray = fragPos;
    float traveled = 0.0;

    for (int i = 0; i < 32; i++) {
        ray += reflDir * step_size;
        traveled += step_size;
        if (traveled > 12.0) break;

        // Project ray position to screen UV
        vec4 proj_pos = proj * view * vec4(ray, 1.0);
        if (proj_pos.w <= 0.0) break;
        vec2 sample_uv = proj_pos.xy / proj_pos.w * 0.5 + 0.5;

        if (sample_uv.x < 0.01 || sample_uv.x > 0.99 ||
            sample_uv.y < 0.01 || sample_uv.y > 0.99) break;

        float scene_depth = waterLinearizeDepth(texture(u_depth_tex, sample_uv).r);
        float ray_depth = -(view * vec4(ray, 1.0)).z;
        float diff = ray_depth - scene_depth;

        // Hit: ray went behind scene geometry
        if (diff > 0.0 && diff < step_size * 3.0) {
            vec3 hit_color = texture(u_reflection_tex, sample_uv).rgb;

            // Confidence: fade at edges and far distance
            float edge = smoothstep(0.0, 0.08, sample_uv.x) * smoothstep(1.0, 0.92, sample_uv.x)
                       * smoothstep(0.0, 0.08, sample_uv.y) * smoothstep(1.0, 0.92, sample_uv.y);
            float dist_fade = 1.0 - smoothstep(4.0, 12.0, traveled);
            float confidence = edge * dist_fade * 0.7;

            return vec4(hit_color, confidence);
        }

        step_size *= 1.06; // adaptive: grow step for far objects
    }

    return vec4(0.0);
}

// Subsurface scattering approximation
vec3 computeSSS(vec3 N, vec3 V, vec3 L, vec3 albedo, float strength) {
    // Back-lighting term: light passes through thin geometry
    vec3 backLight = -L + N * 0.3;
    float VdotBack = pow(max(dot(V, normalize(backLight)), 0.0), 3.0);

    // Wrap lighting: broader SSS contribution even when not directly backlit
    float NdotL_wrap = (dot(N, L) + 0.6) / 1.6;
    float wrapSSS = max(NdotL_wrap, 0.0) * 0.4;

    // Thickness approximation: thin at silhouettes and high parts (ears)
    float thickness = 1.0 - abs(dot(N, V));
    float heightFactor = smoothstep(0.2, 1.0, v_world_pos.y); // ears are high
    thickness = thickness * 0.4 + heightFactor * 0.6;

    // Warm SSS color (light through skin — pinkish-red for organic material)
    vec3 sss_color = vec3(1.0, 0.35, 0.25);
    return sss_color * (VdotBack + wrapSSS) * thickness * strength;
}

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cam_pos - v_world_pos);
    vec3 L = normalize(u_light_dir);

    // ============================================================
    // WATER MATERIAL PATH
    // ============================================================
    if (u_is_water > 0.5) {
        // Rippled normal from wind waves
        N = waterNormal(v_world_pos, u_time);

        // Fresnel: more reflective at glancing angles
        float NdotV = max(dot(N, V), 0.0);
        float fresnel = waterFresnel(NdotV);

        // --- Refracted (bottom visible through water) ---
        float bottom_noise = waterWave(v_world_pos.xz * 2.0, u_time * 0.3) * 4.0;
        vec3 bottom_color = vec3(0.08, 0.09, 0.10) + vec3(0.02) * bottom_noise;
        // Slight blue-grey tint from water depth
        vec3 refracted = bottom_color * vec3(0.85, 0.90, 0.95);

        // --- Reflected (sky environment) ---
        vec3 R = reflect(-V, N);
        float sky_t = clamp(R.y * 0.7 + 0.3, 0.0, 1.0);
        vec3 sky_horizon = vec3(0.55, 0.60, 0.68);
        vec3 sky_zenith = vec3(0.38, 0.52, 0.78);
        vec3 reflected = mix(sky_horizon, sky_zenith, sky_t);

        // --- Specular sun highlight ---
        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float spec = pow(NdotH, 512.0);     // very tight sun spot
        float spec_soft = pow(NdotH, 64.0); // softer sun spread
        vec3 sun_spec = vec3(1.0, 0.95, 0.85) * (spec * 5.0 + spec_soft * 0.3);

        // Shadow
        float shadow = 1.0;
        if (u_has_shadow > 0.5) {
            shadow = computePCSSShadow(u_shadow_map, v_light_pos, u_shadow_texel_size, u_light_size);
        }

        // Screen-space scene reflection (bunny, grass, rocks)
        vec4 ssr_result = waterSSR(v_world_pos, V, vec3(0.0, 1.0, 0.0), u_proj, u_view);

        // Blend: sky reflection as base, SSR overlays scene objects
        vec3 final_reflected = reflected;
        if (ssr_result.a > 0.01) {
            final_reflected = mix(reflected, ssr_result.rgb, ssr_result.a);
        }

        // Combine: water = lerp(bottom, reflection, fresnel) + sun specular
        vec3 color = mix(refracted, final_reflected, fresnel) + sun_spec * shadow;

        // Edge: shallow water at borders fades toward terrain
        vec2 puddleUV = v_uv - 0.5;
        float edge = smoothstep(0.35, 0.48, length(puddleUV));
        color = mix(color, refracted, edge * 0.2);

        // Point light specular on water
        for (int i = 0; i < u_point_light_count && i < 3; i++) {
            vec3 to_light = u_point_lights[i] - v_world_pos;
            float d = length(to_light);
            vec3 pl_dir = to_light / d;
            float atten = 1.0 / (1.0 + 0.3 * d + 0.4 * d * d);
            vec3 pH = normalize(V + pl_dir);
            float pSpec = pow(max(dot(N, pH), 0.0), 128.0);
            color += u_point_colors[i] * pSpec * atten * 2.0;
        }

        // Fog
        float dist = length(v_world_pos - u_cam_pos);
        float fog = 1.0 - exp(-dist * u_fog_density);
        color = mix(color, u_fog_color, fog);

        FragColor = vec4(color, u_alpha);
        return;
    }

    // ============================================================
    // STANDARD MATERIAL PATH
    // ============================================================

    // Normal map blending
    if (u_has_normal_map > 0.5) {
        vec3 nm = texture(u_normal_map, v_uv).rgb * 2.0 - 1.0;
        N = normalize(N + nm * 0.5);
    }

    if (u_normal_strength > 0.0)
        N = perturbNormal(N, v_world_pos, v_uv, u_normal_strength);

    // Procedural terrain coloring
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
    vec3 sun_radiance = vec3(1.0, 0.95, 0.85) * 2.5; // HDR sun intensity
    vec3 color = ambient + cookTorranceBRDF(N, V, L, albedo, metallic, roughness, sun_radiance) * shadow;

    // Subsurface scattering (bunny ears glow when backlit)
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

    // Fog (applied in linear HDR space)
    float dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-dist * u_fog_density * 1.5);
    // Terrain edge fade: gradual blend starting further out for smooth horizon
    float edge_fade = smoothstep(7.0, 16.0, dist);
    fog = max(fog, edge_fade);
    color = mix(color, u_fog_color, fog);

    // HDR output - do NOT clamp, tone mapping done in composite pass
    FragColor = vec4(color, u_alpha);
}
