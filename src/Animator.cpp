#include "Animator.h"

RE::BSEventNotifyControl Animator::ProcessEvent(const RE::BSAnimationGraphEvent*,
    RE::BSTEventSource<RE::BSAnimationGraphEvent>*) {

	return RE::BSEventNotifyControl::kContinue;
}