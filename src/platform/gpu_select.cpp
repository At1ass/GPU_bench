#include "platform/gpu_select.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#ifdef _WIN32
// On Windows, export symbols to hint the driver which GPU to use.
// These are checked by NVIDIA Optimus and AMD PowerXpress drivers.
// By default, request the high-performance (discrete) GPU.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

// ---- PCI vendor ID to name mapping ----

static const char* pciVendorName(unsigned int vendor_id) {
    switch (vendor_id) {
        case 0x10de: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x1a03: return "ASPEED";
        case 0x1234: return "QEMU/Bochs";
        default:     return "Unknown";
    }
}

// ---- POSIX implementation (Linux / macOS / FreeBSD) ----

#if !defined(_WIN32)

#include <unistd.h>

#if defined(__APPLE__)
// macOS: GPU enumeration not supported (system manages GPU automatically)

std::vector<GPUDevice> enumerateGPUs() {
    return std::vector<GPUDevice>();
}

bool selectGPU(int index) {
    (void)index;
    fprintf(stderr, "GPU selection is not supported on macOS.\n");
    return false;
}

#else // Linux / FreeBSD

static bool isLikelyIntegrated(unsigned int vendor_id, const char* vendor_name) {
    // Intel GPUs are almost always integrated
    if (vendor_id == 0x8086) return true;
    // AMD APUs — harder to detect from PCI alone, but if both AMD dGPU and
    // AMD iGPU exist, the one with smaller card index is usually the iGPU
    (void)vendor_name;
    return false;
}
#include <dirent.h>

// Look up device name from /usr/share/hwdata/pci.ids (standard on most distros)
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
            if (strncmp(line, vendor_prefix, 4) == 0 && line[4] == ' ') {
                in_vendor = true;
            }
        } else {
            if (line[0] != '\t') {
                break;
            }
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

#ifdef __linux__

std::vector<GPUDevice> enumerateGPUs() {
    std::vector<GPUDevice> gpus;
    DIR* dir = opendir("/sys/class/drm");
    if (!dir) return gpus;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Match "card0", "card1", etc. Skip "card0-DP-1" etc.
        if (strncmp(entry->d_name, "card", 4) != 0) continue;
        const char* numpart = entry->d_name + 4;
        // Must be just a number after "card"
        if (*numpart < '0' || *numpart > '9') continue;
        if (strchr(numpart, '-')) continue;

        int card_idx = atoi(numpart);

        char path[512];

        // Read PCI vendor/device
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/vendor", entry->d_name);
        std::string vendor_str = readSysfsFile(path);
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/device", entry->d_name);
        std::string device_str = readSysfsFile(path);

        if (vendor_str.empty()) continue; // Not a real GPU

        unsigned int vendor_id = parseHex(vendor_str.c_str());
        unsigned int device_id = parseHex(device_str.c_str());

        GPUDevice gpu;
        gpu.index = card_idx;
        gpu.vendor = pciVendorName(vendor_id);
        gpu.is_integrated = isLikelyIntegrated(vendor_id, gpu.vendor.c_str());

        char pci_buf[32];
        snprintf(pci_buf, sizeof(pci_buf), "%04x:%04x", vendor_id, device_id);
        gpu.pci_id = pci_buf;

        // Read PCI slot address (e.g. "0000:11:00.0") from symlink target
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device", entry->d_name);
        char link_target[512];
        ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
        if (len > 0) {
            link_target[len] = '\0';
            // Extract last component: "../../../0000:11:00.0" → "0000:11:00.0"
            const char* last_slash = strrchr(link_target, '/');
            gpu.pci_slot = last_slash ? (last_slash + 1) : link_target;
        }

        // Try to get a human-readable name
        // Method 1: /sys/class/drm/cardN/device/label (sometimes available)
        snprintf(path, sizeof(path), "/sys/class/drm/%s/device/label", entry->d_name);
        gpu.name = readSysfsFile(path);

        // Method 2: Look up in pci.ids database
        if (gpu.name.empty()) {
            gpu.name = lookupPciName(vendor_id, device_id);
        }

        // Fallback name
        if (gpu.name.empty()) {
            char fallback[128];
            snprintf(fallback, sizeof(fallback), "%s GPU [%s]", gpu.vendor.c_str(), pci_buf);
            gpu.name = fallback;
        }

        gpus.push_back(gpu);
    }
    closedir(dir);

    // Sort by card index
    std::sort(gpus.begin(), gpus.end(), [](const GPUDevice& a, const GPUDevice& b) {
        return a.index < b.index;
    });

    // Heuristic: if we have AMD + AMD, the lower-index one is usually the iGPU
    int amd_count = 0;
    for (const auto& gpu : gpus) {
        if (gpu.vendor == "AMD") amd_count++;
    }
    if (amd_count >= 2) {
        bool found_first_amd = false;
        for (auto& gpu : gpus) {
            if (gpu.vendor == "AMD") {
                if (!found_first_amd) {
                    gpu.is_integrated = true;
                    found_first_amd = true;
                }
            }
        }
    }

    return gpus;
}

