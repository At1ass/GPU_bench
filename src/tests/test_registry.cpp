#include "tests/test_registry.h"
#include "tests/tests.h"
#include "renderer/renderer.h"

const TestInfo g_tests[NUM_TESTS] = {
#define X(id, cls, dname, cname, cat, desc, unit, field, caps) \
    { TestId::id, dname, cname, TestCategory::cat, desc, unit, \
      +[](const BenchPreset& p) -> BenchTest* { return new cls(p.field); }, caps },
#include "tests/test_registry.def"
#undef X
};

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

uint32_t getAvailableCaps(const RenderCaps& caps) {
    uint32_t c = Cap_None;
    if (caps.has_fbo)           c |= Cap_FBO;
    if (caps.has_vao)           c |= Cap_VAO;
    if (caps.has_instancing)    c |= Cap_Instancing;
    if (caps.has_compute)       c |= Cap_Compute;
    if (caps.has_timer_queries) c |= Cap_TimerQuery;
    if (caps.gl_major >= 3)     c |= Cap_GL3;
    if (caps.gl_major >= 4)     c |= Cap_GL4;
    return c;
}
