#include "Variables/AnimationIntegration.h"
#include "Variables/Evaluator.h"

namespace Variables {
    void PrepareAnimations(std::vector<Animation>& a_animations, std::vector<CompiledGroupPtr> a_groups,
                           RE::TESObjectREFR* a_target) {
        RE::ObjectRefHandle targetHandle;
        const bool targetExpected = a_target != nullptr;
        if (targetExpected) {
            targetHandle = a_target->GetHandle();
        }

        for (std::size_t i = 0; i < a_animations.size(); ++i) {
            auto& group = a_groups[i];
            if (!group) continue;

            a_animations[i].before_play = [group = std::move(group), targetHandle, targetExpected](
                                        RE::Actor* a_subject, const Animation&) {
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
    }
}
