#pragma once

// MSVC < 2015 (version 1900) does not have C99 snprintf
#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

// strcasecmp is POSIX, Windows uses _stricmp
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

// RAII wrappers for C FILE handles.
// Usage: FileGuard f(fopen("path", "r"));   // auto-closed on scope exit
//        PipeGuard p(popen("cmd", "r"));     // auto-pclosed on scope exit
#include <cstdio>
#include <memory>

struct FCloseDeleter { void operator()(FILE* f) const { if (f) fclose(f); } };
using FileGuard = std::unique_ptr<FILE, FCloseDeleter>;

#ifndef _WIN32
struct PCloseDeleter { void operator()(FILE* f) const { if (f) pclose(f); } };
using PipeGuard = std::unique_ptr<FILE, PCloseDeleter>;
#endif
