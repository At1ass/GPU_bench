#include "engine/shader_loader.h"
#include "platform/data_path.h"
#include "platform/logger.h"
#include <cstring>
#include <set>

static std::string processIncludes(const std::string& source, std::set<std::string>& included) {
    std::string result;
    result.reserve(source.size());

    size_t pos = 0;
    while (pos < source.size()) {
        size_t eol = source.find('\n', pos);
        if (eol == std::string::npos) eol = source.size();

        std::string line = source.substr(pos, eol - pos);

        const char* pragma = "#pragma include \"";
        size_t pi = line.find(pragma);
        if (pi != std::string::npos) {
            size_t name_start = pi + strlen(pragma);
            size_t name_end = line.find('"', name_start);
            if (name_end != std::string::npos) {
                std::string include_name = line.substr(name_start, name_end - name_start);
                if (included.count(include_name)) {
                    // Already included — skip (prevents circular includes)
                    pos = (eol < source.size()) ? eol + 1 : source.size();
                    continue;
                }
                included.insert(include_name);
                std::string include_path = getDataPath(("shaders/common/" + include_name).c_str());
                if (!include_path.empty()) {
                    LOG_DBG("Shader: include '%s' resolved to '%s'", include_name.c_str(), include_path.c_str());
                    std::string inc_content = readTextFile(include_path.c_str());
                    result += processIncludes(inc_content, included);
                    result += '\n';
                } else {
                    LOG_ERR("Shader include not found: %s", include_name.c_str());
                }
                pos = (eol < source.size()) ? eol + 1 : source.size();
                continue;
            }
        }

        result += line;
        result += '\n';
        pos = (eol < source.size()) ? eol + 1 : source.size();
    }

    return result;
}

std::string ShaderLoader::load(const char* relative_path) {
    std::string full_path = getDataPath((std::string("shaders/") + relative_path).c_str());

    if (!full_path.empty()) {
        std::string content = readTextFile(full_path.c_str());
        if (!content.empty()) {
            std::set<std::string> included;
            return processIncludes(content, included);
        }
    }

    LOG_ERR("Shader file not found: %s", relative_path);
    return std::string();
}
