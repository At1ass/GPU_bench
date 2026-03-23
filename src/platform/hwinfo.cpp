#include "platform/hwinfo.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <cstdio>
#include <cstring>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <sys/utsname.h>
#endif

static std::string trimWhitespace(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

#ifdef _WIN32

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
    info.cpu_name = trimWhitespace(info.cpu_name);

    // Try RtlGetVersion first (accurate on Win 8.1+), fall back to GetVersionExA
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

    // Map NT version to marketing name
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
    snprintf(ver, sizeof(ver), "(%lu.%lu build %lu)",
             maj, min, osvi.dwBuildNumber);
    info.os_version = ver;
    Log::dbg("HWInfo: CPU '%s'", info.cpu_name.c_str());
    Log::dbg("HWInfo: OS '%s %s'", info.os_name.c_str(), info.os_version.c_str());
    return info;
}

#else // Linux / POSIX

static std::string readFirstLine(const char* cmd) {
    PipeGuard fp(popen(cmd, "r"));
    if (!fp) return "";
    char buf[256];
    std::string result;
    if (fgets(buf, sizeof(buf), fp.get())) {
        result = buf;
        if (!result.empty() && result.back() == '\n')
            result.pop_back();
    }
    return result;
}

HWInfo HWInfo::detect() {
    HWInfo info;

    // CPU name — platform-specific
#if defined(__APPLE__)
    info.cpu_name = readFirstLine("sysctl -n machdep.cpu.brand_string");
#elif defined(__FreeBSD__)
    info.cpu_name = readFirstLine("sysctl -n hw.model");
#else
    info.cpu_name = readFirstLine(
        "grep 'model name' /proc/cpuinfo | head -n1 | cut -d: -f2"
    );
#endif
    info.cpu_name = trimWhitespace(info.cpu_name);
    if (info.cpu_name.empty()) info.cpu_name = "Unknown CPU";

    // OS info: try /etc/os-release for distro name, fall back to uname
    struct utsname u;
    bool have_uname = (uname(&u) == 0);
    std::string kernel_version = have_uname ? u.release : "unknown";

#if defined(__APPLE__)
    info.os_name = readFirstLine("sw_vers -productName");
    info.os_version = readFirstLine("sw_vers -productVersion");
    if (info.os_name.empty()) info.os_name = "macOS";
    info.os_version += " (" + kernel_version + ")";
#elif defined(__FreeBSD__)
    info.os_name = have_uname ? u.sysname : "FreeBSD";
    info.os_version = kernel_version;
#else
    // Linux: parse /etc/os-release for PRETTY_NAME
    info.os_name = "";
    {
        FileGuard f(fopen("/etc/os-release", "r"));
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f.get())) {
                if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                    char* val = line + 12;
                    // Strip quotes and newline
                    if (*val == '"') val++;
                    info.os_name = val;
                    while (!info.os_name.empty() &&
                           (info.os_name.back() == '\n' || info.os_name.back() == '\r' || info.os_name.back() == '"'))
                        info.os_name.pop_back();
                    break;
                }
            }
        }
    }
    if (info.os_name.empty())
        info.os_name = have_uname ? u.sysname : "Linux";
    info.os_version = kernel_version;
#endif
    Log::dbg("HWInfo: CPU '%s'", info.cpu_name.c_str());
    Log::dbg("HWInfo: OS '%s %s'", info.os_name.c_str(), info.os_version.c_str());
    return info;
}

#endif
