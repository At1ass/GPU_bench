#pragma once

#include "platform/compat.h"
#include <cstdio>
#include <cstdarg>

// Simple logger: writes to stderr and optionally to a file.
// Usage:
//   Log::init("benchmark.log");  // optional, stderr-only if omitted
//   Log::info("Loaded %d textures", count);
//   Log::warn("Missing feature: %s", name);
//   Log::err("Fatal: %s", msg);
//   Log::dbg("Verbose detail: x=%d", x);  // only if LOG_DEBUG defined

class Log {
public:
    Log() = delete;  // Pure static class, no instances

    enum class Level { Debug, Info, Warn, Error };

    static void init(const char* log_file = nullptr) {
        if (log_file) {
            s_file.reset(fopen(log_file, "w"));
            if (!s_file)
                fprintf(stderr, "[LOG] Could not open '%s' for writing\n", log_file);
        }
        s_level = Level::Debug;
    }

    static void shutdown() {
        s_file.reset();
    }

    static void setLevel(Level lvl) { s_level = lvl; }

    static void dbg(const char* fmt, ...) {
#ifdef LOG_DEBUG
        if (s_level > Level::Debug) return;
        va_list ap; va_start(ap, fmt);
        write("[DBG] ", fmt, ap);
        va_end(ap);
#else
        (void)fmt;
#endif
    }

    static void info(const char* fmt, ...) {
        if (s_level > Level::Info) return;
        va_list ap; va_start(ap, fmt);
        write("[INF] ", fmt, ap);
        va_end(ap);
    }

    static void warn(const char* fmt, ...) {
        if (s_level > Level::Warn) return;
        va_list ap; va_start(ap, fmt);
        write("[WRN] ", fmt, ap);
        va_end(ap);
    }

    static void err(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        write("[ERR] ", fmt, ap);
        va_end(ap);
    }

private:
    static FileGuard s_file;
    static Level s_level;

    static void write(const char* prefix, const char* fmt, va_list ap) {
        // stderr
        fputs(prefix, stderr);
        va_list ap2;
        va_copy(ap2, ap);
        vfprintf(stderr, fmt, ap2);
        va_end(ap2);
        fputc('\n', stderr);

        // file
        if (s_file) {
            fputs(prefix, s_file.get());
            va_list ap3;
            va_copy(ap3, ap);
            vfprintf(s_file.get(), fmt, ap3);
            va_end(ap3);
            fputc('\n', s_file.get());
            fflush(s_file.get());
        }
    }
};
