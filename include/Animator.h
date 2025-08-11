#pragma once
#include <queue>
#include <shared_mutex>
#include "CLibUtilsQTR/Ticker.hpp"
#include "DynamicAnimationFramework/API.hpp"

struct Animation {
	RE::TESIdleForm* a_idle=nullptr;
    std::string anim_name;
	unsigned int t_wait_ms=0;
};

class Animator final :
public Ticker,
public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;

	static bool SendAnimationEvent(RE::Actor* a_actor, const char* AnimationString);

    bool PlayAnimation(const char* a_animation);

    void PlayIdle(RE::TESIdleForm* a_idle, RE::TESObjectREFR* a_target=nullptr) const;

    void UpdateLoop();

    std::queue<std::pair<Animation,DAF_API::AnimEventID>> m_AnimQueue;
    std::shared_mutex animQ_mutex;

	RE::ActorHandlePtr actor;

public:
    explicit Animator(RE::ActorHandlePtr a_actor);

	void ClearQueue();

    bool Add2Q(const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain);
};