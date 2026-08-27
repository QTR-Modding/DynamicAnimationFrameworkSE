#pragma once

#include <rapidjson/document.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Variables/Types.h"

namespace Variables {
    class AnimationMappingCompiler {
    public:
        [[nodiscard]] bool Compile(const rapidjson::Value& a_document, const std::filesystem::path& a_animationFile,
                                   std::size_t a_animationCount, std::vector<CompiledGroupPtr>& a_groups,
                                   std::string& a_error);

    private:
        std::unordered_map<std::filesystem::path, CompiledGroupPtr> cache;
    };
}
