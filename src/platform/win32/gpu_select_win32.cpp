// Platform: Windows — GPU enumeration via DXGI, selection via exported symbols
#include "platform/gpu_select.h"
#include "platform/logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

// Export symbols to hint the driver which GPU to use (NVIDIA Optimus / AMD PowerXpress)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// DXGI minimal definitions (avoids requiring dxgi.h)
typedef struct {
    WCHAR Description[128];
    UINT VendorId, DeviceId, SubSysId, Revision;
    SIZE_T DedicatedVideoMemory, DedicatedSystemMemory, SharedSystemMemory;
    LUID AdapterLuid;
} CB_DXGI_ADAPTER_DESC;

static const GUID CB_IID_IDXGIFactory = {
    0x7b7166ec, 0x21c7, 0x44ae, {0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69}
};

struct CB_IDXGIAdapter;
struct CB_IDXGIFactoryVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void*);
    ULONG   (STDMETHODCALLTYPE *Release)(void*);
    void* SetPrivateData; void* SetPrivateDataInterface; void* GetPrivateData; void* GetParent;
    HRESULT (STDMETHODCALLTYPE *EnumAdapters)(void*, UINT, CB_IDXGIAdapter**);
};
struct CB_IDXGIFactory { CB_IDXGIFactoryVtbl* lpVtbl; };
struct CB_IDXGIAdapterVtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(void*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(void*);
    ULONG   (STDMETHODCALLTYPE *Release)(void*);
    void* SetPrivateData; void* SetPrivateDataInterface; void* GetPrivateData; void* GetParent;
    void* EnumOutputs;
    HRESULT (STDMETHODCALLTYPE *GetDesc)(void*, CB_DXGI_ADAPTER_DESC*);
    void* CheckInterfaceSupport;
};
struct CB_IDXGIAdapter { CB_IDXGIAdapterVtbl* lpVtbl; };
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

    HMODULE dxgi_dll = LoadLibraryA("dxgi.dll");
    if (!dxgi_dll) {
        // Windows XP fallback: EnumDisplayDevices
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
    if (!createFactory) { FreeLibrary(dxgi_dll); return gpus; }

    CB_IDXGIFactory* factory = nullptr;
    HRESULT hr = createFactory(CB_IID_IDXGIFactory, (void**)&factory);
    if (hr != S_OK || !factory) { FreeLibrary(dxgi_dll); return gpus; }

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
            char pci_buf[32];
            snprintf(pci_buf, sizeof(pci_buf), "%04x:%04x", desc.VendorId, desc.DeviceId);
            gpu.pci_id = pci_buf;
            gpu.is_integrated = (desc.DedicatedVideoMemory < 512ULL * 1024 * 1024);
            gpu.name = wcharToUtf8(desc.Description);
            if (gpu.name.empty()) gpu.name = gpu.vendor + " GPU";
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
    for (const auto& gpu : gpus) { if (gpu.index == index) { target = &gpu; break; } }
    if (!target) { LOG_ERR("GPU index %d not found", index); return false; }

    LOG_DBG("GPU: selected device %d: '%s'", target->index, target->name.c_str());
    if (target->is_integrated) {
        LOG_WRN("Requesting integrated GPU. Set GPU preference in driver control panel.");
        NvOptimusEnablement = 0;
        AmdPowerXpressRequestHighPerformance = 0;
    } else {
        LOG_INF("Discrete GPU requested (default behavior)");
    }
    return true;
}

void selectGPUAndReexec(int index, int argc, char* argv[]) {
    (void)argc; (void)argv;
    // On Windows, GPU selection is handled by exported symbols.
    // Re-exec is not needed.
    selectGPU(index);
}
