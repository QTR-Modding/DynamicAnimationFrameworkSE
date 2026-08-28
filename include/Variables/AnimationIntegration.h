#pragma once

#include "CLibUtilsQTR/Animations.hpp"
#include "Variables/Types.h"

namespace Variables {
    struct AnimationEntry {
        Animation animation;
        CompiledGroupPtr variables;
    };

    [[nodiscard]] std::vector<Animation> PrepareAnimations(std::vector<AnimationEntry> a_entries,
                                                           RE::TESObjectREFR* a_target);
}
