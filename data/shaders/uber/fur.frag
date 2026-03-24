// Uber shader — version-portable via ShaderCache preamble.
FS_IN vec3 v_world_pos;
FS_IN vec3 v_world_normal;
FS_IN vec3 v_obj_pos;
FS_IN vec2 v_uv;
FS_IN float v_shell_index;

uniform sampler2D u_fur_tex;       // strand pattern (tiled)
uniform sampler2D u_fur_mask;      // intensity map from fur.png (model UVs)
uniform float u_fur_tex_scale;
uniform float u_has_fur_mask;      // 1.0 if mask loaded, 0.0 otherwise

uniform vec3 u_light_dir;
uniform vec3 u_cam_pos;
uniform vec3 u_fog_color;
uniform float u_fog_density;
uniform vec3 u_fur_color_root;
uniform vec3 u_fur_color_tip;
uniform float u_fur_ao_power;
uniform vec2 u_viewport_size;

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(443.8975, 397.2973, 491.1871));
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

void main() {
    float h = v_shell_index;

    if (h < 0.01) {
        FRAG_COLOR = vec4(u_fur_color_root, 1.0);
        return;
    }

    // --- Fur intensity from mask texture (model UVs) ---
    float fur_intensity = 1.0;
    if (u_has_fur_mask > 0.5) {
        fur_intensity = COMPAT_TEX2D(u_fur_mask, v_uv).r;
    }

    // No fur where mask is black
    if (fur_intensity < 0.02) discard;

    // --- Strand pattern: smooth tri-planar (uniform density) ---
    vec3 an = abs(normalize(v_world_normal));
    vec3 w = an * an;
    w /= (w.x + w.y + w.z + 0.001);
    float scale = u_fur_tex_scale;
    float h_yz = COMPAT_TEX2D(u_fur_tex, v_obj_pos.yz * scale).a;
    float h_xz = COMPAT_TEX2D(u_fur_tex, v_obj_pos.xz * scale).a;
    float h_xy = COMPAT_TEX2D(u_fur_tex, v_obj_pos.xy * scale).a;
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

    // --- Lighting ---
    vec3 normal = normalize(v_world_normal);
    vec3 l = normalize(u_light_dir);
    vec3 vd = normalize(u_cam_pos - v_world_pos);

    float diff = max(0.0, (dot(normal, l) + 0.4) / 1.4);

    // Inner shells: skip expensive specular, rim, and fill light
    float spec = 0.0;
    float rim = 0.0;
    float fill = 0.0;
    if (h > 0.5) {
        vec3 half_vec = normalize(l + vd);
        spec = pow(max(dot(normal, half_vec), 0.0), 48.0) * 0.25 * color_t;
        float ndotv = max(dot(normal, vd), 0.0);
        rim = pow(1.0 - ndotv, 3.0) * 0.15;
        vec3 fill_dir = normalize(vec3(-0.4, 0.3, -0.5));
        fill = max(dot(normal, fill_dir), 0.0) * 0.12;
    }

    float up = dot(normal, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    vec3 ambient = mix(vec3(0.22, 0.18, 0.14), vec3(0.48, 0.52, 0.62), up) * fur_color * 0.3;

    vec3 color = ambient
               + fur_color * ao * (diff * 0.8 + fill)
               + vec3(1.0, 0.95, 0.85) * spec
               + vec3(0.55, 0.60, 0.70) * rim * ao;

    float cam_dist = length(v_world_pos - u_cam_pos);
    float fog = 1.0 - exp(-cam_dist * u_fog_density);
    color = mix(color, u_fog_color, fog);

    // Vignette: darken edges
    vec2 screen_uv = gl_FragCoord.xy / u_viewport_size;
    float vignette = smoothstep(0.9, 0.35, length(screen_uv - 0.5));
    color *= vignette;

    // Color grade: warm shift + contrast
    color = pow(color, vec3(0.95, 1.0, 1.08));
    color = mix(vec3(0.5), color, 1.15);
    color = clamp(color, 0.0, 1.0);

    FRAG_COLOR = vec4(color, alpha);
}
