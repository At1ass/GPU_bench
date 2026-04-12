// Platform: Android — HWInfo::detect()
#include "platform/hwinfo.h"
#include "platform/compat.h"
#include "platform/logger.h"
#include <sys/system_properties.h>
#include <cstdio>
#include <cstring>

static std::string getAndroidProp(const char* name) {
    char value[PROP_VALUE_MAX] = {};
    __system_property_get(name, value);
    return std::string(value);
}

HWInfo HWInfo::detect() {
    HWInfo info;

    // CPU: try marketing name first, then chipname, then generic hardware
    info.cpu_name = getAndroidProp("ro.soc.model");
    if (info.cpu_name.empty())
        info.cpu_name = getAndroidProp("ro.hardware.chipname");
    if (info.cpu_name.empty())
        info.cpu_name = getAndroidProp("ro.hardware");
    if (info.cpu_name.empty()) {
        FileGuard f(fopen("/proc/cpuinfo", "r"));
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f.get())) {
                if (strncmp(line, "Hardware", 8) == 0 || strncmp(line, "model name", 10) == 0) {
                    const char* colon = strchr(line, ':');
                    if (colon) {
                        info.cpu_name = trimHWInfoWhitespace(std::string(colon + 1));
                        break;
                    }
                }
            }
        }
    }
    if (info.cpu_name.empty()) info.cpu_name = "Unknown CPU";

    // OS: Android system properties
    std::string release = getAndroidProp("ro.build.version.release");
    std::string sdk = getAndroidProp("ro.build.version.sdk");
    std::string device = getAndroidProp("ro.product.model");
    info.os_name = "Android";
    info.os_version = release + " (API " + sdk + ")";
    if (!device.empty()) info.os_version += " " + device;

    LOG_DBG("HWInfo: CPU '%s'", info.cpu_name.c_str());
    LOG_DBG("HWInfo: OS '%s %s'", info.os_name.c_str(), info.os_version.c_str());
    return info;
}
