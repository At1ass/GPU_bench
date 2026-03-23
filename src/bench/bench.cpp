#include "bench/bench.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include "tests/test_registry.h"
#include "bench/preset.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <cstring>
#include <unordered_map>

// GL3BenchTest
void GL3BenchTest::setup(Renderer* r, int vw, int vh) {
    gl3_ = r->features<GL3Features>();
    if (!gl3_) {
        Log::err("GL3BenchTest '%s': renderer lacks GL3 — skipping", name());
        return;
    }
    setupGL3(*r, *gl3_, vw, vh);
}
void GL3BenchTest::render(Renderer* r) {
    if (!gl3_) return;
    renderGL3(*r, *gl3_);
}
void GL3BenchTest::cleanup(Renderer* r) {
    if (!gl3_) return;
    cleanupGL3(*r, *gl3_);
    gl3_ = nullptr;
}

// ComputeBenchTest
void ComputeBenchTest::setup(Renderer* r, int vw, int vh) {
    comp_ = r->features<ComputeFeatures>();
    if (!comp_) {
        Log::err("ComputeBenchTest '%s': renderer lacks Compute — skipping", name());
        return;
    }
    setupCompute(*r, *comp_, vw, vh);
}
void ComputeBenchTest::render(Renderer* r) {
    if (!comp_) return;
    renderCompute(*r, *comp_);
}
void ComputeBenchTest::cleanup(Renderer* r) {
    if (!comp_) return;
    cleanupCompute(*r, *comp_);
    comp_ = nullptr;
}

// GL4BenchTest
void GL4BenchTest::setup(Renderer* r, int vw, int vh) {
    gl4_ = r->features<GL4Features>();
    if (!gl4_) {
        Log::err("GL4BenchTest '%s': renderer lacks GL4 — skipping", name());
        return;
    }
    setupGL4(*r, *gl4_, vw, vh);
}
void GL4BenchTest::render(Renderer* r) {
    if (!gl4_) return;
    renderGL4(*r, *gl4_);
}
void GL4BenchTest::cleanup(Renderer* r) {
    if (!gl4_) return;
    cleanupGL4(*r, *gl4_);
    gl4_ = nullptr;
}

// Stability thresholds
static constexpr double CV_INVALID_THRESHOLD = 0.5;

// GPU tier score thresholds (geometric mean of fill Mpix/s × geom Mtris/s)
// Expected ranges (512x512 probe, 200 fill layers, grid=25):
//   Legacy (GeForce 6200):   fill ~200, geom ~10   -> ~45
//   Low    (GeForce 8600):   fill ~2000, geom ~50  -> ~316
//   Mid    (GTX 560 Ti):     fill ~15000, geom ~200 -> ~1732
//   High   (GTX 1060):       fill ~40000, geom ~400 -> ~4000
//   Ultra  (RTX 4070 Ti):    fill ~70000, geom ~700 -> ~7000
static constexpr double TIER_LEGACY_MAX_SCORE = 100.0;
static constexpr double TIER_LOW_MAX_SCORE    = 700.0;
static constexpr double TIER_MID_MAX_SCORE    = 3000.0;
static constexpr double TIER_HIGH_MAX_SCORE   = 6000.0;

// GPU tier VRAM thresholds (MB)
static constexpr int TIER_LEGACY_MAX_VRAM = 128;
static constexpr int TIER_LOW_MAX_VRAM    = 512;
static constexpr int TIER_MID_MAX_VRAM    = 2048;
static constexpr int TIER_HIGH_MAX_VRAM   = 6144;

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
    double idx = p * static_cast<double>(sorted.size() - 1);
    size_t lo = static_cast<size_t>(floor(idx));
    size_t hi = static_cast<size_t>(ceil(idx));
    if (lo == hi) return sorted[lo];
    double frac = idx - static_cast<double>(lo);
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
    r.avg_ms    = sum / static_cast<double>(sorted.size());
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
    double stddev = sqrt(variance / static_cast<double>(sorted.size() > 1 ? sorted.size() - 1 : 1));
    r.cv = (r.avg_ms > 0) ? stddev / r.avg_ms : 0;
    r.p99_median_ratio = (r.median_ms > 0) ? r.p99_ms / r.median_ms : 0;

    // Mark as invalid if CV is too high
    r.valid = (r.cv <= CV_INVALID_THRESHOLD);
    return r;
}

// Build index for O(1) result lookup by name
typedef std::unordered_map<std::string, const BenchResult*> ResultIndex;

static ResultIndex buildResultIndex(const std::vector<BenchResult>& results) {
    ResultIndex idx;
    idx.reserve(results.size());
    for (const auto& r : results) idx[r.name] = &r;
    return idx;
}

static const BenchResult* findResult(const ResultIndex& idx, const char* name) {
    auto it = idx.find(name);
    return (it != idx.end()) ? it->second : nullptr;
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
    auto idx = buildResultIndex(results);

    // Data-driven: collect scores per category from registry
    double cat_vals[static_cast<int>(TestCategory::Count)][NUM_TESTS];
    int cat_count[static_cast<int>(TestCategory::Count)] = {};

    for (int i = 0; i < NUM_TESTS; i++) {
        int ci = static_cast<int>(g_tests[i].category);
        cat_vals[ci][cat_count[ci]] = 0;

        auto it = idx.find(g_tests[i].display_name);
        const BenchResult* r = (it != idx.end()) ? it->second : nullptr;
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
        case GPUTier::Ultra:  return "ultra";
        default:              return "unknown";
    }
}

