#include "test_registry.h"
#include "tests.h"

const TestInfo g_tests[NUM_TESTS] = {
#define X(id, cls, dname, cname, cat, desc, unit, field) \
    { TestId::id, dname, cname, TestCategory::cat, desc, unit, \
      +[](const BenchPreset& p) -> BenchTest* { return new cls(p.field); } },
#include "test_registry.def"
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
