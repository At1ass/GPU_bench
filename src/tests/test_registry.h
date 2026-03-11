#pragma once
#include <cstdint>

class BenchTest;
struct BenchPreset;
struct RenderCaps;

// Hardware capability flags for test requirements
enum TestCap : uint32_t {
    Cap_None       = 0,
    Cap_FBO        = 1 << 0,
    Cap_VAO        = 1 << 1,
    Cap_Instancing = 1 << 2,
    Cap_Compute    = 1 << 3,
    Cap_TimerQuery = 1 << 4,
    Cap_GL3        = 1 << 5,
    Cap_GL4        = 1 << 6,
};

// Test identity enum — generated from test_registry.def
enum class TestId {
#define X(id, cls, dname, cname, cat, desc, unit, field, caps) id,
#include "tests/test_registry.def"
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
    uint32_t     required_caps; // Bitmask of TestCap flags
};

// Total number of registered tests
static constexpr int NUM_TESTS = static_cast<int>(TestId::Count);

// Canonical test table (defined in test_registry.cpp)
extern const TestInfo g_tests[NUM_TESTS];

// Category name for display
const char* testCategoryName(TestCategory cat);

// Convert RenderCaps to TestCap bitmask
uint32_t getAvailableCaps(const RenderCaps& caps);
