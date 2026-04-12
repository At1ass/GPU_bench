// Platform: Linux — GPU enumeration via sysfs, selection via DRI_PRIME
#include "platform/gpu_select.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <unistd.h>
#include <dirent.h>

static bool isLikelyIntegrated(unsigned int vendor_id, const char* vendor_name) {
    if (vendor_id == 0x8086) return true;
    (void)vendor_name;
    return false;
}

static std::string lookupPciName(unsigned int vendor_id, unsigned int device_id) {
    static const char* pci_ids_paths[] = {
        "/usr/share/hwdata/pci.ids",
        "/usr/share/misc/pci.ids",
        "/usr/share/pci.ids",
        nullptr
    };

    FileGuard f;
    for (int i = 0; pci_ids_paths[i]; i++) {
        f.reset(fopen(pci_ids_paths[i], "r"));
        if (f) break;
    }
    if (!f) return "";

    char vendor_prefix[8];
    char device_prefix[8];
    snprintf(vendor_prefix, sizeof(vendor_prefix), "%04x", vendor_id);
    snprintf(device_prefix, sizeof(device_prefix), "\t%04x", device_id);

    bool in_vendor = false;
    char line[512];
    std::string result;

    while (fgets(line, sizeof(line), f.get())) {
        if (line[0] == '#' || line[0] == '\n') continue;
        if (!in_vendor) {
            if (strncmp(line, vendor_prefix, 4) == 0 && line[4] == ' ')
                in_vendor = true;
        } else {
            if (line[0] != '\t') break;
            if (strncmp(line, device_prefix, 5) == 0 && (line[5] == ' ' || line[5] == '\t')) {
                const char* name_start = line + 5;
                while (*name_start == ' ' || *name_start == '\t') name_start++;
                result = name_start;
                while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                    result.pop_back();
                break;
            }
        }
    }
    return result;
}

static std::string readSysfsFile(const char* path) {
    FileGuard f(fopen(path, "r"));
    if (!f) return "";
    char buf[256];
    std::string result;
    if (fgets(buf, sizeof(buf), f.get())) {
        result = buf;
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
    }
    return result;
}

static unsigned int parseHex(const std::string& s) {
    unsigned int val = 0;
    sscanf(s.c_str(), "%x", &val);
    return val;
}

std::vector<GPUDevice> enumerateGPUs() {
    std::vector<GPUDevice> gpus;
    DIR* dir = opendir("/sys/class/drm");
    if (!dir) return gpus;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "card", 4) != 0) continue;
        const char* numpart = entry->d_name + 4;
        if (*numpart < '0' || *numpart > '9') continue;
        if (strchr(numpart, '-')) continue;

        int card_idx = atoi(numpart);
        char path[512];

        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", entry->d_name);
        std::string vendor_str = readSysfsFile(path);
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/device", entry->d_name);
        std::string device_str = readSysfsFile(path);

        if (vendor_str.empty()) continue;

        unsigned int vendor_id = parseHex(vendor_str.c_str());
        unsigned int device_id = parseHex(device_str.c_str());

        GPUDevice gpu;
        gpu.index = card_idx;
        gpu.vendor = pciVendorName(vendor_id);
        gpu.is_integrated = isLikelyIntegrated(vendor_id, gpu.vendor.c_str());

        char pci_buf[32];
        snprintf(pci_buf, sizeof(pci_buf), "%04x:%04x", vendor_id, device_id);
        gpu.pci_id = pci_buf;

        snprintf(path, sizeof(path), "/sys/class/drm/%s/device", entry->d_name);
        char link_target[512];
        ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
        if (len > 0) {
            link_target[len] = '\0';
            const char* last_slash = strrchr(link_target, '/');
            gpu.pci_slot = last_slash ? (last_slash + 1) : link_target;
        }

        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/label", entry->d_name);
        gpu.name = readSysfsFile(path);
        if (gpu.name.empty()) gpu.name = lookupPciName(vendor_id, device_id);
        if (gpu.name.empty()) {
            char fallback[128];
            snprintf(fallback, sizeof(fallback), "%s GPU [%s]", gpu.vendor.c_str(), pci_buf);
            gpu.name = fallback;
        }

        gpus.push_back(gpu);
    }
    closedir(dir);

    std::sort(gpus.begin(), gpus.end(), [](const GPUDevice& a, const GPUDevice& b) {
        return a.index < b.index;
    });

    // Heuristic: if AMD + AMD, lower index is usually iGPU
    int amd_count = 0;
    for (const auto& gpu : gpus) { if (gpu.vendor == "AMD") amd_count++; }
    if (amd_count >= 2) {
        bool found_first_amd = false;
        for (auto& gpu : gpus) {
            if (gpu.vendor == "AMD" && !found_first_amd) {
                gpu.is_integrated = true;
                found_first_amd = true;
            }
        }
    }

    return gpus;
}

