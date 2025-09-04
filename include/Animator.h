#pragma once
#include "CLibUtilsQTR/Animations.hpp"
#include "DynamicAnimationFramework/API.hpp"

class MyAnimator final :
public Animator
{
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;

public:
    explicit MyAnimator(const RE::ActorHandlePtr& a_actor) : Animator(a_actor) {}

    bool Add2Q(const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain);
};