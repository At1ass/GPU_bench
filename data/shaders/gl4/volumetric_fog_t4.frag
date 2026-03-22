#version 430
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_depth_tex;
uniform float u_near;
uniform float u_far;
uniform float u_aspect;
uniform float u_tan_half_fov;
uniform mat4 u_view_inv; // inverse view matrix (camera-to-world)
uniform float u_time;
uniform vec3 u_sun_dir;
uniform float u_fog_density;
uniform vec3 u_fog_color;
uniform int u_fog_steps;

// Linearize depth from depth buffer [0,1] to view-space distance
float linearizeDepth(float d) {
    return u_near * u_far / (u_far - d * (u_far - u_near));
}

// Simple 3D noise (sin-based, no texture lookups)
float fogNoise(vec3 p) {
    float n = sin(p.x * 1.3 + p.z * 0.7 + u_time * 0.15)
            * sin(p.y * 2.1 + p.x * 0.4 - u_time * 0.1)
            * sin(p.z * 1.7 + p.y * 0.9 + u_time * 0.08);
    n += sin(p.x * 3.7 + u_time * 0.2) * sin(p.z * 2.3 - u_time * 0.12) * 0.3;
    // Third octave for more detail
    n += sin(p.x * 5.1 - u_time * 0.08) * sin(p.y * 4.3 + u_time * 0.05) * sin(p.z * 3.9) * 0.15;
    return n * 0.5 + 0.5;
}

// Henyey-Greenstein phase function for anisotropic scattering
float henyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

void main() {
    // Reconstruct view-space ray direction from UV
    vec2 ndc = v_uv * 2.0 - 1.0;
    vec3 view_dir = vec3(
        ndc.x * u_tan_half_fov * u_aspect,
        ndc.y * u_tan_half_fov,
        -1.0
    );

    // Transform to world-space ray
    vec3 ray_dir = normalize((u_view_inv * vec4(view_dir, 0.0)).xyz);
    vec3 ray_origin = (u_view_inv * vec4(0.0, 0.0, 0.0, 1.0)).xyz;

    // Scene depth at this pixel
    float depth_sample = texture(u_depth_tex, v_uv).r;
    float scene_depth = linearizeDepth(depth_sample);

    // Dithered start to reduce banding
    float dither = fract(dot(gl_FragCoord.xy, vec2(0.75487766, 0.56984029)));

    // Raymarch parameters - use uniform step count (64 for T4)
    int steps = u_fog_steps > 0 ? u_fog_steps : 32;
    float max_dist = min(scene_depth, 30.0);
    float step_size = max_dist / float(steps);

    float t = step_size * dither;

    vec3 accumulated_fog = vec3(0.0);
    float accumulated_density = 0.0;
    float transmittance = 1.0;

    vec3 sun_dir = normalize(u_sun_dir);

    for (int i = 0; i < steps; i++) {
        if (transmittance < 0.01) break;

        vec3 sample_pos = ray_origin + ray_dir * t;

        // Height-based fog: denser near ground (y = -1 is ground level)
        float height_fog = exp(-max(sample_pos.y + 1.0, 0.0) * 0.8);

        // Noise-based density variation
        float noise_val = fogNoise(sample_pos * 0.4);
        float density = u_fog_density * height_fog * (0.5 + noise_val * 0.5);

        if (density > 0.001) {
            // Enhanced Henyey-Greenstein: stronger forward scattering for god rays
            float cos_angle = dot(ray_dir, sun_dir);

            // Dual-lobe phase: strong forward scatter + mild back scatter
            float phase_forward = henyeyGreenstein(cos_angle, 0.82); // strong forward lobe (god rays)
            float phase_back = henyeyGreenstein(cos_angle, -0.3);    // mild back scatter
            float phase = mix(phase_back, phase_forward, 0.85);      // 85% forward, 15% back

            // Sun light contribution through fog (increased intensity for god rays)
            vec3 sun_light = vec3(1.0, 0.95, 0.85) * phase * 2.5;

            // Ambient fog light (hemisphere)
            vec3 ambient_fog = u_fog_color * 0.4;

            // In-scattering at this step
            vec3 in_scatter = (sun_light + ambient_fog) * density * step_size;

            // Beer-Lambert absorption
            float step_transmittance = exp(-density * step_size);

            accumulated_fog += in_scatter * transmittance;
            transmittance *= step_transmittance;
        }

        t += step_size;
    }

    accumulated_density = 1.0 - transmittance;

    FragColor = vec4(accumulated_fog, accumulated_density);
}