bool selectGPU(int index) {
    auto gpus = enumerateGPUs();

    const GPUDevice* target = nullptr;
    for (const auto& gpu : gpus) {
        if (gpu.index == index) { target = &gpu; break; }
    }
    if (!target) { LOG_ERR("GPU index %d not found", index); return false; }

    LOG_DBG("GPU: selected device %d: '%s'", target->index, target->name.c_str());

    bool has_nvidia_proprietary = false;
    { FileGuard f(fopen("/proc/driver/nvidia/version", "r")); has_nvidia_proprietary = (f != nullptr); }

    std::string prime_id;
    if (!target->pci_slot.empty()) {
        prime_id = "pci-" + target->pci_slot;
        for (size_t j = 0; j < prime_id.size(); j++) {
            if (prime_id[j] == ':' || prime_id[j] == '.') prime_id[j] = '_';
        }
    } else {
        char buf[16]; snprintf(buf, sizeof(buf), "%d", index); prime_id = buf;
    }

    if (has_nvidia_proprietary) {
        if (target->vendor == "NVIDIA") {
            setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 1);
            unsetenv("DRI_PRIME");
            unsetenv("__EGL_VENDOR_LIBRARY_FILENAMES");
        } else {
            LOG_WRN("Selecting non-NVIDIA GPU with NVIDIA proprietary driver active");
            setenv("__GLX_VENDOR_LIBRARY_NAME", "mesa", 1);
            static const char* mesa_egl_paths[] = {
                "/usr/share/glvnd/egl_vendor.d/50_mesa.json",
                "/usr/share/egl/egl_external_platform.d/50_mesa.json",
                nullptr
            };
            for (int j = 0; mesa_egl_paths[j]; j++) {
                if (access(mesa_egl_paths[j], R_OK) == 0) {
                    setenv("__EGL_VENDOR_LIBRARY_FILENAMES", mesa_egl_paths[j], 1);
                    break;
                }
            }
            setenv("DRI_PRIME", prime_id.c_str(), 1);
        }
    } else {
        setenv("DRI_PRIME", prime_id.c_str(), 1);
    }

    return true;
}

void selectGPUAndReexec(int index, int argc, char* argv[]) {
    (void)argc;
    const char* marker = getenv("_GPU_BENCH_REEXEC");
    if (marker && atoi(marker) == index) {
        const char* dri = getenv("DRI_PRIME");
        const char* glx = getenv("__GLX_VENDOR_LIBRARY_NAME");
        LOG_INF("GPU %d selected (DRI_PRIME=%s, GLX=%s)",
                index, dri ? dri : "unset", glx ? glx : "unset");
        return;
    }

    if (!selectGPU(index)) return;

    char mark[16];
    snprintf(mark, sizeof(mark), "%d", index);
    setenv("_GPU_BENCH_REEXEC", mark, 1);

    LOG_INF("Re-launching for GPU %d...", index);

    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        execv(exe_path, argv);
    }
    execv(argv[0], argv);
    perror("execv failed");
}
