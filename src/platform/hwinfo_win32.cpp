// Platform: Windows — HWInfo::detect()
#include "platform/hwinfo.h"
#include "platform/logger.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>

static std::string readRegistryString(HKEY root, const char* path, const char* value) {
    HKEY key;
    if (RegOpenKeyExA(root, path, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return "";
    char buf[256];
    DWORD size = sizeof(buf);
    DWORD type = 0;
    std::string result;
    if (RegQueryValueExA(key, value, 0, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS && type == REG_SZ)
        result = buf;
    RegCloseKey(key);
    return result;
}

HWInfo HWInfo::detect() {
    HWInfo info;
    info.cpu_name = readRegistryString(
        HKEY_LOCAL_MACHINE,
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        "ProcessorNameString"
    );
    if (info.cpu_name.empty()) info.cpu_name = "Unknown CPU";
    info.cpu_name = trimHWInfoWhitespace(info.cpu_name);

    OSVERSIONINFOA osvi;
    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    bool got_version = false;
    typedef LONG (WINAPI *RtlGetVersionFunc)(OSVERSIONINFOA*);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        RtlGetVersionFunc rtlGetVersion =
            reinterpret_cast<RtlGetVersionFunc>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        if (rtlGetVersion && rtlGetVersion(&osvi) == 0) {
            got_version = true;
        }
    }
    if (!got_version) {
#ifdef _MSC_VER
        #pragma warning(suppress: 4996)
#endif
        GetVersionExA(&osvi);
    }

    DWORD maj = osvi.dwMajorVersion, min = osvi.dwMinorVersion;
    const char* win_name = "Windows";
    if      (maj == 5 && min == 0) win_name = "Windows 2000";
    else if (maj == 5 && min == 1) win_name = "Windows XP";
    else if (maj == 5 && min == 2) win_name = "Windows XP x64";
    else if (maj == 6 && min == 0) win_name = "Windows Vista";
    else if (maj == 6 && min == 1) win_name = "Windows 7";
    else if (maj == 6 && min == 2) win_name = "Windows 8";
    else if (maj == 6 && min == 3) win_name = "Windows 8.1";
    else if (maj == 10)            win_name = (osvi.dwBuildNumber >= 22000) ? "Windows 11" : "Windows 10";

    info.os_name = win_name;
    char ver[64];
    snprintf(ver, sizeof(ver), "(%lu.%lu build %lu)", maj, min, osvi.dwBuildNumber);
    info.os_version = ver;
    LOG_DBG("HWInfo: CPU '%s'", info.cpu_name.c_str());
    LOG_DBG("HWInfo: OS '%s %s'", info.os_name.c_str(), info.os_version.c_str());
    return info;
}
