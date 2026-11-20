#pragma once

namespace Hooks {
    class PlayerActivationHooks {
    public:
        using Dispatcher = bool (*)(RE::PlayerCharacter*, RE::TESObjectREFR*, RE::VR_DEVICE);
        using Drain = void (*)();

        struct Action {
            RE::ObjectRefHandle player;
            RE::ObjectRefHandle target;
            RE::VR_DEVICE device;

            RE::TESObjectREFRPtr ReadyTarget() const;
            bool Replay() const;
        };

        static void Install(SKSE::Trampoline& a_trampoline, Dispatcher a_dispatch, Drain a_drain);
        static std::optional<Action> Capture(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_target,
                                             RE::VR_DEVICE a_device);
        static bool IsReplaying(const RE::TESObjectREFR* a_player, const RE::TESObjectREFR* a_target);

    private:
        struct ReplayContext {
            const RE::TESObjectREFR* player;
            const RE::TESObjectREFR* target;
            std::optional<RE::ObjectRefHandle::native_handle_type> target_handle;
        };

        static bool InstallTargetRead(SKSE::Trampoline& a_trampoline, std::uintptr_t a_address);
        static bool IsDirectCallTo(std::uintptr_t a_call, std::uintptr_t a_target);
        static bool Dispatch(RE::PlayerCharacter* a_player, RE::VR_DEVICE a_device);
        static void ActivatePickRef(RE::PlayerCharacter* a_player);
        static void ActivatePickRefVR(RE::PlayerCharacter* a_player, RE::VR_DEVICE a_device);
        static RE::ObjectRefHandle::native_handle_type ReadActivatePickRefTarget(RE::VR_DEVICE a_device) noexcept;
        static void PollInputDevices(RE::BSInputDeviceManager* a_input, float a_delta);

        static inline REL::Relocation<decltype(ActivatePickRef)*> original_activate;
        static inline REL::Relocation<decltype(ActivatePickRefVR)*> original_activate_vr;
        static inline REL::Relocation<decltype(PollInputDevices)*> original_poll;
        static inline Dispatcher dispatch;
        static inline Drain drain;
        static inline thread_local bool in_main_input_pass{};
        static inline thread_local ReplayContext* replay{};
    };
}
