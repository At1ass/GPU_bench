#pragma once

class BenchTest;
struct BenchPreset;

// Test identity enum — generated from test_registry.def
enum class TestId {
#define X(id, cls, dname, cname, cat, desc, unit, field) id,
#include "test_registry.def"
#undef X
    Count
};

// Scoring/aggregation categories
enum class TestCategory {
    Fill,       // Fillrate, Overdraw, Texturing
    Geometry,   // Geometry, Vertex
    Compute,    // ShaderALU, ShaderFMA
    Overhead,   // DrawCall, DrawCallRaw, StateChange, TexUpload
    Mixed,      // Scene (excluded from composite)
    Count
};

// Factory function: creates a test from preset parameters
using TestFactoryFn = BenchTest*(*)(const BenchPreset&);

// Metadata for one test — populated from test_registry.def
struct TestInfo {
    TestId       id;
    const char*  display_name;  // UI label ("Fillrate")
    const char*  cli_name;      // CLI filter name ("fillrate")
    TestCategory category;
    const char*  description;
    const char*  unit;          // Score unit ("MPix/s")
    TestFactoryFn factory;      // Creates test instance (caller owns pointer)
};

// Total number of registered tests
static constexpr int NUM_TESTS = static_cast<int>(TestId::Count);

// Canonical test table (defined in test_registry.cpp)
extern const TestInfo g_tests[NUM_TESTS];

// Category name for display
const char* testCategoryName(TestCategory cat);
