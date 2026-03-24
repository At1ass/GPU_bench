// Uber shader — version-portable via ShaderCache preamble.
void main() {
    // Depth-only pass. Fragment shader required but writes nothing useful.
    FRAG_COLOR = vec4(1.0);
}
