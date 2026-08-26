#pragma once

#include <rapidjson/document.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "Variables/Types.h"

namespace Variables {
    [[nodiscard]] CompiledGroupPtr CompileGroup(const rapidjson::Value& a_root, std::string_view a_context,
                                                std::string& a_error) noexcept;

    [[nodiscard]] CompiledGroupPtr CompileFile(const std::filesystem::path& a_path, std::string& a_error) noexcept;

    [[nodiscard]] bool IsSafeGroupName(std::string_view a_name, std::string& a_error) noexcept;

    [[nodiscard]] std::filesystem::path ResolveGroupPath(const std::filesystem::path& a_animationFile,
                                                         std::string_view a_groupName, std::string& a_error) noexcept;
}
