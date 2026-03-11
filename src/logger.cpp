#include "logger.h"

FILE* Log::s_file = 0;
Log::Level Log::s_level = Log::Level::Debug;
