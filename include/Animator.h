#pragma once
#include <queue>
#include <shared_mutex>
#include "CLibUtilsQTR/Ticker.hpp"
#include "DynamicAnimationFramework/API.h"

struct Animation {
	RE::TESIdleForm* a_idle=nullptr;
    std::string anim_name;
	unsigned int t_wait_ms=0;
};

class Animator:
public Ticker,
public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
    RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
                                          RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;

	static bool SendAnimationEvent(RE::Actor* a_actor, const char* AnimationString)
    {
        if (const auto animGraphHolder = static_cast<RE::IAnimationGraphManagerHolder*>(a_actor)) {
            if (animGraphHolder->NotifyAnimationGraph(AnimationString)) {
			    return true;
            }
		    return false;
        } 
	    return false;
    }

	bool PlayAnimation(const char* a_animation) {
	    if (const auto a_actor = actor.get()) {
	        a_actor->AddAnimationGraphEventSink(this);
	        return SendAnimationEvent(a_actor, a_animation);
	    }
		return false;
	}

	void PlayIdle(RE::TESIdleForm* a_idle, RE::TESObjectREFR* a_target=nullptr) const {
	    if (const auto a_actor = actor.get()) {
			if (const auto current_process = a_actor->GetActorRuntimeData().currentProcess) {
                current_process->PlayIdle(a_actor,a_idle,a_target);
			}
	    }
	}

    void UpdateLoop() {
		std::unique_lock lock(animQ_mutex);

		Stop();
		if (m_AnimQueue.empty()) {
			UpdateInterval(std::chrono::milliseconds(0));
			return;
		}

		auto [a_idle, a_anim, t_wait_ms] = m_AnimQueue.front().first;
		m_AnimQueue.pop();
		UpdateInterval(std::chrono::milliseconds(t_wait_ms));
		if (a_idle) {
			SKSE::GetTaskInterface()->AddTask([this, a_idle]() {
				PlayIdle(a_idle);
				Start();
				});
		}
		else if (!a_anim.empty()) {
			SKSE::GetTaskInterface()->AddTask([this,a_anim]() {
				if (PlayAnimation(a_anim.c_str())) {
					Start();
				}
				else {
					Stop();
					UpdateInterval(std::chrono::milliseconds(10));
					Start();
				}
			});
		}
		else {
			Start();
		}
    }

	std::queue<std::pair<Animation,DAF_API::AnimEventID>> m_AnimQueue;
    std::shared_mutex animQ_mutex;

	RE::ActorHandlePtr actor;

public:
    explicit Animator(RE::ActorHandlePtr a_actor) : Ticker([this]() { UpdateLoop(); },std::chrono::milliseconds(0)), actor(std::move(
                                                        a_actor)) {}

	void ClearQueue() {
        Stop();
	    UpdateInterval(std::chrono::milliseconds(0));
	    std::unique_lock lock(animQ_mutex);
	    m_AnimQueue = std::queue<std::pair<Animation,DAF_API::AnimEventID>>();
    }

	bool Add2Q(const std::pair<DAF_API::AnimEventID, std::vector<Animation>>& anim_chain) {
		auto& [anim_event_id, animations]= anim_chain;
		if (animations.empty()) {
			return false;
		}


        std::unique_lock lock(animQ_mutex);
		if (!m_AnimQueue.empty() && m_AnimQueue.front().second == anim_event_id) {
			return false;
		}
		for (const auto& anim : animations) {
			m_AnimQueue.push({anim,anim_event_id});
        }

		Start();
        return true;
	}
};