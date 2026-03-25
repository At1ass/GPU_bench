// Uber shader — torch flame billboard (T3+)
// Each vertex belongs to one of N torch quads.
// a_uv.x encodes torch index (0, 1, 2, ...).
// Position comes from fixed u_torch_positions[] (NOT orbiting point lights).

ATTR_IN vec3 a_pos;    // quad corner offset (-1..1 in XY)
ATTR_IN vec3 a_normal; // unused
ATTR_IN vec2 a_uv;     // x = torch index (0, 1, 2), y = unused

uniform mat4 u_proj;
uniform mat4 u_view;
uniform vec3 u_cam_pos;
uniform float u_time;
uniform vec3 u_torch_positions[2]; // fixed on gateway column tops

VS_OUT vec2 v_uv;         // quad UV [-1,1] remapped to [0,1]
VS_OUT float v_torch_id;  // which torch (for color variation)

void main() {
    int torch_idx = int(a_uv.x + 0.5);
    vec3 torch_pos = u_torch_positions[torch_idx];

    // Flame dimensions (world space) — generous size for visibility
    float flame_width = 0.40;
    float flame_height = 0.85;

    // Axial billboard: fixed Y-up, faces camera horizontally
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 to_cam = u_cam_pos - torch_pos;
    to_cam.y = 0.0; // project to horizontal
    float len = length(to_cam);
    vec3 right = len > 0.001 ? normalize(cross(up, to_cam)) : vec3(1.0, 0.0, 0.0);

    // Quad corner: bottom-center anchored at torch position
    // Shift so bottom of quad is at torch pos, flame goes up
    vec3 world_pos = torch_pos
        + right * a_pos.x * flame_width
        + up * (a_pos.y * 0.5 + 0.5) * flame_height;

    v_uv = a_pos.xy * 0.5 + 0.5; // [0,1] range
    v_torch_id = float(torch_idx);

    gl_Position = u_proj * u_view * vec4(world_pos, 1.0);
}