GPUTier classifyGPUTier(const RenderCaps& caps,
                        uint32_t available_caps,
                        double probe_fill_mpixs,
                        double probe_geom_ktris) {
    // If we have probe data, use it as primary classifier
    if (probe_fill_mpixs > 0 && probe_geom_ktris > 0) {
        // Combined score: geometric mean of fill and geometry probe
        double combined = sqrt(probe_fill_mpixs * probe_geom_ktris);
        if (combined < TIER_LEGACY_MAX_SCORE) return GPUTier::Legacy;
        if (combined < TIER_LOW_MAX_SCORE)    return GPUTier::Low;
        if (combined < TIER_MID_MAX_SCORE)    return GPUTier::Mid;
        if (combined < TIER_HIGH_MAX_SCORE)   return GPUTier::High;
        return GPUTier::Ultra;
    }

    // Fallback: classify from caps only
    // GL 2.x without VAO -> legacy
    if (caps.gl_major < 3 && !(available_caps & Cap_VAO))
        return GPUTier::Legacy;

    // Use VRAM as rough proxy
    if (caps.estimated_vram_mb > 0) {
        if (caps.estimated_vram_mb < TIER_LEGACY_MAX_VRAM) return GPUTier::Legacy;
        if (caps.estimated_vram_mb < TIER_LOW_MAX_VRAM)    return GPUTier::Low;
        if (caps.estimated_vram_mb < TIER_MID_MAX_VRAM)    return GPUTier::Mid;
        if (caps.estimated_vram_mb < TIER_HIGH_MAX_VRAM)   return GPUTier::High;
        return GPUTier::Ultra;
    }

    // Use max texture size as last resort
    if (caps.max_texture_size <= TIER_LEGACY_MAX_TEXSIZE) return GPUTier::Legacy;
    if (caps.max_texture_size <= TIER_LOW_MAX_TEXSIZE)    return GPUTier::Low;
    return GPUTier::Mid;
}

int tierToPresetIndex(GPUTier tier) {
    switch (tier) {
        case GPUTier::Legacy: return static_cast<int>(PresetIndex::Light);
        case GPUTier::Low:    return static_cast<int>(PresetIndex::Medium);
        case GPUTier::Mid:    return static_cast<int>(PresetIndex::Heavy);
        case GPUTier::High:   return static_cast<int>(PresetIndex::Ultra);
        case GPUTier::Ultra:  return static_cast<int>(PresetIndex::Extreme);
        default:              return static_cast<int>(PresetIndex::Medium);
    }
}

BottleneckInfo detectBottleneck(const std::vector<BenchResult>& results,
                                const CompositeScore& scores) {
    BottleneckInfo info;
    auto idx = buildResultIndex(results);

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
    const BenchResult* dc = findResult(idx, "DrawCall");
    const BenchResult* dcr = findResult(idx, "DrawCallRaw");
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
    const BenchResult* alu = findResult(idx, "ShaderALU");
    const BenchResult* fma = findResult(idx, "ShaderFMA");
    if (alu && fma && alu->valid && fma->valid && alu->score > 0) {
        double ratio = fma->score / alu->score;
        if (ratio > ALU_FMA_DIVERGENCE_RATIO) {
            info.detail += ". SFU (sin/cos/pow) is significantly slower than FMA";
        }
    }

    // Compare FBOFillrate vs Fillrate → FBO overhead
    const BenchResult* fill = findResult(idx, "Fillrate");
    const BenchResult* fbo_fill = findResult(idx, "FBOFillrate");
    if (fill && fbo_fill && fill->valid && fbo_fill->valid && fill->score > 0) {
        double ratio = fbo_fill->score / fill->score;
        if (ratio < BOTTLENECK_WEAKNESS_RATIO) {
            char buf2[256];
            snprintf(buf2, sizeof(buf2), ". FBO overhead: render-to-texture at %.0f%% of direct fill",
                     ratio * 100.0);
            info.detail += buf2;
        }
    }

    // Compare IndirectDraw vs DrawCallRaw → indirect vs direct
    const BenchResult* indirect = findResult(idx, "IndirectDraw");
    if (dcr && indirect && dcr->valid && indirect->valid && indirect->score > 0) {
        double ratio = indirect->score / dcr->score;
        if (ratio > DRAWCALL_OVERHEAD_RATIO) {
            char buf2[256];
            snprintf(buf2, sizeof(buf2), ". Indirect draw %.1fx faster than direct",
                     ratio);
            info.detail += buf2;
        }
    }

    // Compare ComputeBandwidth vs ComputeFMA → bandwidth vs ALU bound
    const BenchResult* cbw = findResult(idx, "ComputeBW");
    const BenchResult* cfma = findResult(idx, "ComputeFMA");
    if (cbw && cfma && cbw->valid && cfma->valid) {
        // Both valid — report which is the bottleneck
        // (different units, so we note presence rather than ratio)
        info.detail += ". Compute: BW and ALU data available for analysis";
    }

    // Compare ComputeSharedMem vs ComputeBandwidth → shared vs global
    const BenchResult* csmem = findResult(idx, "ComputeSMem");
    if (cbw && csmem && cbw->valid && csmem->valid && cbw->score > 0) {
        double ratio = csmem->score / cbw->score;
        if (ratio > 2.0) {
            char buf2[256];
            snprintf(buf2, sizeof(buf2), ". Shared memory %.1fx faster than global",
                     ratio);
            info.detail += buf2;
        }
    }

    return info;
}
