#pragma once

namespace Variables::Providers::detail {
    enum class ParamKind : std::uint8_t { kInt, kFloat, kForm, kUnsupported };

    struct ParamTypeInfo {
        RE::SCRIPT_PARAM_TYPE type;
        ParamKind kind;
        std::string_view name;
    };

    [[nodiscard]] const ParamTypeInfo* GetParamTypeInfo(RE::SCRIPT_PARAM_TYPE a_type) noexcept;
    [[nodiscard]] std::string GetParamTypeName(RE::SCRIPT_PARAM_TYPE a_type);
    [[nodiscard]] void* ResolveFormPointer(RE::SCRIPT_PARAM_TYPE a_type, RE::TESForm* a_form) noexcept;
}