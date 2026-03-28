#include "demo/shader_cache.h"
#include "demo/shader_loader.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cstring>

ShaderCache::ShaderCache() : renderer_(nullptr), compiles_(0), hits_(0) {}
ShaderCache::~ShaderCache() { destroy(); }

void ShaderCache::init(Renderer* r) {
    renderer_ = r;
    compiles_ = 0;
    hits_ = 0;
}

void ShaderCache::destroy() {
    cache_.clear();
    renderer_ = nullptr;
    if (compiles_ > 0)
        LOG_DBG("ShaderCache: %d compiled, %d cache hits", compiles_, hits_);
}

const char* ShaderCache::versionString(ShaderFeatureSet features) {
    if (features & SF_GLSL_430) return "#version 430\n";
    if (features & SF_GLSL_330) return "#version 330\n";
    if (features & SF_GLSL_150) return "#version 150\n";
    if (features & SF_GLSL_140) return "#version 140\n";
    if (features & SF_GLSL_120) return "#version 120\n";
    return "#version 120\n"; // fallback
}

std::string ShaderCache::buildPreamble(ShaderFeatureSet features, bool is_fragment) const {
    std::string preamble;
    preamble.reserve(512);

    // 1. Version directive
    preamble += versionString(features);

    // 2. Feature #defines
    if (features & SF_GLSL_120)       preamble += "#define GLSL_120\n";
    if (features & SF_SHADOWS)        preamble += "#define HAS_SHADOWS\n";
    if (features & SF_SHADOW_PCF3)    preamble += "#define HAS_SHADOW_PCF3\n";
    if (features & SF_SHADOW_PCF5)    preamble += "#define HAS_SHADOW_PCF5\n";
    if (features & SF_SHADOW_PCSS)    preamble += "#define HAS_SHADOW_PCSS\n";
    if (features & SF_NORMAL_MAP)     preamble += "#define HAS_NORMAL_MAP\n";
    if (features & SF_POINT_LIGHTS)   preamble += "#define HAS_POINT_LIGHTS\n";
    if (features & SF_PBR)            preamble += "#define HAS_PBR\n";
    if (features & SF_SSS)            preamble += "#define HAS_SSS\n";
    if (features & SF_WATER)          preamble += "#define HAS_WATER\n";
    if (features & SF_VIGNETTE)       preamble += "#define HAS_VIGNETTE\n";
    if (features & SF_INSTANCING)     preamble += "#define HAS_INSTANCING\n";
    if (features & SF_PUDDLE_EXCLUDE) preamble += "#define HAS_PUDDLE_EXCLUDE\n";
    if (features & SF_DOMAIN_WARP)    preamble += "#define HAS_DOMAIN_WARP\n";
    if (features & SF_PHYSICAL_SKY)   preamble += "#define HAS_PHYSICAL_SKY\n";
    if (features & SF_FILM_GRAIN)     preamble += "#define HAS_FILM_GRAIN\n";

    // 3. Fragment shader output declaration (non-120 only)
    if (is_fragment && !(features & SF_GLSL_120)) {
        preamble += "out vec4 FragColor;\n";
    }

    // 4. Compatibility macros
    if (features & SF_GLSL_120) {
        preamble += "#define ATTR_IN attribute\n";
        preamble += "#define VS_OUT varying\n";
        preamble += "#define FS_IN varying\n";
        preamble += "#define FRAG_COLOR gl_FragColor\n";
        preamble += "#define COMPAT_TEX2D texture2D\n";
    } else {
        preamble += "#define ATTR_IN in\n";
        preamble += "#define VS_OUT out\n";
        preamble += "#define FS_IN in\n";
        preamble += "#define FRAG_COLOR FragColor\n";
        preamble += "#define COMPAT_TEX2D texture\n";
    }

    return preamble;
}

std::string ShaderCache::loadSource(const char* base_name, const char* extension,
                                    ShaderFeatureSet features, bool is_fragment) const {
    // Load from uber/ directory
    std::string path = std::string("uber/") + base_name + "." + extension;
    std::string raw = ShaderLoader::load(path.c_str());

    if (raw.empty()) {
        LOG_ERR("ShaderCache: failed to load '%s'", path.c_str());
        return std::string();
    }

    // Strip original #version line (if present — uber shaders shouldn't have one,
    // but handle gracefully if they do)
    if (raw.compare(0, 8, "#version") == 0) {
        size_t nl = raw.find('\n');
        if (nl != std::string::npos)
            raw = raw.substr(nl + 1);
    }

    // Prepend preamble
    std::string preamble = buildPreamble(features, is_fragment);
    return preamble + raw;
}

