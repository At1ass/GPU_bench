#pragma once
#include <string>

struct HWInfo {
    std::string cpu_name;
    std::string os_name;
    std::string os_version;

    static HWInfo detect();
};
