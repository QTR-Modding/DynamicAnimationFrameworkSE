#pragma once

#include "Variables/Types.h"

namespace Variables {
    void LogFailure(const CompiledGroup& a_group, std::string_view a_definition, std::string_view a_reason) noexcept;

    [[nodiscard]] bool EvaluateAndWrite(const CompiledGroup& a_group, RE::TESObjectREFR* a_subject,
                                        RE::TESObjectREFR* a_target) noexcept;
}
