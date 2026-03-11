#pragma once
#include "preset.h"
#include <string>

// Save preset to INI file
bool saveConfig(const char* path, const BenchPreset& preset);

// Load preset from INI file. Returns false if file doesn't exist or is invalid.
bool loadConfig(const char* path, BenchPreset& preset);