#else // FreeBSD — no sysfs, GPU enumeration not yet implemented

std::vector<GPUDevice> enumerateGPUs() {
    return std::vector<GPUDevice>();
}

#endif // __linux__

#ifdef __linux__

bool selectGPU(int index) {
    // Enumerate to find vendor info for the target GPU
    auto gpus = enumerateGPUs();

    const GPUDevice* target = nullptr;
    for (const auto& gpu : gpus) {
        if (gpu.index == index) {
            target = &gpu;
            break;
        }
    }

    if (!target) {
        LOG_ERR("GPU index %d not found", index);
        return false;
    }

    LOG_DBG("GPU: selected device %d: '%s'", target->index, target->name.c_str());

    // Determine which env vars to set based on driver situation.
    // Check if NVIDIA proprietary driver is in use by looking for its GLX library.
    bool has_nvidia_proprietary = false;
    {
        FileGuard f(fopen("/proc/driver/nvidia/version", "r"));
        has_nvidia_proprietary = (f != nullptr);
    }

    // Build DRI_PRIME value from PCI slot
    std::string prime_id;
    if (!target->pci_slot.empty()) {
        prime_id = "pci-" + target->pci_slot;
        for (size_t j = 0; j < prime_id.size(); j++) {
            if (prime_id[j] == ':' || prime_id[j] == '.')
                prime_id[j] = '_';
        }
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", index);
        prime_id = buf;
    }

    if (has_nvidia_proprietary) {
        if (target->vendor == "NVIDIA") {
            // User wants NVIDIA GPU
            setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 1);
            unsetenv("DRI_PRIME");
            unsetenv("__EGL_VENDOR_LIBRARY_FILENAMES");
        } else {
            // User wants non-NVIDIA GPU (e.g. AMD iGPU) — force Mesa
            LOG_WRN("Selecting a non-NVIDIA GPU while NVIDIA proprietary driver "
                "controls the display. Rendering will work but the window may be black "
                "because DRI_PRIME buffer sharing doesn't work across NVIDIA/Mesa. "
                "Use --headless for benchmark results without display.");
            // GLX path (X11):
            setenv("__GLX_VENDOR_LIBRARY_NAME", "mesa", 1);
            // EGL path (Wayland): force Mesa EGL vendor
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
        // Pure Mesa system — DRI_PRIME is sufficient
        setenv("DRI_PRIME", prime_id.c_str(), 1);
    }

    return true;
}

#elif defined(__FreeBSD__)

bool selectGPU(int index) {
    // FreeBSD supports DRI_PRIME via Mesa but has no GPU enumeration yet
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", index);
    setenv("DRI_PRIME", buf, 1);
    return true;
}

#endif // __linux__ / __FreeBSD__

#endif // !__APPLE__ (Linux / FreeBSD)

#else // Windows

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// DXGI definitions — minimal subset to avoid requiring dxgi.h
// (which may not be available on older MinGW / XP-era SDKs)
typedef struct {
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
} CB_DXGI_ADAPTER_DESC;

// DXGI GUIDs
static const GUID CB_IID_IDXGIFactory = {
    0x7b7166ec, 0x21c7, 0x44ae, {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}
};

// Minimal COM vtable layout for IDXGIFactory and IDXGIAdapter
// We use raw vtable pointers to avoid requiring dxgi.h headers

struct CB_IDXGIAdapter;

