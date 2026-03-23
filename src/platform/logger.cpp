#include "platform/logger.h"

FileGuard Log::s_file;
Log::Level Log::s_level = Log::Level::Info;
std::chrono::steady_clock::time_point Log::s_start = std::chrono::steady_clock::now();
bool Log::s_use_color = false;
unsigned int Log::s_counts[4] = {0, 0, 0, 0};
