#pragma once

#include "Variables/Types.h"

namespace RE {
    class TESObjectREFR;
}

namespace Variables {
    [[nodiscard]] bool EvaluateAndWrite(const CompiledGroup& a_group, RE::TESObjectREFR* a_subject,
                                        RE::TESObjectREFR* a_target) noexcept;
}
