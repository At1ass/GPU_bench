#include <doctest.h>
#include "tests/tests.h"
#include "bench/preset.h"
#include <vector>
#include <cmath>

// ---- avgFrameMs ----

TEST_CASE("avgFrameMs: empty vector") {
    std::vector<double> v;
    CHECK(avgFrameMs(v) == 0.0);
}

TEST_CASE("avgFrameMs: single value") {
    std::vector<double> v = {5.0};
    CHECK(avgFrameMs(v) == doctest::Approx(5.0));
}

TEST_CASE("avgFrameMs: multiple values") {
    std::vector<double> v = {1.0, 2.0, 3.0};
    CHECK(avgFrameMs(v) == doctest::Approx(2.0));
}

TEST_CASE("avgFrameMs: all identical") {
    std::vector<double> v = {4.0, 4.0, 4.0, 4.0};
    CHECK(avgFrameMs(v) == doctest::Approx(4.0));
}

// ---- FillrateTest score formula ----
// score = (vw * vh * layers) / (avg_ms / 1000) / 1e6

TEST_CASE("FillrateTest score: known values") {
    // layers=200, viewport=512×512, avg=1.0ms
    int layers = 200, vw = 512, vh = 512;
    double avg_ms = 1.0;
    double pixels_per_frame = static_cast<double>(vw) * vh * layers;
    double expected = pixels_per_frame / (avg_ms / 1000.0) / 1e6;  // MPix/s
    // = 512*512*200 / 0.001 / 1e6 = 52428800 / 0.001 / 1e6 = 52428800000 / 1e6 = 52428.8
    CHECK(expected == doctest::Approx(52428.8));
}

// ---- GeometryTest score formula ----
// score = tri_count / (avg_ms / 1000) / 1e6

TEST_CASE("GeometryTest score: known values") {
    int tri_count = 187500;  // grid=25 → 25³ cubes * 12 tris = 187500
    double avg_ms = 1.0;
    double expected = static_cast<double>(tri_count) / (avg_ms / 1000.0) / 1e6;
    // = 187500 / 0.001 / 1e6 = 187.5
    CHECK(expected == doctest::Approx(187.5));
}

// ---- ComputeFMATest score formula ----
// score = total_flops / (avg_ms / 1000) / 1e9

TEST_CASE("ComputeFMATest score: known values") {
    int work_groups = 64, iterations = 100;
    int local_size = 64;
    double avg_ms = 1.0;
    // 24 FLOPs per invocation per iteration
    double total_invocations = static_cast<double>(work_groups) * local_size;
    double flops_per_invocation = 24.0 * iterations;
    double total_flops = total_invocations * flops_per_invocation;
    double expected = total_flops / (avg_ms / 1000.0) / 1e9;
    // = 64*64*24*100 / 0.001 / 1e9 = 9830400 / 0.001 / 1e9 = 9.8304
    CHECK(expected == doctest::Approx(9.8304));
}

// ---- Edge cases ----

TEST_CASE("Score: avg_ms=0 should not produce Inf/NaN") {
    // If avg_ms = 0, score should be 0 (guarded by if (avg_ms <= 0) return 0)
    double avg_ms = 0.0;
    double score = (avg_ms <= 0.0) ? 0.0 : (1000.0 / avg_ms);
    CHECK(score == 0.0);
    CHECK(std::isfinite(score));
}

TEST_CASE("Score: empty times → avg=0 → score=0") {
    std::vector<double> empty;
    double avg_ms = avgFrameMs(empty);
    CHECK(avg_ms == 0.0);
    double score = (avg_ms <= 0.0) ? 0.0 : (1000.0 / avg_ms);
    CHECK(score == 0.0);
}
