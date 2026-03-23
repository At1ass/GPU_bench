#pragma once

#include "platform/compat.h"
#include <cstdio>
#include <cstdarg>
#include <chrono>
#if !defined(_WIN32)
#include <unistd.h>
#endif

// Logger with timestamps, ANSI colors, and automatic subsystem detection.
//
// Two APIs:
//   Log::info("message", args...)     — classic printf-style (no source location)
//   LOG_INF("message", args...)       — macro with auto file:line + subsystem tag
//
// Output format:
//   [INF +1.234s] [renderer] gl3_renderer.cpp:42  message text
//
// Subsystem is extracted automatically from __FILE__ path:
//   src/renderer/gl3.cpp → "renderer"
//   src/demo/scene.cpp   → "demo"
//   src/main.cpp         → "app"

// Compile-time printf format checking (GCC/Clang)
#if defined(__GNUC__) || defined(__clang__)
#define LOG_PRINTF_ATTR(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define LOG_PRINTF_ATTR(fmt_idx, args_idx)
#endif

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
        s_start = std::chrono::steady_clock::now();
        s_use_color = detectColorSupport();
    }

    static void shutdown() {
        if (s_counts[0] || s_counts[1] || s_counts[2] || s_counts[3]) {
            fprintf(stderr, "[LOG] Messages: %u dbg, %u info, %u warn, %u err\n",
                    s_counts[0], s_counts[1], s_counts[2], s_counts[3]);
            if (s_file) {
                fprintf(s_file.get(), "[LOG] Messages: %u dbg, %u info, %u warn, %u err\n",
                        s_counts[0], s_counts[1], s_counts[2], s_counts[3]);
            }
        }
        s_file.reset();
    }

    static void setLevel(Level lvl) { s_level = lvl; }
    static bool levelEnabled(Level lvl) { return lvl >= s_level; }

    // --- Classic API (no source location) ---

    LOG_PRINTF_ATTR(1, 2)
    static void dbg(const char* fmt, ...) {
        if (s_level > Level::Debug) return;
        SubsystemTag none = { nullptr, 0 };
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Debug, nullptr, none, 0, fmt, ap);
        va_end(ap);
    }

    LOG_PRINTF_ATTR(1, 2)
    static void info(const char* fmt, ...) {
        if (s_level > Level::Info) return;
        SubsystemTag none = { nullptr, 0 };
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Info, nullptr, none, 0, fmt, ap);
        va_end(ap);
    }

    LOG_PRINTF_ATTR(1, 2)
    static void warn(const char* fmt, ...) {
        if (s_level > Level::Warn) return;
        SubsystemTag none = { nullptr, 0 };
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Warn, nullptr, none, 0, fmt, ap);
        va_end(ap);
    }

    LOG_PRINTF_ATTR(1, 2)
    static void err(const char* fmt, ...) {
        SubsystemTag none = { nullptr, 0 };
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Error, nullptr, none, 0, fmt, ap);
        va_end(ap);
    }

    // --- Location-aware API (called by LOG_* macros) ---

    LOG_PRINTF_ATTR(3, 4)
    static void dbg_loc(const char* file, int line, const char* fmt, ...) {
        if (s_level > Level::Debug) return;
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Debug, file, extractSubsystem(file), line, fmt, ap);
        va_end(ap);
    }

    LOG_PRINTF_ATTR(3, 4)
    static void info_loc(const char* file, int line, const char* fmt, ...) {
        if (s_level > Level::Info) return;
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Info, file, extractSubsystem(file), line, fmt, ap);
        va_end(ap);
    }

    LOG_PRINTF_ATTR(3, 4)
    static void warn_loc(const char* file, int line, const char* fmt, ...) {
        if (s_level > Level::Warn) return;
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Warn, file, extractSubsystem(file), line, fmt, ap);
        va_end(ap);
    }

    LOG_PRINTF_ATTR(3, 4)
    static void err_loc(const char* file, int line, const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        writeMsg(Level::Error, file, extractSubsystem(file), line, fmt, ap);
        va_end(ap);
    }

    // --- Subsystem extraction from __FILE__ ---
    // "src/renderer/gl3.cpp"         → "renderer"
    // "/full/path/src/demo/scene.cpp" → "demo"
    // "src/main.cpp"                 → "app"

    struct SubsystemTag {
        const char* name;
        int len;
    };

    static SubsystemTag extractSubsystem(const char* file) {
        SubsystemTag tag = { "app", 3 };
        if (!file) return tag;

        // Find last "src/" or "src\" anchor
        const char* after_src = nullptr;
        for (const char* p = file; *p; p++) {
            if (p[0] == 's' && p[1] == 'r' && p[2] == 'c' && (p[3] == '/' || p[3] == '\\'))
                after_src = p + 4;
        }
        if (!after_src) after_src = file;

        // Find first '/' or '\' after src/ — that's the end of subsystem name
        const char* sep = nullptr;
        for (const char* p = after_src; *p; p++) {
            if (*p == '/' || *p == '\\') { sep = p; break; }
        }

        if (sep && sep > after_src) {
            tag.name = after_src;
            tag.len = static_cast<int>(sep - after_src);
        }
        // else: file directly in src/ (main.cpp, app.cpp) → "app"
        return tag;
    }

    // Extract just the filename from a path
    static const char* extractFilename(const char* path) {
        const char* name = path;
        for (const char* p = path; *p; p++) {
            if (*p == '/' || *p == '\\') name = p + 1;
        }
        return name;
    }

