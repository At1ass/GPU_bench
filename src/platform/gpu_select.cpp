// Cross-platform GPU list printing. Platform-specific implementations
// of enumerateGPUs(), selectGPU(), selectGPUAndReexec() are in gpu_select_*.cpp.
#include "platform/gpu_select.h"
#include <cstdio>

// PCI vendor ID to name mapping (shared by Linux and Windows implementations)
const char* pciVendorName(unsigned int vendor_id) {
    switch (vendor_id) {
        case 0x10de: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x1a03: return "ASPEED";
        case 0x1234: return "QEMU/Bochs";
        default:     return "Unknown";
    }
}

void printGPUList(const std::vector<GPUDevice>& gpus) {
    if (gpus.empty()) {
        printf("No GPUs detected (or enumeration not supported on this platform).\n");
        return;
    }
    printf("Available GPUs:\n");
    for (const auto& g : gpus) {
        printf("  %d: %s [%s @ %s]%s\n",
               g.index, g.name.c_str(), g.pci_id.c_str(),
               g.pci_slot.empty() ? "?" : g.pci_slot.c_str(),
               g.is_integrated ? " (integrated)" : "");
    }
}
