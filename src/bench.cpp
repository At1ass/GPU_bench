#include "bench.h"
#include "test_registry.h"
#include "preset.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>

// Stability thresholds
static constexpr double CV_INVALID_THRESHOLD = 0.5;

// GPU tier score thresholds
static constexpr double TIER_LEGACY_MAX_SCORE = 50.0;
static constexpr double TIER_LOW_MAX_SCORE    = 500.0;
static constexpr double TIER_MID_MAX_SCORE    = 5000.0;

// GPU tier VRAM thresholds (MB)
static constexpr int TIER_LEGACY_MAX_VRAM = 128;
static constexpr int TIER_LOW_MAX_VRAM    = 512;
static constexpr int TIER_MID_MAX_VRAM    = 2048;

// GPU tier texture size thresholds
static constexpr int TIER_LEGACY_MAX_TEXSIZE = 2048;
static constexpr int TIER_LOW_MAX_TEXSIZE    = 4096;

// Bottleneck detection thresholds
static constexpr double BOTTLENECK_SIGNIFICANT_RATIO = 0.5;
static constexpr double BOTTLENECK_WEAKNESS_RATIO    = 0.8;
static constexpr double DRAWCALL_OVERHEAD_RATIO      = 1.5;
static constexpr double ALU_FMA_DIVERGENCE_RATIO     = 3.0;

