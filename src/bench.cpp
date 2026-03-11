#include "bench.h"
#include <algorithm>
#include <numeric>
#include <cmath>

static double percentile(std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0;
    double idx = p * (sorted.size() - 1);
    int lo = (int)floor(idx);
    int hi = (int)ceil(idx);
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
    r.frames = (int)times_ms.size();

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
    r.valid     = true;
    return r;
}
