#pragma once

#include <span>
#include <vector>

#include "CLibUtilsQTR/Animations.hpp"
#include "Variables/Types.h"

namespace RE {
    class TESObjectREFR;
}

namespace Variables {
    struct AnimationEntry {
        Animation animation;
        CompiledGroupPtr variables;
    };

    [[nodiscard]] std::vector<Animation> PrepareAnimations(std::span<const AnimationEntry> a_entries,
                                                           RE::TESObjectREFR* a_target);
}
