#include "platform/data_path.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif

// Try to get the executable directory from /proc/self/exe (Linux),
// or fall back to empty string.
std::string getExeDir() {
#if defined(__ANDROID__)
    const char* path = SDL_AndroidGetInternalStoragePath();
    if (path) return std::string(path) + "/";
    return std::string();
#elif defined(__linux__) || defined(__FreeBSD__)
    char buf[1024];
#if defined(__linux__)
    const char* link = "/proc/self/exe";
#else
    const char* link = "/proc/curproc/file";
#endif
    ssize_t len = readlink(link, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        // Strip filename to get directory
        char* slash = strrchr(buf, '/');
        if (slash) { *(slash + 1) = '\0'; return std::string(buf); }
    }
#elif defined(__APPLE__)
    char buf[1024];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        char* slash = strrchr(buf, '/');
        if (slash) { *(slash + 1) = '\0'; return std::string(buf); }
    }
#elif defined(_WIN32)
    char buf[1024];
    DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (len > 0 && len < sizeof(buf)) {
        char* slash = strrchr(buf, '\\');
        if (slash) { *(slash + 1) = '\0'; return std::string(buf); }
    }
#endif
    return std::string();
}

static bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static bool hasPathTraversal(const char* p) {
    if (!p) return true;
    // Reject ".." components: standalone, leading, trailing, or embedded
    for (const char* s = p; *s; ) {
        if (s[0] == '.' && s[1] == '.' && (s[2] == '/' || s[2] == '\\' || s[2] == '\0'))
            return true;
        while (*s && *s != '/' && *s != '\\') s++;
        if (*s) s++;
    }
    return false;
}

std::string getDataPath(const char* relative_path) {
    if (!relative_path || hasPathTraversal(relative_path)) return std::string();
#ifdef __ANDROID__
    // On Android, SDL_RWFromFile auto-resolves paths from APK assets/
    return std::string(relative_path);
#endif
    // 1. ./data/
    {
        std::string p = std::string("data/") + relative_path;
        if (fileExists(p)) {
            LOG_DBG("DataPath: '%s' -> '%s'", relative_path, p.c_str());
            return p;
        }
    }

    // 2. <exe_dir>/data/
    std::string exe_dir = getExeDir();
    if (!exe_dir.empty()) {
        std::string p = exe_dir + "data/" + relative_path;
        if (fileExists(p)) {
            LOG_DBG("DataPath: '%s' -> '%s'", relative_path, p.c_str());
            return p;
        }

        // 3. <exe_dir>/../share/gpu_benchmark/data/
        std::string p2 = exe_dir + "../share/gpu_benchmark/data/" + relative_path;
        if (fileExists(p2)) {
            LOG_DBG("DataPath: '%s' -> '%s'", relative_path, p2.c_str());
            return p2;
        }
    }

    LOG_DBG("DataPath: '%s' not found", relative_path);
    return std::string();
}

std::string readTextFile(const char* path) {
    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    if (!rw) return std::string();

    Sint64 size = SDL_RWsize(rw);
    if (size <= 0) { SDL_RWclose(rw); return std::string(); }

    std::string content;
    content.resize(static_cast<size_t>(size));
    size_t read = SDL_RWread(rw, &content[0], 1, static_cast<size_t>(size));
    content.resize(read);
    SDL_RWclose(rw);
    return content;
}
