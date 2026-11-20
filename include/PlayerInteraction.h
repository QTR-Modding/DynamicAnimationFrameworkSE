#pragma once
#include "PlayerActivationHooks.h"
#include "Presets/PresetInterface.h"

namespace Hooks {
    class PlayerInteraction {
    public:
        static void Install(SKSE::Trampoline& a_trampoline);
        static void Animate(DAF_API::AnimEventID a_event, RE::TESObjectREFR* a_actor, RE::TESObjectREFR* a_target);

    private:
        struct ReplayAnimations {
            Presets::AnimData* activation;
            Presets::AnimData* pickup;
        };

        struct Interaction {
            PlayerActivationHooks::Action action;
            bool StartPickup(Presets::AnimData* a_pickup = nullptr) const;
            bool Replay(Presets::AnimData* a_activation, Presets::AnimData* a_pickup) const;
        };

        static RE::TESBoundObject* GetDirectActivationBase(const RE::PlayerCharacter& a_player,
                                                            RE::TESObjectREFR& a_target);
        static bool UsesNativePickup(const RE::TESBoundObject& a_base, std::uintptr_t a_hooked_activate);
        static bool Dispatch(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_target, RE::VR_DEVICE a_device);
        static void Submit(SKSE::TaskInterface::TaskFn a_call);
        static void Drain();

        static inline std::mutex ready_mutex;
        static inline std::vector<SKSE::TaskInterface::TaskFn> ready_calls;
        static inline thread_local ReplayAnimations* replaying{};
    };
}
