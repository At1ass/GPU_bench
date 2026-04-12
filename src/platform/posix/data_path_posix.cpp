// Platform: Linux, FreeBSD
#include "platform/data_path.h"
#include <unistd.h>
#include <cstring>

std::string getExeDir() {
#if defined(__FreeBSD__)
    const char* link = "/proc/curproc/file";
#else
    const char* link = "/proc/self/exe";
#endif
    char buf[1024];
    ssize_t len = readlink(link, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        char* slash = strrchr(buf, '/');
        if (slash) { *(slash + 1) = '\0'; return std::string(buf); }
    }
    return std::string();
}
