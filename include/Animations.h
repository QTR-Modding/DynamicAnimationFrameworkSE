#pragma once
#include "ClibUtil/singleton.hpp"
#include "CLibUtilsQTR/Animations.hpp"
#include "Hooks.h"

namespace Animations
{
	class MyAnimator : 
		public Animator,
		public clib_util::singleton::ISingleton<MyAnimator>
	{
        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                              RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override {
			return RE::BSEventNotifyControl::kContinue;
		}

		void RemoveSink(const RE::Actor* a_actor) {a_actor->RemoveAnimationGraphEventSink(this);}

	};
}