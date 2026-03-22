#version 130
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_depth_tex;
uniform sampler2D u_noise_tex;
uniform vec2 u_screen_size;   // viewport width, height
uniform float u_near;
uniform float u_far;
uniform float u_aspect;
uniform float u_tan_half_fov;
uniform float u_radius;       // SSAO sample radius in view space
uniform float u_bias;         // depth bias to prevent self-occlusion
uniform float u_intensity;    // AO strength multiplier

// Reconstruct linear view-space depth from depth buffer value
float linearDepth(float d) {
    float z_ndc = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z_ndc * (u_far - u_near));
}

// Reconstruct view-space position from UV and depth
vec3 viewPosFromDepth(vec2 uv) {
    float d = texture(u_depth_tex, uv).r;
    float z = linearDepth(d);
    vec2 ndc = uv * 2.0 - 1.0;
    float x = ndc.x * u_aspect * u_tan_half_fov * z;
    float y = ndc.y * u_tan_half_fov * z;
    return vec3(x, y, -z);
}

// 16-sample kernel (hemisphere, pre-weighted toward center)
const int SAMPLE_COUNT = 16;
const vec3 kernel[16] = vec3[16](
    vec3( 0.5381, 0.1856,-0.4319), vec3( 0.1379, 0.2486, 0.4430),
    vec3( 0.3371, 0.5679,-0.0057), vec3(-0.6999,-0.0451,-0.0019),
    vec3( 0.0689,-0.1598,-0.8547), vec3( 0.0560, 0.0069,-0.1843),
    vec3(-0.0146, 0.1402, 0.0762), vec3( 0.0100,-0.1924,-0.0344),
    vec3(-0.3577,-0.5301,-0.4358), vec3(-0.3169, 0.1063, 0.0158),
    vec3( 0.0103,-0.5869, 0.0046), vec3(-0.0897,-0.4940, 0.3287),
    vec3( 0.7119,-0.0154,-0.0918), vec3(-0.0533, 0.0596,-0.5411),
    vec3( 0.0352,-0.0631, 0.5460), vec3(-0.4776, 0.2847,-0.0271)
);

void main() {
    vec3 origin = viewPosFromDepth(v_uv);

    // Skip sky (far plane)
    if (-origin.z >= u_far * 0.99) {
        FragColor = vec4(1.0);
        return;
    }

    // Noise for random rotation of kernel (tile 4x4 noise texture)
    vec2 noise_uv = v_uv * u_screen_size / 4.0;
    vec3 random_vec = normalize(texture(u_noise_tex, noise_uv).xyz * 2.0 - 1.0);

    // Estimate normal from depth (cross product of partial derivatives)
    vec3 dpdx = viewPosFromDepth(v_uv + vec2(1.0/u_screen_size.x, 0.0)) - origin;
    vec3 dpdy = viewPosFromDepth(v_uv + vec2(0.0, 1.0/u_screen_size.y)) - origin;
    vec3 normal = normalize(cross(dpdx, dpdy));

    // Build TBN from normal + random vector
    vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float origin_depth = -origin.z;

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        // Orient sample to hemisphere around surface normal
        vec3 sample_pos = origin + TBN * kernel[i] * u_radius;

        // Project sample to screen space
        vec2 sample_uv;
        sample_uv.x = (sample_pos.x / (-sample_pos.z * u_aspect * u_tan_half_fov)) * 0.5 + 0.5;
        sample_uv.y = (sample_pos.y / (-sample_pos.z * u_tan_half_fov)) * 0.5 + 0.5;

        // Skip out-of-bounds samples
        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0)
            continue;

        // Sample depth at projected position
        float sample_depth = linearDepth(texture(u_depth_tex, sample_uv).r);

        // Range check: reject samples across large depth discontinuities
        float depth_diff = abs(origin_depth - sample_depth);
        float range_check = 1.0 - smoothstep(u_radius * 0.5, u_radius * 2.0, depth_diff);

        // Compare: is the sample occluded?
        occlusion += (sample_depth < -sample_pos.z - u_bias ? 1.0 : 0.0) * range_check;
    }

    float ao = 1.0 - (occlusion / float(SAMPLE_COUNT)) * u_intensity;
    ao = clamp(ao, 0.0, 1.0);
    FragColor = vec4(ao, ao, ao, 1.0);
}
