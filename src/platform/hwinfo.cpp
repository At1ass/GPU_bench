// Common utilities for HWInfo. Platform-specific detect() is in hwinfo_*.cpp.
#include "platform/hwinfo.h"
#include <cstring>

std::string trimHWInfoWhitespace(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
