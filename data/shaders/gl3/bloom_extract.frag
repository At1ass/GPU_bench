#version 130
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_scene_tex;
uniform float u_threshold;
void main() {
    vec3 color = texture(u_scene_tex, v_uv).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    FragColor = (brightness > u_threshold) ? vec4(color - u_threshold * 0.5, 1.0) : vec4(0.0);
}
