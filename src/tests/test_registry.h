#pragma once
#include <cstdint>
#include <type_traits>

class BenchTest;
class Renderer;
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
    Cap_GL4             = 1 << 6,
    Cap_GeometryShader  = 1 << 7,
    Cap_Tessellation    = 1 << 8,
    Cap_ImageLoadStore  = 1 << 9,
    Cap_BufferStorage   = 1 << 10,
    Cap_BindlessTexture = 1 << 11,
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

// Caps — power of 2
#define ASSERT_CAP_POW2(cap) \
    static_assert((cap) != 0 && ((cap) & ((cap) - 1)) == 0, #cap " must be power of 2")
ASSERT_CAP_POW2(Cap_FBO);
ASSERT_CAP_POW2(Cap_VAO);
ASSERT_CAP_POW2(Cap_Instancing);
ASSERT_CAP_POW2(Cap_Compute);
ASSERT_CAP_POW2(Cap_TimerQuery);
ASSERT_CAP_POW2(Cap_GL3);
ASSERT_CAP_POW2(Cap_GL4);
ASSERT_CAP_POW2(Cap_GeometryShader);
ASSERT_CAP_POW2(Cap_Tessellation);
ASSERT_CAP_POW2(Cap_ImageLoadStore);
ASSERT_CAP_POW2(Cap_BufferStorage);
ASSERT_CAP_POW2(Cap_BindlessTexture);
#undef ASSERT_CAP_POW2

// All cap bits fit in uint32_t
static_assert(Cap_BindlessTexture < (1u << 31), "Caps overflow uint32_t");

// Category name for display
const char* testCategoryName(TestCategory cat);

// Convert renderer capabilities to TestCap bitmask
uint32_t getAvailableCaps(const Renderer& renderer);