private:
    static FileGuard s_file;
    static Level s_level;
    static std::chrono::steady_clock::time_point s_start;
    static bool s_use_color;
    static unsigned int s_counts[4]; // dbg, info, warn, err

    // Level metadata
    struct LevelInfo {
        const char* tag;    // "DBG", "INF", etc.
        const char* color;  // ANSI escape (empty if no color)
    };

    static LevelInfo levelInfo(Level lvl) {
        static const LevelInfo table[] = {
            { "DBG", "\033[90m"   },  // gray
            { "INF", "\033[32m"   },  // green
            { "WRN", "\033[33m"   },  // yellow
            { "ERR", "\033[31;1m" },  // bright red
        };
        return table[static_cast<int>(lvl)];
    }

    static bool detectColorSupport() {
#if defined(_WIN32)
        return false; // conservative — ANSI may not work on older Windows
#else
        return isatty(fileno(stderr)) != 0;
#endif
    }

    static double elapsedSec() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - s_start).count();
    }

    // Write one formatted line to a FILE* sink
    static void writeTo(FILE* out, const LevelInfo& li, double elapsed,
                        SubsystemTag sub, const char* fname, int line,
                        const char* msg, bool color) {
        const char* c = color ? li.color : "";
        const char* r = color ? "\033[0m"  : "";

        fprintf(out, "%s[%s %+8.3fs]%s ", c, li.tag, elapsed, r);
        if (sub.name)
            fprintf(out, "%s[%-9.*s]%s ", c, sub.len, sub.name, r);
        if (fname)
            fprintf(out, "%s%s:%d%s  ", c, fname, line, r);
        fputs(msg, out);
        fputc('\n', out);
    }

    // Format user message once, dispatch to all sinks
    static void writeMsg(Level lvl, const char* file, SubsystemTag sub,
                         int line, const char* fmt, va_list ap) {
        s_counts[static_cast<int>(lvl)]++;

        // Format user message once — no va_copy needed
        char msg[2048];
        vsnprintf(msg, sizeof(msg), fmt, ap);

        double elapsed = elapsedSec();
        LevelInfo li = levelInfo(lvl);
        const char* fname = (file && line > 0) ? extractFilename(file) : nullptr;

        writeTo(stderr, li, elapsed, sub, fname, line, msg, s_use_color);
        if (s_file) {
            writeTo(s_file.get(), li, elapsed, sub, fname, line, msg, false);
            fflush(s_file.get());
        }
    }
};

// --- Convenience macros with automatic source location + subsystem ---
// Usage: LOG_DBG("created mesh %u", handle);
// Output: [DBG +0.342s] [renderer] gl3_renderer.cpp:169  created mesh 5

#define LOG_DBG(fmt, ...) \
    do { if (Log::levelEnabled(Log::Level::Debug)) \
        Log::dbg_loc(__FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_INF(fmt, ...) \
    do { if (Log::levelEnabled(Log::Level::Info)) \
        Log::info_loc(__FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_WRN(fmt, ...) \
    do { if (Log::levelEnabled(Log::Level::Warn)) \
        Log::warn_loc(__FILE__, __LINE__, fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_ERR(fmt, ...) \
    Log::err_loc(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
