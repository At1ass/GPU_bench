// Uber shader — version-portable via ShaderCache preamble.
FS_IN vec2 v_uv;

uniform sampler2D u_scene_tex;
uniform sampler2D u_bloom_tex;
uniform sampler2D u_ssao_tex;
uniform float u_bloom_strength;
uniform float u_has_ssao;
uniform vec2 u_viewport_size;
void main() {
    vec3 scene = COMPAT_TEX2D(u_scene_tex, v_uv).rgb;
    vec3 bloom = COMPAT_TEX2D(u_bloom_tex, v_uv).rgb;

    // Apply SSAO
    if (u_has_ssao > 0.5) {
        float ao = COMPAT_TEX2D(u_ssao_tex, v_uv).r;
        scene *= ao;
    }

    vec3 color = scene + bloom * u_bloom_strength;

    // Vignette
    vec2 screen_uv = gl_FragCoord.xy / u_viewport_size;
    float vignette = smoothstep(0.9, 0.35, length(screen_uv - 0.5));
    color *= vignette;

    // Color grade: warm shift + contrast
    color = pow(color, vec3(0.95, 1.0, 1.08));
    color = mix(vec3(0.5), color, 1.15);
    color = clamp(color, 0.0, 1.0);

    FRAG_COLOR = vec4(color, 1.0);
}
