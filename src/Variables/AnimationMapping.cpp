#include "Variables/AnimationMapping.h"
#include "Variables/Compiler.h"


namespace Variables {
    bool AnimationMappingCompiler::Compile(const std::vector<std::string>& a_variables,
                                           const std::filesystem::path& a_animationFile,
                                           const std::size_t a_animationCount, std::vector<CompiledGroupPtr>& a_groups,
                                           std::string& a_error) {
        a_error.clear();
        a_groups.clear();
        if (a_variables.empty()) return true;
        if (a_variables.size() > a_animationCount) {
            a_error = "variables cannot contain more entries than animations";
            return false;
        }

        for (std::size_t i = 0; i < a_variables.size(); ++i) {
            const auto& groupName = a_variables[i];
            if (groupName.empty()) continue;
            if (a_groups.empty()) {
                a_groups.resize(a_animationCount);
            }

            auto groupPath = ResolveGroupPath(a_animationFile, groupName, a_error).lexically_normal();
            if (groupPath.empty()) return false;

            if (const auto it = cache.find(groupPath); it != cache.end()) {
                a_groups[i] = it->second;
                continue;
            }

            auto group = CompileFile(groupPath, a_error);
            if (!group) return false;
            a_groups[i] = group;
            cache.emplace(std::move(groupPath), std::move(group));
        }
        return true;
    }
}
