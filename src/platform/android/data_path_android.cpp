// Platform: Android
#include "platform/data_path.h"
#include <SDL.h>

std::string getExeDir() {
    const char* path = SDL_AndroidGetInternalStoragePath();
    if (path) return std::string(path) + "/";
    return std::string();
}

// Override: on Android, SDL_RWFromFile auto-resolves paths from APK assets/
// so getDataPath() returns the relative path directly.
// This is defined here and the generic version in data_path.cpp is excluded
// via CMake (data_path.cpp is still compiled for readTextFile and helpers).