struct CB_IDXGIFactoryVtbl {
    // IUnknown
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void*);
    ULONG   (STDMETHODCALLTYPE *Release)(void*);
    // IDXGIObject (4 methods: SetPrivateData, SetPrivateDataInterface, GetPrivateData, GetParent)
    void* SetPrivateData;
    void* SetPrivateDataInterface;
    void* GetPrivateData;
    void* GetParent;
    // IDXGIFactory
    HRESULT (STDMETHODCALLTYPE *EnumAdapters)(void*, UINT, CB_IDXGIAdapter**);
};

struct CB_IDXGIFactory {
    CB_IDXGIFactoryVtbl* lpVtbl;
};

struct CB_IDXGIAdapterVtbl {
    // IUnknown
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void*);
    ULONG   (STDMETHODCALLTYPE *Release)(void*);
    // IDXGIObject
    void* SetPrivateData;
    void* SetPrivateDataInterface;
    void* GetPrivateData;
    void* GetParent;
    // IDXGIAdapter
    void* EnumOutputs;
    HRESULT (STDMETHODCALLTYPE *GetDesc)(void*, CB_DXGI_ADAPTER_DESC*);
    void* CheckInterfaceSupport;
};

struct CB_IDXGIAdapter {
    CB_IDXGIAdapterVtbl* lpVtbl;
};

// Type for CreateDXGIFactory
typedef HRESULT (WINAPI *PFN_CreateDXGIFactory)(REFIID, void**);

static std::string wcharToUtf8(const WCHAR* wstr) {
    if (!wstr || !wstr[0]) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::vector<GPUDevice> enumerateGPUs() {
    std::vector<GPUDevice> gpus;

    // Load DXGI dynamically — not available on Windows XP
    HMODULE dxgi_dll = LoadLibraryA("dxgi.dll");
    if (!dxgi_dll) {
        // Windows XP fallback: EnumDisplayDevices (user32.dll, Win2000+)
        DISPLAY_DEVICEA dd;
        dd.cb = sizeof(dd);
        for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0); i++) {
            if (dd.DeviceString[0] == '\0') continue;
            if (dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) continue;

            GPUDevice gpu;
            gpu.index = static_cast<int>(i);
            gpu.name = dd.DeviceString;
            gpu.is_integrated = false;
            gpus.push_back(gpu);

            dd.cb = sizeof(dd);
        }
        return gpus;
    }

    PFN_CreateDXGIFactory createFactory =
        reinterpret_cast<PFN_CreateDXGIFactory>(reinterpret_cast<void*>(GetProcAddress(dxgi_dll, "CreateDXGIFactory")));
    if (!createFactory) {
        FreeLibrary(dxgi_dll);
        return gpus;
    }

    CB_IDXGIFactory* factory = nullptr;
    HRESULT hr = createFactory(CB_IID_IDXGIFactory, (void**)&factory);
    if (hr != S_OK || !factory) {
        FreeLibrary(dxgi_dll);
        return gpus;
    }

    for (UINT i = 0; ; i++) {
        CB_IDXGIAdapter* adapter = nullptr;
        hr = factory->lpVtbl->EnumAdapters(factory, i, &adapter);
        if (hr != S_OK || !adapter) break;

        CB_DXGI_ADAPTER_DESC desc;
        memset(&desc, 0, sizeof(desc));
        hr = adapter->lpVtbl->GetDesc(adapter, &desc);

        if (hr == S_OK) {
            GPUDevice gpu;
            gpu.index = static_cast<int>(i);
            gpu.vendor = pciVendorName(desc.VendorId);

            // DXGI_ADAPTER_DESC.Description is WCHAR[128]
            // Access it as the memory right after the vtable desc fields
            // Actually, we defined CB_DXGI_ADAPTER_DESC without Description.
            // We need to include it. Let me use a different approach:
            // Read the Description field which is at the start of the real DXGI_ADAPTER_DESC

            // The real DXGI_ADAPTER_DESC layout is:
            // WCHAR Description[128] — 256 bytes
            // UINT VendorId, DeviceId, SubSysId, Revision — 16 bytes
            // SIZE_T DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory
            // LUID AdapterLuid
            // Our CB_DXGI_ADAPTER_DESC is missing Description. We need a proper struct.

            char pci_buf[32];
            snprintf(pci_buf, sizeof(pci_buf), "%04x:%04x", desc.VendorId, desc.DeviceId);
            gpu.pci_id = pci_buf;

            // Detect integrated by checking DedicatedVideoMemory
            // iGPUs typically have 0 or very small dedicated VRAM
            gpu.is_integrated = (desc.DedicatedVideoMemory < 512ULL * 1024 * 1024);

            // Convert WCHAR Description to UTF-8
            gpu.name = wcharToUtf8(desc.Description);
            if (gpu.name.empty())
                gpu.name = gpu.vendor + " GPU";

            gpus.push_back(gpu);
        }

        adapter->lpVtbl->Release(adapter);
    }

    factory->lpVtbl->Release(factory);
    FreeLibrary(dxgi_dll);
    return gpus;
}

