#include <doctest.h>
#include "demo/tier/shader_cache.h"
#include "demo/tier/shader_feature.h"
#include "engine/resource_id.h"

// Version flags — exactly one must be set
static const uint32_t VERSION_MASK =
    SF_GLSL_120 | SF_GLSL_150 | SF_GLSL_140 | SF_GLSL_330 | SF_GLSL_430 |
    SF_GLES_100 | SF_GLES_300;

static int countBits(uint32_t v) {
    int c = 0;
    while (v) { c += v & 1; v >>= 1; }
    return c;
}

TEST_SUITE("ShaderFeatures") {

TEST_CASE("ShaderFeatures: each tier has exactly one version flag") {
    for (int t = 1; t <= 4; t++) {
        ShaderFeatureSet f = featuresForTier(static_cast<DemoTier>(t), false, false, false);
        uint32_t ver = f & VERSION_MASK;
        CHECK(countBits(ver) == 1);
    }
}

TEST_CASE("ShaderFeatures: Basic always has SF_VIGNETTE") {
    ShaderFeatureSet f = featuresForTier(DemoTier::Basic, false, false, false);
    CHECK((f & SF_VIGNETTE) != 0);
}

TEST_CASE("ShaderFeatures: Ultra has PBR, SSS, WATER") {
    ShaderFeatureSet f = featuresForTier(DemoTier::Ultra, false, false, false);
    CHECK((f & SF_PBR) != 0);
    CHECK((f & SF_SSS) != 0);
    CHECK((f & SF_WATER) != 0);

    // Lower tiers should NOT have PBR
    ShaderFeatureSet basic = featuresForTier(DemoTier::Basic, false, false, false);
    CHECK((basic & SF_PBR) == 0);
    ShaderFeatureSet enhanced = featuresForTier(DemoTier::Enhanced, false, false, false);
    CHECK((enhanced & SF_PBR) == 0);
}

TEST_CASE("ShaderFeatures: higher tiers have more features (desktop)") {
    // Higher tiers should have >= feature count (not strict subset — some may change)
    for (int t = 1; t < 4; t++) {
        ShaderFeatureSet lo = featuresForTier(static_cast<DemoTier>(t), false, false, false);
        ShaderFeatureSet hi = featuresForTier(static_cast<DemoTier>(t + 1), false, false, false);
        uint32_t lo_feat = lo & ~VERSION_MASK;
        uint32_t hi_feat = hi & ~VERSION_MASK;
        CHECK(countBits(hi_feat) >= countBits(lo_feat));
    }
}

TEST_CASE("ShaderFeatures: GLES 2.0 caps at Basic") {
    ShaderFeatureSet f = featuresForTier(DemoTier::Enhanced, false, true, false);
    // GLES 2.0 can only do Basic, so Enhanced should still be Basic-level
    CHECK((f & SF_GLES_100) != 0);
    CHECK((f & SF_SHADOWS) == 0);
}

TEST_CASE("ShaderFeatures: GLES 3.0 version flag") {
    ShaderFeatureSet f = featuresForTier(DemoTier::Basic, false, true, true);
    CHECK((f & SF_GLES_300) != 0);
}

} // TEST_SUITE("ShaderFeatures")
