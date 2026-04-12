// Platform: macOS
#include "platform/data_path.h"
#include <mach-o/dyld.h>
#include <cstring>
#include <cstdint>

std::string getExeDir() {
    char buf[1024];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        char* slash = strrchr(buf, '/');
        if (slash) { *(slash + 1) = '\0'; return std::string(buf); }
    }
    return std::string();
}
