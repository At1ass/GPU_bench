// Platform: macOS, Android, FreeBSD — GPU selection not supported
#include "platform/gpu_select.h"
#include "platform/logger.h"

std::vector<GPUDevice> enumerateGPUs() {
    return std::vector<GPUDevice>();
}

bool selectGPU(int index) {
    (void)index;
    return false;
}

void selectGPUAndReexec(int index, int argc, char* argv[]) {
    (void)index; (void)argc; (void)argv;
    LOG_WRN("GPU selection is not supported on this platform");
}
