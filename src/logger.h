#pragma once

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
    enum Level { LVL_DEBUG, LVL_INFO, LVL_WARN, LVL_ERROR };

    static void init(const char* log_file = 0) {
        if (log_file) {
            s_file = fopen(log_file, "w");
            if (!s_file)
                fprintf(stderr, "[LOG] Could not open '%s' for writing\n", log_file);
        }
        s_level = LVL_DEBUG;
    }

    static void shutdown() {
        if (s_file) { fclose(s_file); s_file = 0; }
    }

    static void setLevel(Level lvl) { s_level = lvl; }

    static void dbg(const char* fmt, ...) {
#ifdef LOG_DEBUG
        if (s_level > LVL_DEBUG) return;
        va_list ap; va_start(ap, fmt);
        write("[DBG] ", fmt, ap);
        va_end(ap);
#else
        (void)fmt;
#endif
    }

    static void info(const char* fmt, ...) {
        if (s_level > LVL_INFO) return;
        va_list ap; va_start(ap, fmt);
        write("[INF] ", fmt, ap);
        va_end(ap);
    }

    static void warn(const char* fmt, ...) {
        if (s_level > LVL_WARN) return;
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
    static FILE* s_file;
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
            fputs(prefix, s_file);
            va_list ap3;
            va_copy(ap3, ap);
            vfprintf(s_file, fmt, ap3);
            va_end(ap3);
            fputc('\n', s_file);
            fflush(s_file);
        }
    }
};
