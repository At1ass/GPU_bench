#pragma once
#include <string>
#include <vector>

struct GPUDevice {
    int index;              // DRM card index (Linux) or adapter index (Windows)
    std::string name;       // Human-readable name
    std::string vendor;     // "NVIDIA", "AMD", "Intel", etc.
    std::string pci_id;     // "vendor:device" hex string
    std::string pci_slot;   // PCI slot address e.g. "0000:11:00.0"
    bool is_integrated;     // Heuristic: iGPU vs dGPU
};

// Enumerate available GPUs. Works before SDL/GL init.
std::vector<GPUDevice> enumerateGPUs();

// Print GPU list to stderr.
void printGPUList(const std::vector<GPUDevice>& gpus);

// Select a GPU by index. Must be called BEFORE SDL_Init().
// Sets environment variables (DRI_PRIME on Linux).
// Returns true if the index is valid.
bool selectGPU(int index);

// Select GPU and re-exec if needed (for GLVND env vars that must be set
// before library loading). Call from main() before any SDL/GL calls.
// If re-exec happens, does not return. argv[0] must be valid.
void selectGPUAndReexec(int index, int argc, char* argv[]);
