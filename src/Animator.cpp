#include "Animator.h"

RE::BSEventNotifyControl MyAnimator::ProcessEvent(const RE::BSAnimationGraphEvent*,
    RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {

	return RE::BSEventNotifyControl::kContinue;
}

bool MyAnimator::Add2Q(const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain) {
    auto& [anim_event_id, animations]= anim_chain;
    if (animations.empty()) {
        return false;
    }


    std::unique_lock lock(animQ_mutex);
    if (!m_AnimQueue.empty() && m_AnimQueue.front().anim_id == anim_event_id) {
        return false;
    }

    for (auto anim : animations) {
		anim.anim_id = anim_event_id;
        m_AnimQueue.push({anim});
    }

    Start();
    return true;
}

