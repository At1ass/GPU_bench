#include "demo/tier/shader_bank.h"
#include "renderer/renderer.h"
#include "platform/logger.h"

// Static metadata table generated from shader_registry.def
namespace {
    // NONE sentinel for upgrade field (no upgrade available)
    #define NONE ShaderBank::COUNT

    struct ShaderMeta {
        const char* base_name;
        ShaderBank::Type type;
        int min_tier;
        ShaderBank::Id upgrade;  // T4 GL4 shader that replaces this one, or COUNT
    };

    static const ShaderMeta kMeta[] = {
        #define SHADER(id, base, stype, tier, upg) \
            { base, ShaderBank::stype, static_cast<int>(DemoTier::tier), ShaderBank::upg },
        #include "demo/tier/shader_registry.def"
        #undef SHADER
    };

    #undef NONE
}

ShaderBank::ShaderBank() : renderer_(nullptr) {
    for (int i = 0; i < COUNT; i++)
        for (int t = 0; t < MAX_TIERS; t++)
            programs_[i][t] = nullptr;
}

ShaderBank::~ShaderBank() { destroy(); }

int ShaderBank::minTier(Id id)            { return kMeta[id].min_tier; }
ShaderBank::Type ShaderBank::type(Id id)  { return kMeta[id].type; }
const char* ShaderBank::baseName(Id id)   { return kMeta[id].base_name; }

bool ShaderBank::compile(Renderer* r, int max_tier) {
    renderer_ = r;
    cache_.init(r, kDemoFeatures, kDemoFeatureCount);

    bool core_profile = r->isCoreProfile();
    bool is_gles = r->isGLES();
    bool gles3 = r->isGLES3();

    for (int id = 0; id < COUNT; id++) {
        const ShaderMeta& m = kMeta[id];

        if (m.type == Uber) {
            // Compile per-tier variants up to max_tier
            for (int t = m.min_tier; t <= max_tier && t <= MAX_TIERS; t++) {
                ShaderFeatureSet feat = featuresForTier(
                    static_cast<DemoTier>(t), core_profile, is_gles, gles3);
                ShaderProgram* prog = cache_.get(m.base_name, feat, feat);
                if (prog) {
                    programs_[id][t - 1] = prog;
                } else {
                    LOG_WRN("ShaderBank: failed to compile uber '%s' tier %d",
                            m.base_name, t);
                }
            }
        } else if (m.min_tier <= max_tier) {
            // GL4/Compute/Tess: compile once, store in slot 0
            ShaderProgram* prog = nullptr;
            switch (m.type) {
            case GL4:
                prog = cache_.getGL4(m.base_name);
                break;
            case Compute:
                prog = cache_.getCompute(m.base_name, SF_GLSL_430);
                break;
            case Tess:
                prog = cache_.getTess(m.base_name, SF_GLSL_430);
                break;
            default:
                break;
            }
            if (prog) {
                programs_[id][0] = prog;
            } else {
                LOG_WRN("ShaderBank: failed to compile '%s' (%s)",
                        m.base_name,
                        m.type == GL4 ? "GL4" : m.type == Compute ? "Compute" : "Tess");
            }
        }
    }

    // Critical: at least T1 must have sky + island shaders
    return programs_[Sky][0] != nullptr && programs_[Island][0] != nullptr;
}

ShaderProgram* ShaderBank::get(Id id, DemoTier tier) const {
    int ti = static_cast<int>(tier) - 1;
    if (ti < 0 || ti >= MAX_TIERS) return nullptr;

    // T4 upgrade: if at Ultra tier and an upgrade shader exists, prefer it
    if (tier == DemoTier::Ultra && kMeta[id].upgrade != COUNT) {
        ShaderProgram* upgraded = programs_[kMeta[id].upgrade][0];
        if (upgraded) return upgraded;
    }

    if (kMeta[id].type == Uber) {
        // Return tier-specific variant, fallback to lower tiers
        for (int t = ti; t >= 0; t--) {
            if (programs_[id][t]) return programs_[id][t];
        }
        return nullptr;
    }
    // GL4/Compute/Tess: single entry in slot 0
    return programs_[id][0];
}

ShaderProgram* ShaderBank::get(Id id) const {
    return get(id, DemoTier::Ultra);
}

int ShaderBank::validateTier(int max_tier) const {
    // Critical shaders per tier that must exist
    // T1: sky, island, fur, particle
    // T2: + shadow_depth, bloom (3), grass, ssao (2)
    // T3: + torch
    // T4: + all GL4/compute/tess shaders
    static const Id t1_critical[] = { Sky, Island, Fur, Particle };

    for (int t = max_tier; t >= 1; t--) {
        DemoTier tier = static_cast<DemoTier>(t);
        bool valid = true;
        for (int i = 0; i < 4; i++) {
            if (!get(t1_critical[i], tier)) {
                LOG_WRN("ShaderBank: tier %d missing '%s'", t, kMeta[t1_critical[i]].base_name);
                valid = false;
            }
        }
        if (valid) return t;
    }
    return 0;
}

void ShaderBank::destroy() {
    for (int i = 0; i < COUNT; i++)
        for (int t = 0; t < MAX_TIERS; t++)
            programs_[i][t] = nullptr;
    cache_.destroy();
    renderer_ = nullptr;
}
