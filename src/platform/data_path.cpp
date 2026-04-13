// Cross-platform data path resolution and file reading.
// getExeDir() is implemented per-platform in data_path_*.cpp files.
#include "platform/data_path.h"
#include "platform/logger.h"
#include <SDL.h>
#include <cstring>
#include <sys/stat.h>

static bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool hasPathTraversal(const char* p) {
    if (!p) return true;
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
#else
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
#endif
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
