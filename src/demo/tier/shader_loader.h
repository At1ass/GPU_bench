#pragma once
#include <string>

namespace ShaderLoader {
    // Load shader from data/shaders/<relative_path>.
    // Processes #pragma include "file" directives (from data/shaders/common/).
    // Returns empty string and logs error if file not found.
    std::string load(const char* relative_path);
}