ShaderProgram* ShaderCache::get(const char* base_name,
                                ShaderFeatureSet vert_features,
                                ShaderFeatureSet frag_features) {
    if (!renderer_) return nullptr;

    ProgramKey key;
    key.base_name = base_name;
    key.vert_features = vert_features;
    key.frag_features = frag_features;

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        hits_++;
        return it->second.get();
    }

    // Compile new variant
    std::string vs_src = loadSource(base_name, "vert", vert_features, false);
    std::string fs_src = loadSource(base_name, "frag", frag_features, true);

    if (vs_src.empty() || fs_src.empty())
        return nullptr;

    std::unique_ptr<ShaderProgram> prog(new ShaderProgram());
    if (!prog->create(renderer_, vs_src.c_str(), fs_src.c_str())) {
        LOG_ERR("ShaderCache: failed to compile '%s' (features=0x%08X)",
                base_name, frag_features);
        return nullptr;
    }

    compiles_++;
    LOG_DBG("ShaderCache: compiled '%s' (features=0x%08X), cache size=%d",
            base_name, frag_features, static_cast<int>(cache_.size()) + 1);

    ShaderProgram* raw_ptr = prog.get();
    cache_[key] = std::move(prog);
    return raw_ptr;
}

ShaderProgram* ShaderCache::getCompute(const char* base_name,
                                       ShaderFeatureSet features) {
    if (!renderer_) return nullptr;

    ProgramKey key;
    key.base_name = std::string(base_name) + ".comp";
    key.vert_features = features;
    key.frag_features = 0;

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        hits_++;
        return it->second.get();
    }

    // Load compute source (from gl4/ directory, no uber transform needed)
    std::string path = std::string("gl4/") + base_name + ".comp";
    std::string src = ShaderLoader::load(path.c_str());
    if (src.empty()) return nullptr;

    ComputeFeatures* cf = renderer_->features<ComputeFeatures>();
    if (!cf) return nullptr;

    ShaderHandle sh = cf->createComputeShader(src.c_str());
    if (!sh) return nullptr;

    std::unique_ptr<ShaderProgram> prog(new ShaderProgram());
    prog->adopt(renderer_, sh);

    compiles_++;
    ShaderProgram* raw_ptr = prog.get();
    cache_[key] = std::move(prog);
    return raw_ptr;
}

ShaderProgram* ShaderCache::getTess(const char* base_name,
                                    ShaderFeatureSet features) {
    if (!renderer_) return nullptr;

    ProgramKey key;
    key.base_name = std::string(base_name) + ".tess";
    key.vert_features = features;
    key.frag_features = 0;

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        hits_++;
        return it->second.get();
    }

    GL4Features* g4 = renderer_->features<GL4Features>();
    if (!g4 || !g4->hasTessellation()) return nullptr;

    // Load 4 stages from gl4/
    std::string vs  = ShaderLoader::load((std::string("gl4/") + base_name + ".vert").c_str());
    std::string tcs = ShaderLoader::load((std::string("gl4/") + base_name + ".tcs").c_str());
    std::string tes = ShaderLoader::load((std::string("gl4/") + base_name + ".tes").c_str());
    std::string fs  = ShaderLoader::load((std::string("gl4/") + base_name + ".frag").c_str());

    if (vs.empty() || tcs.empty() || tes.empty() || fs.empty()) return nullptr;

    ShaderHandle sh = g4->createTessShader(vs.c_str(), tcs.c_str(), tes.c_str(), fs.c_str());
    if (!sh) return nullptr;

    std::unique_ptr<ShaderProgram> prog(new ShaderProgram());
    prog->adopt(renderer_, sh);

    compiles_++;
    ShaderProgram* raw_ptr = prog.get();
    cache_[key] = std::move(prog);
    return raw_ptr;
}

// Tier-to-features mapping
ShaderFeatureSet featuresForTier(DemoTier tier, bool core_profile) {
    ShaderFeatureSet f = SF_NONE;

    switch (tier) {
    case DemoTier::Basic:
        f |= core_profile ? SF_GLSL_150 : SF_GLSL_120;
        f |= SF_VIGNETTE;
        break;
    case DemoTier::Enhanced:
        f |= core_profile ? SF_GLSL_150 : SF_GLSL_140;
        f |= SF_SHADOWS | SF_SHADOW_PCF3 | SF_INSTANCING;
        f |= SF_DOMAIN_WARP | SF_FILM_GRAIN;
        break;
    case DemoTier::Quality:
        f |= SF_GLSL_330;
        f |= SF_SHADOWS | SF_SHADOW_PCF5 | SF_NORMAL_MAP | SF_POINT_LIGHTS | SF_INSTANCING;
        f |= SF_DOMAIN_WARP | SF_PHYSICAL_SKY | SF_FILM_GRAIN;
        break;
    case DemoTier::Ultra:
        f |= SF_GLSL_430;
        f |= SF_SHADOWS | SF_SHADOW_PCSS | SF_NORMAL_MAP | SF_POINT_LIGHTS;
        f |= SF_PBR | SF_SSS | SF_WATER | SF_INSTANCING | SF_PUDDLE_EXCLUDE;
        f |= SF_DOMAIN_WARP | SF_PHYSICAL_SKY | SF_FILM_GRAIN;
        break;
    }

    return f;
}
