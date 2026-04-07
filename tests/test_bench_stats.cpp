// Unit tests for computeStats() — statistics, percentiles, CV, validity
#include "doctest.h"
#include "bench/bench.h"
#include <vector>
#include <cmath>

TEST_CASE("computeStats — constant values") {
    std::vector<double> times(100, 5.0);
    BenchResult r = computeStats("Test", "FPS", times, 200.0);

    CHECK(r.name == "Test");
    CHECK(r.unit == "FPS");
    CHECK(r.score == doctest::Approx(200.0));
    CHECK(r.frames == 100);
    CHECK(r.avg_ms == doctest::Approx(5.0));
    CHECK(r.min_ms == doctest::Approx(5.0));
    CHECK(r.max_ms == doctest::Approx(5.0));
    CHECK(r.median_ms == doctest::Approx(5.0));
    CHECK(r.cv == doctest::Approx(0.0));
    CHECK(r.valid == true);
}

TEST_CASE("computeStats — linear sequence") {
    std::vector<double> times;
    for (int i = 1; i <= 10; i++) times.push_back(static_cast<double>(i));

    BenchResult r = computeStats("Linear", "ms", times, 100.0);

    CHECK(r.avg_ms == doctest::Approx(5.5));
    CHECK(r.min_ms == doctest::Approx(1.0));
    CHECK(r.max_ms == doctest::Approx(10.0));
    CHECK(r.median_ms == doctest::Approx(5.5));
    CHECK(r.frames == 10);
    // CV of [1..10] ≈ 0.55, exceeds 0.5 threshold → marked invalid
    CHECK(r.cv > 0.5);
    CHECK(r.valid == false);
}

TEST_CASE("computeStats — percentiles on 100 values") {
    std::vector<double> times;
    for (int i = 1; i <= 100; i++) times.push_back(static_cast<double>(i));

    BenchResult r = computeStats("Pct", "ms", times, 50.0);

    // P1 of [1..100]: index = 0.01 * 99 = 0.99 → lerp(1, 2, 0.99) = 1.99
    CHECK(r.p1_ms == doctest::Approx(1.99).epsilon(0.01));
    // P99: index = 0.99 * 99 = 98.01 → lerp(99, 100, 0.01) = 99.01
    CHECK(r.p99_ms == doctest::Approx(99.01).epsilon(0.01));
    // Median: index = 0.5 * 99 = 49.5 → lerp(50, 51, 0.5) = 50.5
    CHECK(r.median_ms == doctest::Approx(50.5));
}

TEST_CASE("computeStats — high CV marks invalid") {
    // CV > 0.5 should mark result as invalid
    // Mean = 10, stddev needs to be > 5 for CV > 0.5
    std::vector<double> times;
    for (int i = 0; i < 50; i++) times.push_back(1.0);
    for (int i = 0; i < 50; i++) times.push_back(30.0);

    BenchResult r = computeStats("Unstable", "ms", times, 100.0);

    CHECK(r.cv > 0.5);
    CHECK(r.valid == false);
}

TEST_CASE("computeStats — low CV is valid") {
    // Tight cluster around 10.0 with small noise
    std::vector<double> times;
    for (int i = 0; i < 100; i++) times.push_back(10.0 + (i % 3) * 0.1);

    BenchResult r = computeStats("Stable", "ms", times, 100.0);

    CHECK(r.cv < 0.5);
    CHECK(r.valid == true);
}

TEST_CASE("computeStats — single value") {
    std::vector<double> times = {7.5};
    BenchResult r = computeStats("Single", "ms", times, 133.0);

    CHECK(r.avg_ms == doctest::Approx(7.5));
    CHECK(r.min_ms == doctest::Approx(7.5));
    CHECK(r.max_ms == doctest::Approx(7.5));
    CHECK(r.median_ms == doctest::Approx(7.5));
    CHECK(r.cv == doctest::Approx(0.0));
    CHECK(r.frames == 1);
    CHECK(r.valid == true);
}

TEST_CASE("computeStats — empty input") {
    std::vector<double> times;
    BenchResult r = computeStats("Empty", "ms", times, 0.0);

    CHECK(r.frames == 0);
    CHECK(r.valid == false);
}

TEST_CASE("computeStats — p99/median ratio") {
    std::vector<double> times;
    for (int i = 0; i < 99; i++) times.push_back(10.0);
    times.push_back(100.0);  // single spike

    BenchResult r = computeStats("Spike", "ms", times, 50.0);

    CHECK(r.median_ms == doctest::Approx(10.0).epsilon(0.1));
    CHECK(r.p99_ms > r.median_ms);
    CHECK(r.p99_median_ratio > 1.0);
}

TEST_CASE("computeStats — score is passed through") {
    std::vector<double> times(10, 1.0);
    BenchResult r = computeStats("PassThrough", "Mpix/s", times, 12345.678);

    CHECK(r.score == doctest::Approx(12345.678));
}
