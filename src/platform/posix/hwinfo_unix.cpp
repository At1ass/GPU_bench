// Platform: Linux, macOS, FreeBSD — HWInfo::detect()
#include "platform/hwinfo.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <sys/utsname.h>
#include <cstdio>
#include <cstring>

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
    // x86: "model name" in /proc/cpuinfo
    info.cpu_name = readFirstLine(
        "grep 'model name' /proc/cpuinfo | head -n1 | cut -d: -f2"
    );
    // ARM Linux: try lscpu, /sys/firmware/devicetree, or Hardware field
    if (info.cpu_name.empty())
        info.cpu_name = readFirstLine(
            "lscpu 2>/dev/null | grep 'Model name' | head -n1 | cut -d: -f2"
        );
    if (info.cpu_name.empty()) {
        FileGuard f(fopen("/sys/firmware/devicetree/base/model", "r"));
        if (f) {
            char buf[256];
            if (fgets(buf, sizeof(buf), f.get()))
                info.cpu_name = buf;
        }
    }
    if (info.cpu_name.empty())
        info.cpu_name = readFirstLine(
            "grep 'Hardware' /proc/cpuinfo | head -n1 | cut -d: -f2"
        );
#endif
    info.cpu_name = trimHWInfoWhitespace(info.cpu_name);
    if (info.cpu_name.empty()) info.cpu_name = "Unknown CPU";

    // OS info
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
    info.os_name = "";
    {
        FileGuard f(fopen("/etc/os-release", "r"));
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f.get())) {
                if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                    char* val = line + 12;
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
    LOG_DBG("HWInfo: CPU '%s'", info.cpu_name.c_str());
    LOG_DBG("HWInfo: OS '%s %s'", info.os_name.c_str(), info.os_version.c_str());
    return info;
}
