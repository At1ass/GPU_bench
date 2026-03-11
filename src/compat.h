#pragma once

// MSVC < 2015 (version 1900) does not have C99 snprintf
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

// strcasecmp is POSIX, Windows uses _stricmp
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
