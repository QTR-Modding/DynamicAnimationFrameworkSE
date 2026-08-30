#include "Variables/AnimationMapping.h"
#include "Variables/Compiler.h"

namespace Variables {
    bool AnimationMappingCompiler::Compile(const rapidjson::Value& a_document,
                                           const std::filesystem::path& a_animationFile,
                                           const std::size_t a_animationCount, std::vector<CompiledGroupPtr>& a_groups,
                                           std::string& a_error) {
        a_error.clear();
        a_groups.assign(a_animationCount, nullptr);
        if (!a_document.IsObject()) return true;

        const rapidjson::Value* variables = nullptr;
        for (auto it = a_document.MemberBegin(); it != a_document.MemberEnd(); ++it) {
            if (std::string_view(it->name.GetString(), it->name.GetStringLength()) == "variables") {
                if (variables) {
                    a_error = "duplicate top-level variables field";
                    return false;
                }
                variables = &it->value;
            }
        }
        if (!variables) return true;

        if (!variables->IsArray() || variables->Size() != a_animationCount) {
            a_error = "variables must be an array with exactly one entry per animation";
            return false;
        }

        for (rapidjson::SizeType i = 0; i < variables->Size(); ++i) {
            const auto& value = (*variables)[i];
            if (value.IsNull()) continue;
            if (!value.IsString()) {
                a_error = "variables entries must be a bare group name or null";
                return false;
            }

            const std::string_view groupName(value.GetString(), value.GetStringLength());
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