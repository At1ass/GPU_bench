#version 150
out vec4 FragColor;
void main() {
    // Depth-only pass. Fragment shader required but writes nothing useful.
    FragColor = vec4(1.0);
}
