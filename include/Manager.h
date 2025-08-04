#pragma once
#include <shared_mutex>
#include "Animator.h"
#include "ClibUtil/singleton.hpp"
#include "Presets/PresetInterface.h"

class Manager : public clib_util::singleton::ISingleton<Manager>
{

	bool PlayAnimation(RE::Actor* a_actor, const std::vector<Animation>& anim_chain);
	int PlayAnimation(Presets::AnimEvent a_animevent, RE::TESObjectREFR* a_actor, RE::TESForm* a_form);

    static bool ActorCheck(const RE::Actor* a_actor);

	struct ActorHandleHash {
	    size_t operator() (const RE::ActorHandlePtr& actor_ptr) const noexcept;
    };

	struct ActorHandleEqual {
	    bool operator() (const RE::ActorHandlePtr& lhs, const RE::ActorHandlePtr& rhs) const noexcept;
    };

    std::shared_mutex m_animators_;
	std::unordered_map<RE::ActorHandlePtr,Animator*,ActorHandleHash,ActorHandleEqual> animators;

	struct Filter {
	    RE::FormID actor_id;
	    RE::TESForm* form;
	};

    static Presets::AnimData GetAnimData(Presets::AnimEvent a_animevent, const Filter& filter);

public:

	void PauseAnimators();
	void ResumeAnimators();

	int OnActivate(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnPickup(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnDrop(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnItemAdd(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnItemRemove(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnEquip(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnUnequip(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnBuy(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnSell(RE::TESObjectREFR* a_actor, RE::TESBoundObject* a_item);
	int OnMenuOpenClose(std::string_view menu_name, bool opened);
	int OnItemHover(std::string_view menu_name, const RE::StandardItemData* a_item_data);
};

