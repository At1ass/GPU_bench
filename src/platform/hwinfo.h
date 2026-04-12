#pragma once
#include <string>

struct HWInfo {
    std::string cpu_name;
    std::string os_name;
    std::string os_version;

    HWInfo() = default;
    HWInfo(const HWInfo&) = default;
    HWInfo& operator=(const HWInfo&) = default;
    HWInfo(HWInfo&&) noexcept = default;
    HWInfo& operator=(HWInfo&&) noexcept = default;

    static HWInfo detect();
};

// Shared utility for platform hwinfo implementations
std::string trimHWInfoWhitespace(const std::string& s);
