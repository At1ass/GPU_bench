#pragma once

#include "platform/compat.h"
#include <cstdio>
#include <chrono>
#if !defined(_WIN32)
#include <unistd.h>
#endif

// Logger with timestamps, ANSI colors, and automatic subsystem detection.
//
// Preferred API (full compile-time format checking + auto source location):
//   LOG_DBG("mesh %u created", handle);
//   LOG_INF("loaded %d textures", count);
//   LOG_WRN("feature %s unavailable", name);
//   LOG_ERR("shader compile failed: %s", log);
//
// Classic API (no source location, format checking via snprintf inlining):
//   Log::info("message");              // zero-args: no formatting
//   Log::info("value=%d", x);          // variadic: formatted via snprintf
//
// Output format:
//   [INF +1.234s] [renderer] gl3_renderer.cpp:42  message text

class Log {
public:
    Log() = delete;

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

    // --- Core sink: pre-formatted message, no formatting concerns ---

    static void emitMsg(Level lvl, const char* file, int line, const char* msg) {
        s_counts[static_cast<int>(lvl)]++;
        double elapsed = elapsedSec();
        LevelInfo li = levelInfo(lvl);
        SubsystemTag sub = file ? extractSubsystem(file) : SubsystemTag{ nullptr, 0 };
        const char* fname = (file && line > 0) ? extractFilename(file) : nullptr;

        writeTo(stderr, li, elapsed, sub, fname, line, msg, s_use_color);
        if (s_file) {
            writeTo(s_file.get(), li, elapsed, sub, fname, line, msg, false);
            fflush(s_file.get());
        }
    }

    // --- Classic API: zero-args overloads (no snprintf, no warnings) ---

    static void dbg(const char* msg) {
        if (s_level > Level::Debug) return;
        emitMsg(Level::Debug, nullptr, 0, msg);
    }
    static void info(const char* msg) {
        if (s_level > Level::Info) return;
        emitMsg(Level::Info, nullptr, 0, msg);
    }
    static void warn(const char* msg) {
        if (s_level > Level::Warn) return;
        emitMsg(Level::Warn, nullptr, 0, msg);
    }
    static void err(const char* msg) {
        emitMsg(Level::Error, nullptr, 0, msg);
    }

    // --- Classic API: variadic template overloads (with formatting) ---
    // Note: format checking relies on snprintf inlining at -O1+.
    // For guaranteed compile-time checking, prefer LOG_* macros.

    template<typename T, typename... Args>
    static void dbg(const char* fmt, T first, Args... rest) {
        if (s_level > Level::Debug) return;
        char buf[2048]; formatBuf(buf, sizeof(buf), fmt, first, rest...);
        emitMsg(Level::Debug, nullptr, 0, buf);
    }
    template<typename T, typename... Args>
    static void info(const char* fmt, T first, Args... rest) {
        if (s_level > Level::Info) return;
        char buf[2048]; formatBuf(buf, sizeof(buf), fmt, first, rest...);
        emitMsg(Level::Info, nullptr, 0, buf);
    }
    template<typename T, typename... Args>
    static void warn(const char* fmt, T first, Args... rest) {
        if (s_level > Level::Warn) return;
        char buf[2048]; formatBuf(buf, sizeof(buf), fmt, first, rest...);
        emitMsg(Level::Warn, nullptr, 0, buf);
    }
    template<typename T, typename... Args>
    static void err(const char* fmt, T first, Args... rest) {
        char buf[2048]; formatBuf(buf, sizeof(buf), fmt, first, rest...);
        emitMsg(Level::Error, nullptr, 0, buf);
    }

    // --- Subsystem/filename extraction from __FILE__ ---

    struct SubsystemTag {
        const char* name;
        int len;
    };

    static SubsystemTag extractSubsystem(const char* file) {
        SubsystemTag tag = { "app", 3 };
        if (!file) return tag;

        const char* after_src = nullptr;
        for (const char* p = file; *p; p++) {
            if (p[0] == 's' && p[1] == 'r' && p[2] == 'c' && (p[3] == '/' || p[3] == '\\'))
                after_src = p + 4;
        }
        if (!after_src) after_src = file;

        const char* sep = nullptr;
        for (const char* p = after_src; *p; p++) {
            if (*p == '/' || *p == '\\') { sep = p; break; }
        }

        if (sep && sep > after_src) {
            tag.name = after_src;
            tag.len = static_cast<int>(sep - after_src);
        }
        return tag;
    }

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
    static unsigned int s_counts[4];

    struct LevelInfo {
        const char* tag;
        const char* color;
    };

    static LevelInfo levelInfo(Level lvl) {
        static const LevelInfo table[] = {
            { "DBG", "\033[90m"   },
            { "INF", "\033[32m"   },
            { "WRN", "\033[33m"   },
            { "ERR", "\033[31;1m" },
        };
        return table[static_cast<int>(lvl)];
    }

    static bool detectColorSupport() {
#if defined(_WIN32)
        return false;
#else
        return isatty(fileno(stderr)) != 0;
#endif
    }

    static double elapsedSec() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - s_start).count();
    }

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

    // snprintf wrapper — isolated from public API.
    // Non-literal format is unavoidable here; checking happens at the call
    // site when the compiler inlines the template (any optimization level).
    template<typename... Args>
    static void formatBuf(char* buf, size_t sz, const char* fmt, Args... args) {
        snprintf(buf, sz, fmt, args...);
    }
};

// --- LOG_* macros: format at call site for full compile-time checking ---
//
// snprintf sees the literal format string directly → GCC/Clang verify
// argument types against format specifiers with zero false positives.
// Also provides automatic __FILE__:__LINE__ and subsystem tag.

#define LOG_DBG(fmt, ...) \
    do { if (Log::levelEnabled(Log::Level::Debug)) { \
        char _log_buf_[2048]; \
        snprintf(_log_buf_, sizeof(_log_buf_), fmt, ##__VA_ARGS__); \
        Log::emitMsg(Log::Level::Debug, __FILE__, __LINE__, _log_buf_); \
    }} while(0)

#define LOG_INF(fmt, ...) \
    do { if (Log::levelEnabled(Log::Level::Info)) { \
        char _log_buf_[2048]; \
        snprintf(_log_buf_, sizeof(_log_buf_), fmt, ##__VA_ARGS__); \
        Log::emitMsg(Log::Level::Info, __FILE__, __LINE__, _log_buf_); \
    }} while(0)

#define LOG_WRN(fmt, ...) \
    do { if (Log::levelEnabled(Log::Level::Warn)) { \
        char _log_buf_[2048]; \
        snprintf(_log_buf_, sizeof(_log_buf_), fmt, ##__VA_ARGS__); \
        Log::emitMsg(Log::Level::Warn, __FILE__, __LINE__, _log_buf_); \
    }} while(0)

#define LOG_ERR(fmt, ...) \
    do { \
        char _log_buf_[2048]; \
        snprintf(_log_buf_, sizeof(_log_buf_), fmt, ##__VA_ARGS__); \
        Log::emitMsg(Log::Level::Error, __FILE__, __LINE__, _log_buf_); \
    } while(0)
