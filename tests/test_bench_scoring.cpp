// Unit tests for computeCompositeScores() and detectBottleneck()
#include "doctest.h"
#include "bench/bench.h"
#include "tests/test_registry.h"
#include <vector>
#include <cmath>
#include <cstring>

// Helper: create a minimal valid BenchResult with given name and score
static BenchResult makeResult(const char* name, double score) {
    BenchResult r;
    r.name = name;
    r.unit = "test";
    r.score = score;
    r.avg_ms = 1.0;
    r.valid = true;
    r.sanity_ok = true;
    r.frames = 100;
    return r;
}

TEST_SUITE("CompositeScores") {

TEST_CASE("compositeScores — empty results") {
    std::vector<BenchResult> results;
    CompositeScore cs = computeCompositeScores(results);

    CHECK(cs.overall == 0.0);
    CHECK(cs.fill == 0.0);
    CHECK(cs.geometry == 0.0);
    CHECK(cs.compute == 0.0);
    CHECK(cs.overhead == 0.0);
    CHECK(cs.categories_present == 0);
}

TEST_CASE("compositeScores — single Fill test") {
    std::vector<BenchResult> results;
    results.push_back(makeResult("Fillrate", 100000.0));

    CompositeScore cs = computeCompositeScores(results);

    CHECK(cs.fill > 0.0);
    CHECK(cs.geometry == 0.0);
    CHECK(cs.compute == 0.0);
    CHECK(cs.overhead == 0.0);
    CHECK(cs.categories_present == 1);
    CHECK(cs.fill_norm > 0.0);
}

TEST_CASE("compositeScores — all four categories") {
    std::vector<BenchResult> results;

    // One test from each category (using registry display names)
    results.push_back(makeResult("Fillrate", 100000.0));
    results.push_back(makeResult("Geometry", 5000.0));
    results.push_back(makeResult("ShaderALU", 1000.0));
    results.push_back(makeResult("DrawCall", 20000.0));

    CompositeScore cs = computeCompositeScores(results);

    CHECK(cs.fill > 0.0);
    CHECK(cs.geometry > 0.0);
    CHECK(cs.compute > 0.0);
    CHECK(cs.overhead > 0.0);
    CHECK(cs.categories_present == 4);
    CHECK(cs.overall > 0.0);
}

TEST_CASE("compositeScores — geometric mean of two Fill tests") {
    std::vector<BenchResult> results;
    results.push_back(makeResult("Fillrate", 100.0));
    results.push_back(makeResult("Overdraw", 400.0));

    CompositeScore cs = computeCompositeScores(results);

    // Geometric mean of 100 and 400 = sqrt(100 * 400) = 200
    CHECK(cs.fill == doctest::Approx(200.0).epsilon(0.01));
}

TEST_CASE("compositeScores — zero score ignored in geomean") {
    std::vector<BenchResult> results;
    results.push_back(makeResult("Fillrate", 100.0));
    results.push_back(makeResult("Overdraw", 0.0));  // zero = not run

    CompositeScore cs = computeCompositeScores(results);

    // Only Fillrate contributes, geomean of single value = value itself
    CHECK(cs.fill == doctest::Approx(100.0));
}

} // TEST_SUITE("CompositeScores")

TEST_SUITE("Bottleneck") {

TEST_CASE("detectBottleneck — incomplete data") {
    std::vector<BenchResult> results;
    results.push_back(makeResult("Fillrate", 100000.0));

    CompositeScore cs = computeCompositeScores(results);
    BottleneckInfo info = detectBottleneck(results, cs);

    CHECK(info.detail.find("Incomplete") != std::string::npos);
}

TEST_CASE("detectBottleneck — balanced performance") {
    std::vector<BenchResult> results;
    results.push_back(makeResult("Fillrate", 242895.9));    // = REF_FILL
    results.push_back(makeResult("Geometry", 8717.84));      // = REF_GEOMETRY
    results.push_back(makeResult("ShaderALU", 2257.25));     // = REF_COMPUTE
    results.push_back(makeResult("DrawCall", 30993.39));      // = REF_OVERHEAD

    CompositeScore cs = computeCompositeScores(results);
    BottleneckInfo info = detectBottleneck(results, cs);

    // All at reference → balanced, weakness_ratio should be close to 1.0
    CHECK(info.weakness_ratio > 0.8);
    CHECK(info.detail.find("Balanced") != std::string::npos);
}

TEST_CASE("detectBottleneck — fill is weak") {
    std::vector<BenchResult> results;
    results.push_back(makeResult("Fillrate", 1000.0));         // very low
    results.push_back(makeResult("Geometry", 8717.84));
    results.push_back(makeResult("ShaderALU", 2257.25));
    results.push_back(makeResult("DrawCall", 30993.39));

    CompositeScore cs = computeCompositeScores(results);
    BottleneckInfo info = detectBottleneck(results, cs);

    CHECK(info.weakest_category == "Fill");
    CHECK(info.weakness_ratio < 0.5);
}

TEST_CASE("detectBottleneck — no data") {
    std::vector<BenchResult> results;
    CompositeScore cs;
    BottleneckInfo info = detectBottleneck(results, cs);

    CHECK(info.detail.find("No test data") != std::string::npos);
}

} // TEST_SUITE("Bottleneck")
