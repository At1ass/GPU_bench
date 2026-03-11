#include "bench.h"
#include "preset.h"
#include <algorithm>
#include <numeric>
#include <cmath>

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

    std::vector<double> sorted = times_ms;
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
    for (size_t i = 0; i < sorted.size(); i++) {
        double diff = sorted[i] - r.avg_ms;
        variance += diff * diff;
    }
    double stddev = sqrt(variance / sorted.size());
    r.cv = (r.avg_ms > 0) ? stddev / r.avg_ms : 0;
    r.p99_median_ratio = (r.median_ms > 0) ? r.p99_ms / r.median_ms : 0;

    // Mark as invalid if CV is too high
    r.valid = (r.cv <= 0.5);
    return r;
}

// Helper: find a result by name
static const BenchResult* findResult(const std::vector<BenchResult>& results, const char* name) {
    for (size_t i = 0; i < results.size(); i++) {
        if (results[i].name == name) return &results[i];
    }
    return 0;
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

    // Fill category: Fillrate, Overdraw, Texturing
    {
        double vals[3] = {0, 0, 0};
        const BenchResult* r;
        if ((r = findResult(results, "Fillrate")) && r->score > 0) vals[0] = r->score;
        if ((r = findResult(results, "Overdraw")) && r->score > 0) vals[1] = r->score;
        if ((r = findResult(results, "Texturing")) && r->score > 0) vals[2] = r->score;
        cs.fill = geomean(vals, 3);
    }

    // Geometry category: Geometry, Vertex
    {
        double vals[2] = {0, 0};
        const BenchResult* r;
        if ((r = findResult(results, "Geometry")) && r->score > 0) vals[0] = r->score;
        if ((r = findResult(results, "Vertex")) && r->score > 0) vals[1] = r->score;
        cs.geometry = geomean(vals, 2);
    }

    // Compute category: ShaderALU, ShaderFMA
    {
        double vals[2] = {0, 0};
        const BenchResult* r;
        if ((r = findResult(results, "ShaderALU")) && r->score > 0) vals[0] = r->score;
        if ((r = findResult(results, "ShaderFMA")) && r->score > 0) vals[1] = r->score;
        cs.compute = geomean(vals, 2);
    }

    // Overhead category: DrawCall, DrawCallRaw, StateChange, TexUpload
    {
        double vals[4] = {0, 0, 0, 0};
        const BenchResult* r;
        if ((r = findResult(results, "DrawCall")) && r->score > 0) vals[0] = r->score;
        if ((r = findResult(results, "DrawCallRaw")) && r->score > 0) vals[1] = r->score;
        if ((r = findResult(results, "StateChange")) && r->score > 0) vals[2] = r->score;
        if ((r = findResult(results, "TexUpload")) && r->score > 0) vals[3] = r->score;
        cs.overhead = geomean(vals, 4);
    }

    // Overall: geometric mean of category scores
    {
        double cats[4] = { cs.fill, cs.geometry, cs.compute, cs.overhead };
        cs.overall = geomean(cats, 4);
    }

    return cs;
}

const char* gpuTierName(GPUTier tier) {
    switch (tier) {
        case GPU_TIER_LEGACY: return "legacy";
        case GPU_TIER_LOW:    return "low";
        case GPU_TIER_MID:    return "mid";
        case GPU_TIER_HIGH:   return "high";
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
        if (combined < 50)    return GPU_TIER_LEGACY;
        if (combined < 500)   return GPU_TIER_LOW;
        if (combined < 5000)  return GPU_TIER_MID;
        return GPU_TIER_HIGH;
    }

    // Fallback: classify from caps only
    // GL 2.x without VAO → legacy
    if (caps.gl_major < 3 && !caps.has_vao)
        return GPU_TIER_LEGACY;

    // Use VRAM as rough proxy
    if (caps.estimated_vram_mb > 0) {
        if (caps.estimated_vram_mb < 128)  return GPU_TIER_LEGACY;
        if (caps.estimated_vram_mb < 512)  return GPU_TIER_LOW;
        if (caps.estimated_vram_mb < 2048) return GPU_TIER_MID;
        return GPU_TIER_HIGH;
    }

    // Use max texture size as last resort
    if (caps.max_texture_size <= 2048) return GPU_TIER_LEGACY;
    if (caps.max_texture_size <= 4096) return GPU_TIER_LOW;
    return GPU_TIER_MID;
}

int tierToPresetIndex(GPUTier tier) {
    switch (tier) {
        case GPU_TIER_LEGACY: return PRESET_LIGHT;
        case GPU_TIER_LOW:    return PRESET_MEDIUM;
        case GPU_TIER_MID:    return PRESET_HEAVY;
        case GPU_TIER_HIGH:   return PRESET_ULTRA;
        default:              return PRESET_MEDIUM;
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
        if (min_ratio < 0.5) {
            snprintf(buf, sizeof(buf), "%s is significantly weaker (%.0f%% of average)",
                     cats[weakest].name, min_ratio * 100.0);
        } else if (min_ratio < 0.8) {
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
        if (ratio > 1.5) {
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
        if (ratio > 3.0) {
            info.detail += ". SFU (sin/cos/pow) is significantly slower than FMA";
        }
    }

    return info;
}
