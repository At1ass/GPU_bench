#include "gpu_select.h"
#include "compat.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

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

// ---- Linux implementation ----

#if !defined(_WIN32)

static bool isLikelyIntegrated(unsigned int vendor_id, const char* vendor_name) {
    // Intel GPUs are almost always integrated
    if (vendor_id == 0x8086) return true;
    // AMD APUs — harder to detect from PCI alone, but if both AMD dGPU and
    // AMD iGPU exist, the one with smaller card index is usually the iGPU
    (void)vendor_name;
    return false;
}
#include <dirent.h>
#include <unistd.h>

// Look up device name from /usr/share/hwdata/pci.ids (standard on most distros)
static std::string lookupPciName(unsigned int vendor_id, unsigned int device_id) {
    static const char* pci_ids_paths[] = {
        "/usr/share/hwdata/pci.ids",
        "/usr/share/misc/pci.ids",
        "/usr/share/pci.ids",
        NULL
    };

    FILE* f = NULL;
    for (int i = 0; pci_ids_paths[i]; i++) {
        f = fopen(pci_ids_paths[i], "r");
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

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!in_vendor) {
            // Vendor line: starts with 4 hex digits at column 0
            if (strncmp(line, vendor_prefix, 4) == 0 && line[4] == ' ') {
                in_vendor = true;
            }
        } else {
            // Inside our vendor block
            if (line[0] != '\t') {
                // New vendor started — device not found
                break;
            }
            // Device line: starts with \tXXXX
            if (strncmp(line, device_prefix, 5) == 0 && (line[5] == ' ' || line[5] == '\t')) {
                // Extract device name after "\tXXXX  "
                const char* name_start = line + 5;
                while (*name_start == ' ' || *name_start == '\t') name_start++;
                result = name_start;
                while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                    result.pop_back();
                break;
            }
        }
    }

    fclose(f);
    return result;
}

