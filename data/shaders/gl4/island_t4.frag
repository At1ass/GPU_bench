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

// ============================================================
// WATER RENDERING: Gerstner waves, Beer's law, GGX specular, SSR
// ============================================================

// GGX normal distribution for water specular
float waterGGX(float NdotH, float roughness) {
    float a2 = roughness * roughness;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159 * d * d);
}

// Gerstner wave: returns (displacement.xz, height) and accumulates normal
void gerstnerWave(vec2 pos, float time, vec2 dir, float amp, float freq,
                  float speed, float steep, inout vec3 offset, inout vec3 norm) {
    float phase = freq * dot(dir, pos) + time * speed;
    float s = sin(phase);
    float c = cos(phase);
    // Displacement
    offset.x -= dir.x * steep * amp * c;
    offset.y += amp * s;
    offset.z -= dir.y * steep * amp * c;
    // Analytical normal accumulation
    norm.x -= dir.x * freq * amp * c;
    norm.y -= steep * freq * amp * s;
    norm.z -= dir.y * freq * amp * c;
}

// Compute Gerstner wave displacement and analytical normal (4 octaves for pond)
void waterGerstner(vec2 pos, float time, out float height, out vec3 normal) {
    vec3 offset = vec3(0.0);
    vec3 norm = vec3(0.0, 1.0, 0.0); // start from flat

    // Wave 0: gentle swell
    gerstnerWave(pos, time, normalize(vec2(0.7, 0.7)),  0.012, 4.2, 0.6, 0.2, offset, norm);
    // Wave 1: cross-chop
    gerstnerWave(pos, time, normalize(vec2(-0.3, 0.9)), 0.006, 8.5, 1.1, 0.15, offset, norm);
    // Wave 2: ripple
    gerstnerWave(pos, time, normalize(vec2(1.0, 0.15)), 0.003, 15.0, 1.8, 0.1, offset, norm);
    // Wave 3: micro-ripple
    gerstnerWave(pos, time, normalize(vec2(0.5, -0.8)), 0.0015, 25.0, 2.4, 0.1, offset, norm);

    height = offset.y;
    normal = normalize(norm);
}

