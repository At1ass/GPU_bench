#version 430

struct Particle {
    vec4 pos;   // xyz = position, w = lifetime
    vec4 vel;   // xyz = velocity, w = max_lifetime
    vec4 color; // rgba
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

uniform mat4 u_proj;
uniform mat4 u_view;
uniform vec3 u_cam_pos;
uniform vec3 u_cam_right;
uniform vec3 u_cam_up;

out vec2 v_quad_uv;
out vec4 v_color;

// Must match particles_t4.comp layout
const uint FIREFLY_COUNT = 256u;
const uint TORCH_COUNT = 2u;
const float EMBER_RATIO = 0.2;

void main() {
    // Each particle is a quad (6 vertices = 2 triangles): particle index = gl_VertexID / 6
    uint particle_idx = uint(gl_VertexID) / 6u;
    uint vertex_in_quad = uint(gl_VertexID) % 6u;
    // Map 6 vertices to 4 corners: BL, BR, TL,  TL, BR, TR
    uint corner_map[6] = uint[6](0u, 1u, 2u, 2u, 1u, 3u);
    uint corner = corner_map[vertex_in_quad];

    if (particle_idx >= particles.length()) {
        gl_Position = vec4(0.0);
        return;
    }

    Particle p = particles[particle_idx];

    // Skip dead particles
    if (p.pos.w <= 0.0) {
        gl_Position = vec4(-9999.0);
        return;
    }

    float life_ratio = p.pos.w / max(p.vel.w, 0.01);
    bool is_firefly = (particle_idx < FIREFLY_COUNT);

    // Determine sub-type for fire particles
    uint sparks_total = particles.length() - FIREFLY_COUNT;
    uint sparks_per_torch = sparks_total / TORCH_COUNT;
    uint spark_idx = particle_idx - FIREFLY_COUNT;
    uint local_idx = spark_idx % sparks_per_torch;
    uint ember_threshold = uint(float(sparks_per_torch) * (1.0 - EMBER_RATIO));
    bool is_ember = !is_firefly && (local_idx >= ember_threshold);

    float size;
    if (is_firefly) {
        // Firefly: moderate size, pulses with brightness
        float base = 0.025 + 0.015 * smoothstep(0.0, 0.2, life_ratio) * smoothstep(1.0, 0.5, life_ratio);
        size = base * (1.0 + 0.3 * p.color.a);
    } else if (is_ember) {
        // Ember: larger chunk, shrinks slowly
        size = 0.025 + 0.020 * life_ratio;
    } else {
        // Spark: tiny, fast-fading
        size = 0.006 + 0.008 * life_ratio;
    }

    // Quad corner offsets (camera-facing billboard)
    // Corners: 0=BL, 1=BR, 2=TL, 3=TR
    vec2 offsets[4] = vec2[4](
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );
    vec2 off = offsets[corner];

    vec3 world_pos = p.pos.xyz
                   + u_cam_right * off.x * size
                   + u_cam_up * off.y * size;

    v_quad_uv = off * 0.5 + 0.5;
    v_color = p.color;

    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}