static std::string readSysfsFile(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return "";
    char buf[256];
    std::string result;
    if (fgets(buf, sizeof(buf), f)) {
        result = buf;
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
    }
    fclose(f);
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
    while ((entry = readdir(dir)) != NULL) {
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
    for (size_t i = 1; i < gpus.size(); i++) {
        for (size_t j = i; j > 0 && gpus[j].index < gpus[j-1].index; j--) {
            GPUDevice tmp = gpus[j];
            gpus[j] = gpus[j-1];
            gpus[j-1] = tmp;
        }
    }

    // Heuristic: if we have AMD + AMD, the lower-index one is usually the iGPU
    int amd_count = 0;
    for (size_t i = 0; i < gpus.size(); i++) {
        if (gpus[i].vendor == "AMD") amd_count++;
    }
    if (amd_count >= 2) {
        bool found_first_amd = false;
        for (size_t i = 0; i < gpus.size(); i++) {
            if (gpus[i].vendor == "AMD") {
                if (!found_first_amd) {
                    gpus[i].is_integrated = true;
                    found_first_amd = true;
                }
            }
        }
    }

    return gpus;
}

bool selectGPU(int index) {
    // Enumerate to find vendor info for the target GPU
    std::vector<GPUDevice> gpus = enumerateGPUs();

    const GPUDevice* target = NULL;
    for (size_t i = 0; i < gpus.size(); i++) {
        if (gpus[i].index == index) {
            target = &gpus[i];
            break;
        }
    }

    if (!target) {
        fprintf(stderr, "GPU index %d not found.\n", index);
        return false;
    }

    // Determine which env vars to set based on driver situation.
    // Check if NVIDIA proprietary driver is in use by looking for its GLX library.
    bool has_nvidia_proprietary = false;
    FILE* f = fopen("/proc/driver/nvidia/version", "r");
    if (f) {
        has_nvidia_proprietary = true;
        fclose(f);
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
            // GLX path (X11):
            setenv("__GLX_VENDOR_LIBRARY_NAME", "mesa", 1);
            // EGL path (Wayland): force Mesa EGL vendor
            // Check common paths for Mesa EGL vendor JSON
            static const char* mesa_egl_paths[] = {
                "/usr/share/glvnd/egl_vendor.d/50_mesa.json",
                "/usr/share/egl/egl_external_platform.d/50_mesa.json",
                NULL
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
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, NULL, NULL);
    return result;
}

std::vector<GPUDevice> enumerateGPUs() {
    std::vector<GPUDevice> gpus;

    // Load DXGI dynamically — not available on Windows XP
    HMODULE dxgi_dll = LoadLibraryA("dxgi.dll");
    if (!dxgi_dll) {
        // Windows XP or very old system — no DXGI
        fprintf(stderr, "Note: DXGI not available (Windows XP?), GPU enumeration skipped.\n");
        return gpus;
    }

    PFN_CreateDXGIFactory createFactory =
        reinterpret_cast<PFN_CreateDXGIFactory>(reinterpret_cast<void*>(GetProcAddress(dxgi_dll, "CreateDXGIFactory")));
    if (!createFactory) {
        FreeLibrary(dxgi_dll);
        return gpus;
    }

    CB_IDXGIFactory* factory = NULL;
    HRESULT hr = createFactory(CB_IID_IDXGIFactory, (void**)&factory);
    if (hr != S_OK || !factory) {
        FreeLibrary(dxgi_dll);
        return gpus;
    }

    for (UINT i = 0; ; i++) {
        CB_IDXGIAdapter* adapter = NULL;
        hr = factory->lpVtbl->EnumAdapters(factory, i, &adapter);
        if (hr != S_OK || !adapter) break;

        CB_DXGI_ADAPTER_DESC desc;
        memset(&desc, 0, sizeof(desc));
        hr = adapter->lpVtbl->GetDesc(adapter, &desc);

        if (hr == S_OK) {
            GPUDevice gpu;
            gpu.index = (int)i;
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
    std::vector<GPUDevice> gpus = enumerateGPUs();

    const GPUDevice* target = NULL;
    for (size_t i = 0; i < gpus.size(); i++) {
        if (gpus[i].index == index) {
            target = &gpus[i];
            break;
        }
    }

    if (!target) {
        fprintf(stderr, "GPU index %d not found.\n", index);
        return false;
    }

    // On Windows, GPU selection for OpenGL is limited to discrete/integrated hint
    // via the exported NvOptimusEnablement / AmdPowerXpressRequestHighPerformance symbols.
    // These are read by the driver at process start and cannot be changed at runtime.
    // We can only warn the user if their selection doesn't match the default.
    if (target->is_integrated) {
        fprintf(stderr, "Note: Requesting integrated GPU.\n"
                        "This requires setting the GPU preference in NVIDIA Control Panel\n"
                        "or Windows Settings > Display > Graphics for this application.\n"
                        "The exported symbols request the discrete GPU by default.\n");
        // Try to influence by zeroing the export (won't work after process start,
        // but documents the intent)
        NvOptimusEnablement = 0;
        AmdPowerXpressRequestHighPerformance = 0;
    } else {
        fprintf(stderr, "Discrete GPU requested (default behavior).\n");
    }

    return true;
}

#endif

void selectGPUAndReexec(int index, int argc, char* argv[]) {
#ifndef _WIN32
    (void)argc;
    // Check if we've already re-exec'd (avoid infinite loop)
    const char* marker = getenv("_GPU_BENCH_REEXEC");
    if (marker && atoi(marker) == index) {
        // Already re-exec'd for this GPU, proceed normally
        return;
    }

    // Set env vars via selectGPU
    if (!selectGPU(index)) return;

    // Mark that we've set env vars
    char mark[16];
    snprintf(mark, sizeof(mark), "%d", index);
    setenv("_GPU_BENCH_REEXEC", mark, 1);

    // Re-exec to ensure GLVND picks up env vars before library loading
    fprintf(stderr, "Re-launching for GPU %d...\n", index);

    // Get the actual executable path
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        exe_path[len] = '\0';
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
        fprintf(stderr, "No GPUs detected (or enumeration not supported on this platform).\n");
        return;
    }
    fprintf(stderr, "Available GPUs:\n");
    for (size_t i = 0; i < gpus.size(); i++) {
        const GPUDevice& g = gpus[i];
        fprintf(stderr, "  %d: %s [%s @ %s]%s\n",
                g.index, g.name.c_str(), g.pci_id.c_str(),
                g.pci_slot.empty() ? "?" : g.pci_slot.c_str(),
                g.is_integrated ? " (integrated)" : "");
    }
}
