#pragma once

#include "Variables/Types.h"

namespace Variables {
    class AnimationMappingCompiler {
    public:
        [[nodiscard]] bool Compile(const std::vector<std::string>& a_variables,
                                   const std::filesystem::path& a_animationFile, std::size_t a_animationCount,
                                   std::vector<CompiledGroupPtr>& a_groups, std::string& a_error);

    private:
        std::unordered_map<std::filesystem::path, CompiledGroupPtr> cache;
    };
}