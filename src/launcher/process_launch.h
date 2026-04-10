#pragma once
#include <string>
#include <vector>

// Launch a child process by name ("gpu_benchmark" or "gpu_demo").
// Locates the executable in the same directory as the launcher.
// args = CLI arguments (without the executable name itself).
// The launcher continues running after the child is spawned.
// Returns true if the process was started successfully.
bool launchProcess(const std::string& exe_name, const std::vector<std::string>& args);
