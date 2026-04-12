#pragma once

// Modern extension checking (GL 3.0+ glGetStringi).
// Fallback: legacy glGetString(GL_EXTENSIONS) for GL 2.x / GLES 2.0.
// Ref: Khronos "Get Context Info" wiki — glGetString(GL_EXTENSIONS)
//      is deprecated in core profile GL 3.1+.
//
// Usage:
//   GLExtensions::init();                    // after GL context + version parse
//   if (GLExtensions::has("GL_ARB_debug_output")) { ... }

class GLExtensions {
public:
    static void init();
    static bool has(const char* extension_name);

private:
    static bool initialized_;
};
