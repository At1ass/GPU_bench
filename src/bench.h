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
    double cv;              // coefficient of variation (stddev/avg)
    double p99_median_ratio; // p99/median ratio
    double gpu_ms;          // GPU timer result (0 if unavailable)
    int    frames;
    bool   valid;
    bool   sanity_ok;  // true if render output is non-black (sanity check passed)

    BenchResult() : score(0), avg_ms(0), min_ms(0), max_ms(0),
                    median_ms(0), p1_ms(0), p99_ms(0), cv(0), p99_median_ratio(0),
                    gpu_ms(0), frames(0), valid(false), sanity_ok(true) {}
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

// Composite score (geometric mean of normalized scores by category)
struct CompositeScore {
    double overall;
    double fill;       // Fillrate, Overdraw, Texturing
    double geometry;   // Geometry, Vertex
    double compute;    // ShaderALU, ShaderFMA
    double overhead;   // DrawCall, DrawCallRaw, StateChange, TexUpload

    CompositeScore() : overall(0), fill(0), geometry(0), compute(0), overhead(0) {}
};

CompositeScore computeCompositeScores(const std::vector<BenchResult>& results);

// Bottleneck detection
struct BottleneckInfo {
    std::string weakest_category;  // "Fill", "Geometry", "Compute", "Overhead"
    std::string detail;            // Human-readable explanation
    double weakness_ratio;         // How much weaker vs average (0..1 = bad..ok)

    BottleneckInfo() : weakness_ratio(1.0) {}
};

BottleneckInfo detectBottleneck(const std::vector<BenchResult>& results,
                                const CompositeScore& scores);

// GPU performance tier classification
enum GPUTier {
    GPU_TIER_LEGACY = 0,  // GL 2.x class, very old GPUs
    GPU_TIER_LOW    = 1,  // Low-end / old discrete or integrated
    GPU_TIER_MID    = 2,  // Mid-range
    GPU_TIER_HIGH   = 3   // High-end
};

const char* gpuTierName(GPUTier tier);

// Classify GPU tier from caps and optional probe scores (0 = no probe data)
GPUTier classifyGPUTier(const RenderCaps& caps,
                        double probe_fill_mpixs = 0,
                        double probe_geom_ktris = 0);

// Map GPU tier to suggested preset index
int tierToPresetIndex(GPUTier tier);
