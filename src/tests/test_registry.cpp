#include "tests/test_registry.h"
#include "tests/tests.h"
#include "renderer/renderer.h"
#include "renderer/features.h"
#include "renderer/gl2_renderer.h"
#include "renderer/gl3_renderer.h"
#include "renderer/gl4_renderer.h"
#include <type_traits>

const TestInfo g_tests[NUM_TESTS] = {
#define X(id, cls, dname, cname, cat, desc, unit, field, caps) \
    { TestId::id, dname, cname, TestCategory::cat, desc, unit, \
      +[](const BenchPreset& p) -> BenchTest* { return new cls(p.field); }, caps },
#include "tests/test_registry.def"
#undef X
};

// --- Compile-time cap/base class validation ---

static const unsigned GL3_LEVEL_CAPS = Cap_GL3 | Cap_Instancing | Cap_GeometryShader;
// Caps that imply GL4BenchTest (excludes Cap_ImageLoadStore which can pair with ComputeBenchTest)
static const unsigned GL4_LEVEL_CAPS = Cap_GL4 | Cap_Tessellation
    | Cap_BufferStorage | Cap_BindlessTexture;

template<typename TestClass, unsigned Caps>
struct validate_test {
    // Forward: test trait -> caps
    static_assert(
        !test_traits<TestClass>::requires_gl3 || (Caps & GL3_LEVEL_CAPS),
        "Inherits GL3BenchTest but caps don't require GL3-level feature");
    static_assert(
        !test_traits<TestClass>::requires_compute || (Caps & Cap_Compute),
        "Inherits ComputeBenchTest but caps don't require Cap_Compute");
    static_assert(
        !test_traits<TestClass>::requires_gl4 || (Caps & (Cap_GL4 | GL4_LEVEL_CAPS)),
        "Inherits GL4BenchTest but caps don't require GL4-level feature");
    // Reverse: caps -> test trait
    static_assert(
        !(Caps & GL3_LEVEL_CAPS) || test_traits<TestClass>::requires_gl3,
        "Caps require GL3 but test doesn't inherit GL3BenchTest");
    static_assert(
        !(Caps & Cap_Compute) || test_traits<TestClass>::requires_compute,
        "Caps require Compute but test doesn't inherit ComputeBenchTest");
    static_assert(
        !(Caps & GL4_LEVEL_CAPS) || test_traits<TestClass>::requires_gl4,
        "Caps require GL4 but test doesn't inherit GL4BenchTest");
    static const bool valid = true;
};

// --- Structural static_asserts: renderer hierarchy ---

// GL2 provides nothing
static_assert(!renderer_traits<GL2Renderer>::provides_gl3,     "GL2 must not provide GL3");
static_assert(!renderer_traits<GL2Renderer>::provides_gl4,     "GL2 must not provide GL4");
static_assert(!renderer_traits<GL2Renderer>::provides_compute, "GL2 must not provide Compute");

// GL3 provides GL3 only
static_assert( renderer_traits<GL3Renderer>::provides_gl3,     "GL3 must provide GL3");
static_assert(!renderer_traits<GL3Renderer>::provides_gl4,     "GL3 must not provide GL4");
static_assert(!renderer_traits<GL3Renderer>::provides_compute, "GL3 must not provide Compute");

// GL4 provides all
static_assert( renderer_traits<GL4Renderer>::provides_gl3,     "GL4 must provide GL3");
static_assert( renderer_traits<GL4Renderer>::provides_gl4,     "GL4 must provide GL4");
static_assert( renderer_traits<GL4Renderer>::provides_compute, "GL4 must provide Compute");

// Validate all tests at compile time
#define X(id, cls, dname, cname, cat, desc, unit, field, caps) \
    static_assert(validate_test<cls, (caps)>::valid, "Cap/base mismatch: " #id);
#include "tests/test_registry.def"
#undef X

// Test count guard
enum TestCount_ {
    #define X(id, ...) TestCount_##id##_,
    #include "tests/test_registry.def"
    #undef X
    TOTAL_TEST_COUNT_
};
static_assert(TOTAL_TEST_COUNT_ == 30, "Test count changed — update this, presets, CMakeLists");

// --- Runtime functions ---

const char* testCategoryName(TestCategory cat) {
    switch (cat) {
        case TestCategory::Fill:     return "Fill";
        case TestCategory::Geometry: return "Geometry";
        case TestCategory::Compute:  return "Compute";
        case TestCategory::Overhead: return "Overhead";
        case TestCategory::Mixed:    return "Mixed";
        default:                     return "Unknown";
    }
}

uint32_t getAvailableCaps(const Renderer& renderer) {
    const RenderCaps& caps = renderer.getCaps();
    uint32_t c = Cap_None;

    // GL2-level (from RenderCaps -- hardware/driver info)
    if (caps.has_fbo)           c |= Cap_FBO;
    if (caps.has_timer_queries) c |= Cap_TimerQuery;

    // GL3-level: gated by feature interface (structurally safe)
    if (const GL3Features* gl3 = renderer.features<GL3Features>()) {
        if (gl3->hasVAO())             c |= Cap_VAO;
        if (gl3->hasInstancing())      c |= Cap_Instancing;
        if (gl3->hasGeometryShader())  c |= Cap_GeometryShader;
        // Cap_GL3: GL3 renderer with core features
        if (gl3->hasVAO() && gl3->hasTextureArray()) c |= Cap_GL3;
    }

    // Compute: gated by feature interface
    if (const ComputeFeatures* comp = renderer.features<ComputeFeatures>()) {
        if (comp->hasCompute()) c |= Cap_Compute;
    }

    // GL4: tessellation is mandatory in GL 4.0+ — it gates Cap_GL4.
    // Other GL4 features have their own caps for finer-grained checks.
    if (const GL4Features* gl4 = renderer.features<GL4Features>()) {
        if (gl4->hasTessellation())    c |= Cap_GL4 | Cap_Tessellation;
        if (gl4->hasImageLoadStore())  c |= Cap_ImageLoadStore;
        if (gl4->hasBufferStorage())   c |= Cap_BufferStorage;
        if (gl4->hasBindlessTexture()) c |= Cap_BindlessTexture;
    }

    return c;
}
