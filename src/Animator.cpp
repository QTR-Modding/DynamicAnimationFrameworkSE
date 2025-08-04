#include "Animator.h"

RE::BSEventNotifyControl Animator::ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
    RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {

    return RE::BSEventNotifyControl::kContinue;
}