// Schlick Fresnel for water (IOR 1.33, F0 ≈ 0.02)
// Minimum 15% reflection ensures SSR is visible even at steep viewing angles
float waterFresnel(float NdotV) {
    float f0 = 0.02;
    float fresnel = f0 + (1.0 - f0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    return max(fresnel, 0.15);
}

// Beer's law: depth-based light absorption in water
vec3 waterAbsorption(float depth) {
    // Red absorbed fast, blue absorbed slow
    vec3 absorb = vec3(0.45, 0.09, 0.04);
    vec3 extinction = exp(-absorb * max(depth, 0.0));
    vec3 deep_color = vec3(0.01, 0.04, 0.08);
    vec3 shallow_color = vec3(0.05, 0.25, 0.30);
    return mix(deep_color, shallow_color, extinction);
}

// Linearize depth from [0,1] depth buffer to view-space distance
float waterLinearizeDepth(float d) {
    return u_near * u_far / (u_far - d * (u_far - u_near));
}

// Project view-space position to screen pixel coords + linear depth
vec3 waterProjectToScreen(vec3 viewPos, mat4 proj) {
    vec4 clip = proj * vec4(viewPos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    return vec3((ndc.xy * 0.5 + 0.5) * u_screen_size, -viewPos.z);
}

// Screen-space DDA ray march for water reflections (Morgan McGuire 2014)
// Traces ray pixel-by-pixel in screen space for clean, artifact-free reflections.
vec4 waterSSR(vec3 fragPos, vec3 V, vec3 N, mat4 proj, mat4 view) {
    if (u_has_reflection < 0.5) return vec4(0.0);

    // Reflect view direction around water surface normal
    vec3 reflDir = reflect(-V, N);
    if (reflDir.y < 0.01) return vec4(0.0); // skip rays into the water

    // Transform to view space
    vec3 vsOrigin = (view * vec4(fragPos, 1.0)).xyz;
    vec3 vsDir = normalize((view * vec4(reflDir, 0.0)).xyz);

    // Bias along normal to prevent self-intersection
    vec3 vsNormal = normalize((view * vec4(N, 0.0)).xyz);
    vsOrigin += vsNormal * 0.05;

    // Project ray endpoints to screen space
    float ray_length = 25.0;
    vec3 vsEnd = vsOrigin + vsDir * ray_length;

    // Clip to near plane
    if (vsEnd.z > -u_near) {
        float t_clip = (-u_near - vsOrigin.z) / vsDir.z;
        vsEnd = vsOrigin + vsDir * max(t_clip, 0.0);
    }

    vec3 screen0 = waterProjectToScreen(vsOrigin, proj);
    vec3 screen1 = waterProjectToScreen(vsEnd, proj);

    // Screen-space delta — march along the longest axis
    vec2 delta = screen1.xy - screen0.xy;
    float len = max(abs(delta.x), abs(delta.y));
    if (len < 1.0) return vec4(0.0);

    int max_steps = min(int(len), 256);
    vec2 step_dir = delta / len;

    // Perspective-correct depth interpolation via 1/z
    float invZ0 = 1.0 / screen0.z;
    float invZ1 = 1.0 / screen1.z;

    // Per-pixel jitter to reduce banding
    float jitter = fract(dot(gl_FragCoord.xy, vec2(0.75487766, 0.56984029)));
    vec2 pixel = screen0.xy + step_dir * jitter;
    float t = jitter / len;
    float prev_ray_z = screen0.z;

    for (int i = 0; i < max_steps; i++) {
        pixel += step_dir;
        t += 1.0 / len;
        if (t > 1.0) break;

        vec2 uv = pixel / u_screen_size;
        if (uv.x < 0.005 || uv.x > 0.995 || uv.y < 0.005 || uv.y > 0.995) break;

        float ray_z = 1.0 / mix(invZ0, invZ1, t);
        float scene_z = waterLinearizeDepth(texture(u_depth_tex, uv).r);

        float diff = ray_z - scene_z;
        float thickness = max(ray_z * 0.05, 0.1);

        // Hit: ray crossed from in-front to behind geometry
        if (diff > 0.0 && diff < thickness && prev_ray_z <= scene_z + 0.01) {
            // Binary refinement for sub-pixel precision
            vec2 ref_pixel = pixel - step_dir;
            float ref_t = t - 1.0 / len;
            float half_step = 0.5;
            for (int j = 0; j < 8; j++) {
                ref_pixel += step_dir * half_step;
                ref_t += half_step / len;
                float rz = 1.0 / mix(invZ0, invZ1, ref_t);
                vec2 ruv = ref_pixel / u_screen_size;
                float sz = waterLinearizeDepth(texture(u_depth_tex, ruv).r);
                if (rz > sz) { ref_pixel -= step_dir * half_step; ref_t -= half_step / len; }
                half_step *= 0.5;
            }

            vec2 hit_uv = ref_pixel / u_screen_size;
            vec3 hit_color = texture(u_reflection_tex, hit_uv).rgb;

            // Confidence fading
            float edge = smoothstep(0.0, 0.1, hit_uv.x) * smoothstep(1.0, 0.9, hit_uv.x)
                       * smoothstep(0.0, 0.1, hit_uv.y) * smoothstep(1.0, 0.9, hit_uv.y);
            float dist_fade = 1.0 - t;
            float thick_fade = 1.0 - smoothstep(0.0, thickness, diff);
            float confidence = edge * dist_fade * thick_fade * 0.95;

            return vec4(hit_color, confidence);
        }

        prev_ray_z = ray_z;
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
    // WATER MATERIAL PATH (Gerstner + Beer's law + GGX + SSR)
    // ============================================================
    if (u_is_water > 0.5) {
        // --- Gerstner waves: analytical normal (no finite differences) ---
        float waveHeight;
        vec3 waveNormal;
        waterGerstner(v_world_pos.xz, u_time, waveHeight, waveNormal);
        N = waveNormal;

        float NdotV = max(dot(N, V), 0.001);
        float NdotL = max(dot(N, L), 0.0);

        // --- Shore edge fade: transparent at disc border ---
        vec2 puddleUV = v_uv - 0.5;
        float edgeDist = length(puddleUV) * 2.0; // 0 at center, 1 at edge
        float shoreFade = 1.0 - smoothstep(0.75, 0.98, edgeDist);
        // Waves diminish near shore
        N = normalize(mix(vec3(0.0, 1.0, 0.0), N, shoreFade));
        NdotV = max(dot(N, V), 0.001);

        // --- Fresnel (Schlick, F0=0.02 for water IOR 1.33) ---
        float fresnel = waterFresnel(NdotV);
        // Shore reduces reflectivity
        fresnel *= shoreFade;

        // --- Refracted: Beer's law absorption + ground color ---
        float waterDepth = 0.25 + 0.15 * (1.0 - edgeDist); // deeper at center
        vec3 waterBody = waterAbsorption(waterDepth);
        // Add subtle bottom caustic pattern
        float caustic = 0.5 + 0.5 * sin(v_world_pos.x * 8.0 + u_time * 0.5)
                       * sin(v_world_pos.z * 7.0 - u_time * 0.4);
        waterBody += vec3(0.03, 0.04, 0.02) * caustic * NdotL * shoreFade;

        // --- Reflected: sky gradient as fallback ---
        vec3 R = reflect(-V, N);
        float sky_t = clamp(R.y * 0.6 + 0.4, 0.0, 1.0);
        vec3 sky_horizon = vec3(0.55, 0.62, 0.72);
        vec3 sky_zenith = vec3(0.35, 0.50, 0.78);
        vec3 skyReflection = mix(sky_horizon, sky_zenith, sky_t);

        // --- SSR: screen-space ray-march reflections (bunny, columns, etc.) ---
        vec4 ssr = waterSSR(v_world_pos, V, N, u_proj, u_view);
        vec3 finalReflection = skyReflection;
        if (ssr.a > 0.01) {
            finalReflection = mix(skyReflection, ssr.rgb, ssr.a);
        }

        // --- Dual-lobe GGX sun specular ---
        vec3 H = normalize(V + L);
        float NdotH = max(dot(N, H), 0.0);
        float specSharp = waterGGX(NdotH, 0.03) * 2.5;  // tight sun disc
        float specSoft  = waterGGX(NdotH, 0.18) * 0.12;  // soft glow
        vec3 sunColor = vec3(1.0, 0.95, 0.85) * 2.5; // HDR sun
        vec3 sunSpec = sunColor * (specSharp + specSoft) * NdotL;

        // --- Shadow ---
        float shadow = 1.0;
        if (u_has_shadow > 0.5) {
            shadow = computePCSSShadow(u_shadow_map, v_light_pos, u_shadow_texel_size, u_light_size);
        }

        // --- Combine: lerp(refracted, reflected, fresnel) + sun specular ---
        vec3 color = mix(waterBody, finalReflection, fresnel) + sunSpec * shadow;

        // --- Point light specular (GGX) ---
        for (int i = 0; i < u_point_light_count && i < 3; i++) {
            vec3 to_light = u_point_lights[i] - v_world_pos;
            float d = length(to_light);
            vec3 pl_dir = to_light / d;
            float atten = 1.0 / (1.0 + 0.3 * d + 0.4 * d * d);
            vec3 pH = normalize(V + pl_dir);
            float pNdotH = max(dot(N, pH), 0.0);
            float pSpec = waterGGX(pNdotH, 0.08);
            color += u_point_colors[i] * pSpec * atten * 0.5;
        }

        // --- Fog ---
        float dist = length(v_world_pos - u_cam_pos);
        float fog = 1.0 - exp(-dist * u_fog_density);
        color = mix(color, u_fog_color, fog);

        // Shore alpha: water fades to transparent at edges
        float waterAlpha = shoreFade * u_alpha;

        FragColor = vec4(color, waterAlpha);
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
