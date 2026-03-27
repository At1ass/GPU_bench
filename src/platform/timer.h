#pragma once

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <time.h>
#endif

class Timer {
public:
    Timer() {
#ifdef _WIN32
        QueryPerformanceFrequency(&freq_);
#endif
        reset();
    }

    void reset() {
#ifdef _WIN32
        QueryPerformanceCounter(&start_);
#else
        clock_gettime(CLOCK_MONOTONIC, &start_);
#endif
    }

    double elapsed_ms() const {
#ifdef _WIN32
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart - start_.QuadPart) * 1000.0 / (double)freq_.QuadPart;
#else
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (double)(now.tv_sec - start_.tv_sec) * 1000.0
             + (double)(now.tv_nsec - start_.tv_nsec) / 1000000.0;
#endif
    }

    double elapsed_sec() const { return elapsed_ms() / 1000.0; }

private:
#ifdef _WIN32
    LARGE_INTEGER freq_, start_;
#else
    struct timespec start_;
#endif
};
