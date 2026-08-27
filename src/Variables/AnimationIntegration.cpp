#include "Variables/AnimationIntegration.h"

#include "Variables/Evaluator.h"

namespace Variables {
    std::vector<Animation> PrepareAnimations(const std::span<const AnimationEntry> a_entries,
                                             RE::TESObjectREFR* a_target) {
        RE::ObjectRefHandle targetHandle;
        const bool targetExpected = a_target != nullptr;
        if (targetExpected) {
            targetHandle = a_target->GetHandle();
        }

        std::vector<Animation> animations;
        animations.reserve(a_entries.size());
        for (const auto& entry : a_entries) {
            auto animation = entry.animation;
            if (entry.variables) {
                animation.before_play = [group = entry.variables, targetHandle, targetExpected](RE::Actor* a_subject,
                                                                                                const Animation&) {
                    RE::TESObjectREFRPtr target;
                    if (targetExpected) {
                        if (!targetHandle) {
                            LogFailure(*group, {}, "target reference handle was invalid when the animation was queued");
                            return false;
                        }
                        target = targetHandle.get();
                        if (!target) {
                            LogFailure(*group, {}, "target reference handle expired before the animation was played");
                            return false;
                        }
                    }
                    return EvaluateAndWrite(*group, a_subject, target.get());
                };
            }
            animations.push_back(std::move(animation));
        }
        return animations;
    }
}
