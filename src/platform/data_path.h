#pragma once
#include <string>

// Get directory containing the running executable (with trailing separator).
// Returns empty string if detection fails.
std::string getExeDir();

// Resolve a path relative to the data/ directory.
// Searches: ./data/, <exe_dir>/data/, <exe_dir>/../share/gpu_benchmark/data/
// Returns absolute path if found, empty string otherwise.
std::string getDataPath(const char* relative_path);

// Read entire text file into string. Returns empty on failure.
std::string readTextFile(const char* path);
