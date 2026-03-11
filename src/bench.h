#pragma once
#include "renderer.h"
#include <string>
#include <vector>

struct BenchResult {
    std::string name;
    std::string unit;       // e.g. "Mpix/s", "Mtris/s"
    double score;
    double avg_ms;
    double min_ms;
    double max_ms;
    double median_ms;
    double p1_ms;           // 1st percentile
    double p99_ms;          // 99th percentile
    int    frames;
    bool   valid;

    BenchResult() : score(0), avg_ms(0), min_ms(0), max_ms(0),
                    median_ms(0), p1_ms(0), p99_ms(0), frames(0), valid(false) {}
};

// Base class for benchmark tests
class BenchTest {
public:
    virtual ~BenchTest() {}
    virtual const char* name() const = 0;
    virtual const char* scoreUnit() const = 0;
    virtual const char* description() const = 0;

    // Called once before measurement. Create GPU resources here.
    virtual void setup(Renderer* r, int viewport_w, int viewport_h) = 0;

    // Render one frame of the test. Called in a tight loop during measurement.
    virtual void render(Renderer* r) = 0;

    // Called once after measurement. Destroy GPU resources here.
    virtual void cleanup(Renderer* r) = 0;

    // Compute a test-specific score from the collected frame times.
    virtual double computeScore(const std::vector<double>& frame_times_ms,
                                int viewport_w, int viewport_h) = 0;
};

// Computes statistics from raw frame times
BenchResult computeStats(const std::string& name,
                         const std::string& unit,
                         const std::vector<double>& times_ms,
                         double score);