bool selectGPU(int index) {
    auto gpus = enumerateGPUs();

    const GPUDevice* target = nullptr;
    for (const auto& gpu : gpus) {
        if (gpu.index == index) {
            target = &gpu;
            break;
        }
    }

    if (!target) {
        LOG_ERR("GPU index %d not found", index);
        return false;
    }

    LOG_DBG("GPU: selected device %d: '%s'", target->index, target->name.c_str());

    // On Windows, GPU selection for OpenGL is limited to discrete/integrated hint
    // via the exported NvOptimusEnablement / AmdPowerXpressRequestHighPerformance symbols.
    // These are read by the driver at process start and cannot be changed at runtime.
    // We can only warn the user if their selection doesn't match the default.
    if (target->is_integrated) {
        LOG_WRN("Requesting integrated GPU. "
                "This requires setting the GPU preference in NVIDIA Control Panel "
                "or Windows Settings > Display > Graphics for this application. "
                "The exported symbols request the discrete GPU by default.");
        // Try to influence by zeroing the export (won't work after process start,
        // but documents the intent)
        NvOptimusEnablement = 0;
        AmdPowerXpressRequestHighPerformance = 0;
    } else {
        LOG_INF("Discrete GPU requested (default behavior)");
    }

    return true;
}

#endif

void selectGPUAndReexec(int index, int argc, char* argv[]) {
#if defined(__APPLE__)
    // macOS: GPU selection not supported, no re-exec needed
    (void)argc; (void)argv;
    (void)index;
    LOG_WRN("GPU selection is not supported on macOS");
#elif !defined(_WIN32)
    (void)argc;
    // Check if we've already re-exec'd (avoid infinite loop)
    const char* marker = getenv("_GPU_BENCH_REEXEC");
    if (marker && atoi(marker) == index) {
        // Already re-exec'd for this GPU — log active env vars and proceed
        const char* dri = getenv("DRI_PRIME");
        const char* glx = getenv("__GLX_VENDOR_LIBRARY_NAME");
        LOG_INF("GPU %d selected (DRI_PRIME=%s, GLX=%s)",
                index, dri ? dri : "unset", glx ? glx : "unset");
        return;
    }

    // Set env vars via selectGPU
    if (!selectGPU(index)) return;

    // Mark that we've set env vars
    char mark[16];
    snprintf(mark, sizeof(mark), "%d", index);
    setenv("_GPU_BENCH_REEXEC", mark, 1);

    // Re-exec to ensure GLVND picks up env vars before library loading
    LOG_INF("Re-launching for GPU %d...", index);

    // Get the actual executable path
    char exe_path[4096];
    bool got_exe = false;

#if defined(__FreeBSD__)
    // FreeBSD: use sysctl KERN_PROC_PATHNAME
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
    size_t exe_len = sizeof(exe_path);
    if (sysctl(mib, 4, exe_path, &exe_len, NULL, 0) == 0) {
        got_exe = true;
    }
#else
    // Linux: /proc/self/exe
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
        got_exe = true;
    }
#endif

    if (got_exe) {
        execv(exe_path, argv);
    }
    // Fallback: try argv[0]
    execv(argv[0], argv);
    perror("execv failed");
#else
    (void)argc; (void)argv;
    // On Windows, GPU selection is handled by exported symbols
    // (NvOptimusEnablement / AmdPowerXpressRequestHighPerformance).
    // Re-exec is not needed, but we still call selectGPU for the user message.
    selectGPU(index);
#endif
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
