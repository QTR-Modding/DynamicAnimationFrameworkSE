#pragma once

#include "CLibUtilsQTR/Animations.hpp"
#include "Variables/Types.h"

namespace Variables {
    void PrepareAnimations(std::vector<Animation>& a_animations, std::vector<CompiledGroupPtr> a_groups,
                           const std::vector<std::optional<std::size_t>>& a_durationIndices,
                           RE::TESObjectREFR* a_target);
}