static double percentile(std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0;
    double idx = p * (sorted.size() - 1);
    int lo = static_cast<int>(floor(idx));
    int hi = static_cast<int>(ceil(idx));
    if (lo == hi) return sorted[lo];
    double frac = idx - lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

BenchResult computeStats(const std::string& name,
                         const std::string& unit,
                         const std::vector<double>& times_ms,
                         double score) {
    BenchResult r;
    r.name   = name;
    r.unit   = unit;
    r.score  = score;
    r.frames = static_cast<int>(times_ms.size());

    if (times_ms.empty()) {
        r.valid = false;
        return r;
    }

    auto sorted = times_ms;
    std::sort(sorted.begin(), sorted.end());

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    r.avg_ms    = sum / sorted.size();
    r.min_ms    = sorted.front();
    r.max_ms    = sorted.back();
    r.median_ms = percentile(sorted, 0.5);
    r.p1_ms     = percentile(sorted, 0.01);
    r.p99_ms    = percentile(sorted, 0.99);

    // Stability metrics
    double variance = 0;
    for (double val : sorted) {
        double diff = val - r.avg_ms;
        variance += diff * diff;
    }
    double stddev = sqrt(variance / sorted.size());
    r.cv = (r.avg_ms > 0) ? stddev / r.avg_ms : 0;
    r.p99_median_ratio = (r.median_ms > 0) ? r.p99_ms / r.median_ms : 0;

    // Mark as invalid if CV is too high
    r.valid = (r.cv <= CV_INVALID_THRESHOLD);
    return r;
}

// Helper: find a result by name
static const BenchResult* findResult(const std::vector<BenchResult>& results, const char* name) {
    for (const auto& r : results) {
        if (r.name == name) return &r;
    }
    return nullptr;
}

// Geometric mean of non-zero values
static double geomean(const double* vals, int count) {
    double product = 1.0;
    int valid = 0;
    for (int i = 0; i < count; i++) {
        if (vals[i] > 0) {
            product *= vals[i];
            valid++;
        }
    }
    if (valid == 0) return 0;
    return pow(product, 1.0 / valid);
}

CompositeScore computeCompositeScores(const std::vector<BenchResult>& results) {
    CompositeScore cs;

    // Data-driven: collect scores per category from registry
    double cat_vals[static_cast<int>(TestCategory::Count)][NUM_TESTS];
    int cat_count[static_cast<int>(TestCategory::Count)] = {};

    for (int i = 0; i < NUM_TESTS; i++) {
        int ci = static_cast<int>(g_tests[i].category);
        cat_vals[ci][cat_count[ci]] = 0;

        const BenchResult* r = findResult(results, g_tests[i].display_name);
        if (r && r->score > 0)
            cat_vals[ci][cat_count[ci]] = r->score;

        cat_count[ci]++;
    }

    int ci_fill = static_cast<int>(TestCategory::Fill);
    int ci_geom = static_cast<int>(TestCategory::Geometry);
    int ci_comp = static_cast<int>(TestCategory::Compute);
    int ci_over = static_cast<int>(TestCategory::Overhead);

    cs.fill     = geomean(cat_vals[ci_fill], cat_count[ci_fill]);
    cs.geometry = geomean(cat_vals[ci_geom], cat_count[ci_geom]);
    cs.compute  = geomean(cat_vals[ci_comp], cat_count[ci_comp]);
    cs.overhead = geomean(cat_vals[ci_over], cat_count[ci_over]);

    // Overall: geometric mean of category scores (excluding Mixed)
    double cats[4] = { cs.fill, cs.geometry, cs.compute, cs.overhead };
    cs.overall = geomean(cats, 4);

    return cs;
}

const char* gpuTierName(GPUTier tier) {
    switch (tier) {
        case GPUTier::Legacy: return "legacy";
        case GPUTier::Low:    return "low";
        case GPUTier::Mid:    return "mid";
        case GPUTier::High:   return "high";
        default:              return "unknown";
    }
}

GPUTier classifyGPUTier(const RenderCaps& caps,
                        double probe_fill_mpixs,
                        double probe_geom_ktris) {
    // If we have probe data, use it as primary classifier
    if (probe_fill_mpixs > 0 && probe_geom_ktris > 0) {
        // Combined score: geometric mean of fill and geometry probe
        double combined = sqrt(probe_fill_mpixs * probe_geom_ktris);
        if (combined < TIER_LEGACY_MAX_SCORE) return GPUTier::Legacy;
        if (combined < TIER_LOW_MAX_SCORE)   return GPUTier::Low;
        if (combined < TIER_MID_MAX_SCORE)   return GPUTier::Mid;
        return GPUTier::High;
    }

    // Fallback: classify from caps only
    // GL 2.x without VAO → legacy
    if (caps.gl_major < 3 && !caps.has_vao)
        return GPUTier::Legacy;

    // Use VRAM as rough proxy
    if (caps.estimated_vram_mb > 0) {
        if (caps.estimated_vram_mb < TIER_LEGACY_MAX_VRAM) return GPUTier::Legacy;
        if (caps.estimated_vram_mb < TIER_LOW_MAX_VRAM)  return GPUTier::Low;
        if (caps.estimated_vram_mb < TIER_MID_MAX_VRAM)  return GPUTier::Mid;
        return GPUTier::High;
    }

    // Use max texture size as last resort
    if (caps.max_texture_size <= TIER_LEGACY_MAX_TEXSIZE) return GPUTier::Legacy;
    if (caps.max_texture_size <= TIER_LOW_MAX_TEXSIZE)   return GPUTier::Low;
    return GPUTier::Mid;
}

int tierToPresetIndex(GPUTier tier) {
    switch (tier) {
        case GPUTier::Legacy: return static_cast<int>(PresetIndex::Light);
        case GPUTier::Low:    return static_cast<int>(PresetIndex::Medium);
        case GPUTier::Mid:    return static_cast<int>(PresetIndex::Heavy);
        case GPUTier::High:   return static_cast<int>(PresetIndex::Ultra);
        default:              return static_cast<int>(PresetIndex::Medium);
    }
}

BottleneckInfo detectBottleneck(const std::vector<BenchResult>& results,
                                const CompositeScore& scores) {
    BottleneckInfo info;

    // Find weakest category relative to others
    struct { const char* name; double score; } cats[] = {
        {"Fill", scores.fill},
        {"Geometry", scores.geometry},
        {"Compute", scores.compute},
        {"Overhead", scores.overhead}
    };

    // Count non-zero categories
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (cats[i].score > 0) count++;
    }

    // Need all 4 categories for meaningful cross-category comparison
    // (scores use different units: Mpix/s, Ktris/s, Gops, etc.)
    if (count < 4) {
        char buf[256];
        if (count == 0) {
            info.detail = "No test data for bottleneck analysis";
        } else {
            // List missing categories
            std::string missing;
            for (int i = 0; i < 4; i++) {
                if (cats[i].score <= 0) {
                    if (!missing.empty()) missing += ", ";
                    missing += cats[i].name;
                }
            }
            snprintf(buf, sizeof(buf), "Incomplete data — run all tests for bottleneck analysis (missing: %s)",
                     missing.c_str());
            info.detail = buf;
        }
        return info;
    }

    // Normalize each category to 0..1 range relative to its own max across runs.
    // Since we compare within a single run, use ratio-to-average as relative metric.
    double sum = 0;
    for (int i = 0; i < 4; i++) sum += cats[i].score;
    double avg = sum / 4;

    // Find weakest
    double min_ratio = 1e9;
    int weakest = -1;
    for (int i = 0; i < 4; i++) {
        double ratio = cats[i].score / avg;
        if (ratio < min_ratio) {
            min_ratio = ratio;
            weakest = i;
        }
    }

    if (weakest >= 0) {
        info.weakest_category = cats[weakest].name;
        info.weakness_ratio = min_ratio;

        char buf[256];
        if (min_ratio < BOTTLENECK_SIGNIFICANT_RATIO) {
            snprintf(buf, sizeof(buf), "%s is significantly weaker (%.0f%% of average)",
                     cats[weakest].name, min_ratio * 100.0);
        } else if (min_ratio < BOTTLENECK_WEAKNESS_RATIO) {
            snprintf(buf, sizeof(buf), "%s is the weakest category (%.0f%% of average)",
                     cats[weakest].name, min_ratio * 100.0);
        } else {
            snprintf(buf, sizeof(buf), "Balanced performance (weakest: %s at %.0f%%)",
                     cats[weakest].name, min_ratio * 100.0);
        }
        info.detail = buf;
    }

    // Compare DrawCall vs DrawCallRaw if both present
    const BenchResult* dc = findResult(results, "DrawCall");
    const BenchResult* dcr = findResult(results, "DrawCallRaw");
    if (dc && dcr && dc->valid && dcr->valid && dc->score > 0) {
        double ratio = dcr->score / dc->score;
        if (ratio > DRAWCALL_OVERHEAD_RATIO) {
            char buf2[256];
            snprintf(buf2, sizeof(buf2), ". Uniform updates cost %.0f%% of draw call time",
                     (ratio - 1.0) * 100.0);
            info.detail += buf2;
        }
    }

    // Compare ShaderALU vs ShaderFMA
    const BenchResult* alu = findResult(results, "ShaderALU");
    const BenchResult* fma = findResult(results, "ShaderFMA");
    if (alu && fma && alu->valid && fma->valid && alu->score > 0) {
        double ratio = fma->score / alu->score;
        if (ratio > ALU_FMA_DIVERGENCE_RATIO) {
            info.detail += ". SFU (sin/cos/pow) is significantly slower than FMA";
        }
    }

    return info;
}
