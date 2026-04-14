#include "engine/shader_cache.h"
#include "engine/shader_loader.h"
#include "renderer/features.h"
#include "platform/logger.h"
#include <cstring>

ShaderCache::ShaderCache() : renderer_(nullptr), defines_(nullptr), define_count_(0), compiles_(0), hits_(0) {}
ShaderCache::~ShaderCache() { destroy(); }

void ShaderCache::init(Renderer* r, const FeatureDefine* defines, int define_count) {
    renderer_ = r;
    defines_ = defines;
    define_count_ = define_count;
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
    if (features & SF_GLES_300)  return "#version 300 es\n";
    if (features & SF_GLES_100)  return "#version 100\n";
    if (features & SF_GLSL_430)  return "#version 430\n";
    if (features & SF_GLSL_330)  return "#version 330\n";
    if (features & SF_GLSL_150)  return "#version 150\n";
    if (features & SF_GLSL_140)  return "#version 140\n";
    if (features & SF_GLSL_120)  return "#version 120\n";
    return "#version 120\n"; // fallback
}

std::string ShaderCache::buildPreamble(ShaderFeatureSet features, bool is_fragment) const {
    std::string preamble;
    preamble.reserve(512);

    bool is_gles = (features & (SF_GLES_100 | SF_GLES_300)) != 0;
    bool uses_legacy_syntax = (features & (SF_GLSL_120 | SF_GLES_100)) != 0;

    // 1. Version directive
    preamble += versionString(features);

    // 2. Version-derived compat defines (engine-level)
    if (uses_legacy_syntax)  preamble += "#define GLSL_120\n";
    if (is_gles)             preamble += "#define GLES\n";

    // 3. Application feature #defines (data-driven from table passed at init)
    for (int i = 0; i < define_count_; i++) {
        if (features & defines_[i].mask) {
            preamble += "#define ";
            preamble += defines_[i].define;
            preamble += '\n';
        }
    }

    // 3. GLES precision qualifiers (required by spec, must come after #version).
    //    Use highp for both VS and FS to avoid precision mismatch on shared uniforms.
    if (is_gles) {
        preamble += "precision highp float;\n";
    }

    // 4. Fragment shader output declaration
    //    GLSL 120 and GLES 100 use gl_FragColor; all others declare FragColor.
    if (is_fragment && !uses_legacy_syntax) {
        preamble += "out vec4 FragColor;\n";
    }

    // 5. Compatibility macros
    if (uses_legacy_syntax) {
        // GLSL 1.20 / GLES 2.0: attribute, varying, texture2D, gl_FragColor
        preamble += "#define ATTR_IN attribute\n";
        preamble += "#define VS_OUT varying\n";
        preamble += "#define FS_IN varying\n";
        preamble += "#define FRAG_COLOR gl_FragColor\n";
        preamble += "#define COMPAT_TEX2D texture2D\n";
    } else {
        // GLSL 1.50+ / GLES 3.0+: in, out, texture, FragColor
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

std::string ShaderCache::prepareSource(const char* raw, ShaderFeatureSet features, bool is_fragment) const {
    std::string src(raw);
    if (src.compare(0, 8, "#version") == 0) {
        size_t nl = src.find('\n');
        if (nl != std::string::npos)
            src = src.substr(nl + 1);
    }
    return buildPreamble(features, is_fragment) + src;
}

ShaderProgram* ShaderCache::compileInline(const char* key_name,
                                           const char* vs_source, const char* fs_source,
                                           ShaderFeatureSet features) {
    if (!renderer_) return nullptr;

    ProgramKey key;
    key.base_name = std::string(key_name) + ".inline";
    key.vert_features = features;
    key.frag_features = features;

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        hits_++;
        return it->second.get();
    }

    std::string vs = prepareSource(vs_source, features, false);
    std::string fs = prepareSource(fs_source, features, true);

    std::unique_ptr<ShaderProgram> prog(new ShaderProgram());
    if (!prog->create(renderer_, vs.c_str(), fs.c_str())) {
        LOG_ERR("ShaderCache: failed to compile inline '%s' (features=0x%08X)",
                key_name, features);
        return nullptr;
    }

    compiles_++;
    ShaderProgram* raw_ptr = prog.get();
    cache_[key] = std::move(prog);
    return raw_ptr;
}

ShaderProgram* ShaderCache::getGL4(const char* base_name) {
    if (!renderer_) return nullptr;

    ProgramKey key;
    key.base_name = std::string(base_name) + ".gl4";
    key.vert_features = 0;
    key.frag_features = 0;

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        hits_++;
        return it->second.get();
    }

    std::string vs = ShaderLoader::load((std::string("gl4/") + base_name + ".vert").c_str());
    std::string fs = ShaderLoader::load((std::string("gl4/") + base_name + ".frag").c_str());
    if (vs.empty() || fs.empty()) return nullptr;

    std::unique_ptr<ShaderProgram> prog(new ShaderProgram());
    if (!prog->create(renderer_, vs.c_str(), fs.c_str())) {
        LOG_ERR("ShaderCache: failed to compile GL4 '%s'", base_name);
        return nullptr;
    }

    compiles_++;
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

// featuresForTier() moved to demo/tier/demo_tier_config.cpp (demo-specific)
