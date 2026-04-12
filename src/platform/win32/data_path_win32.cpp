// Platform: Windows
#include "platform/data_path.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>

std::string getExeDir() {
    char buf[1024];
    DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (len > 0 && len < sizeof(buf)) {
        char* slash = strrchr(buf, '\\');
        if (slash) { *(slash + 1) = '\0'; return std::string(buf); }
    }
    return std::string();
}
