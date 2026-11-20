#include "PlayerInteraction.h"
#include "Hooks.h"
#include "Manager.h"

bool Hooks::PlayerInteraction::UsesNativePickup(const RE::TESBoundObject& a_base, std::uintptr_t a_hooked_activate) {
    const std::uintptr_t* a_vtable;
    std::memcpy(&a_vtable, &a_base, sizeof(a_vtable));
    const auto a_activate = a_hooked_activate ? a_hooked_activate : a_vtable[activate_vfunc_slot];
    const REL::Relocation<const std::uintptr_t*> bound_vtable{RE::TESBoundObject::VTABLE[0]};
    const auto default_activate = bound_vtable.get()[activate_vfunc_slot];
    const auto text = REL::Module::get().segment(REL::Segment::textx);
    if (a_activate < text.address() || a_activate - text.address() > text.size() - 5) {
        return false;
    }
    if (a_activate == default_activate) {
        return true;
    }

    if (const auto a_light = a_base.As<RE::TESObjectLIGH>()) {
        return a_light->CanBeCarried();
    }
    if (const auto a_book = a_base.As<RE::TESObjectBOOK>()) {
        return a_book->TeachesSpell() && a_book->CanBeTaken();
    }

    if (!a_base.As<RE::TESObjectWEAP>()) {
        return false;
    }
    const auto instruction = reinterpret_cast<const std::uint8_t*>(a_activate);
    if (*instruction != 0xE9) {
        return false;
    }
    std::int32_t a_displacement;
    std::memcpy(&a_displacement, instruction + 1, sizeof(a_displacement));
    return a_activate + 5 + a_displacement == default_activate;
}

void Hooks::PlayerInteraction::Install(SKSE::Trampoline& a_trampoline) {
    PlayerActivationHooks::Install(a_trampoline, Dispatch, Drain);
}

RE::TESBoundObject* Hooks::PlayerInteraction::GetDirectActivationBase(const RE::PlayerCharacter& a_player,
                                                                         RE::TESObjectREFR& a_target) {
    if (a_target.IsActivationBlocked() || a_target.extraList.HasType<RE::ExtraOpenCloseActivateRef>() ||
        a_player.HasPerkEntries(RE::BGSEntryPoint::ENTRY_POINTS::kActivate)) {
        return nullptr;
    }
    if (const auto a_flags = a_target.extraList.GetByType<RE::ExtraFlags>();
        a_flags && a_flags->flags.any(RE::ExtraFlags::Flag::kBlockPlayerActivate)) {
        return nullptr;
    }
    constexpr std::uint8_t requires_activation_parent = 1;
    if (const auto a_parents = a_target.extraList.GetByType<RE::ExtraActivateRef>();
        a_parents && (a_parents->activateFlags & requires_activation_parent)) {
        return nullptr;
    }

    const auto a_base = a_target.GetBaseObject();
    if (a_base && a_base->Is(RE::FormType::Door) && (a_target.formFlags & RE::TESObjectREFR::RecordFlags::kInaccessible)) {
        return nullptr;
    }
    return a_base;
}

bool Hooks::PlayerInteraction::Dispatch(RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_target,
                                         RE::VR_DEVICE a_device) {
    const auto a_base = a_player ? GetDirectActivationBase(*a_player, *a_target) : nullptr;
    if (!a_base) {
        return false;
    }
    const auto a_hooked_activate = GetHookedActivation(*a_base);
    const bool uses_pickup = UsesNativePickup(*a_base, a_hooked_activate);
    if (!a_hooked_activate && !uses_pickup) {
        return false;
    }

    const auto a_action = PlayerActivationHooks::Capture(a_player, a_target, a_device);
    if (!a_action || !a_action->ReadyTarget()) {
        return false;
    }

    const Interaction interaction{*a_action};
    const auto manager = Manager::GetSingleton();
    auto a_activation = a_hooked_activate ? manager->SelectAnimation(Presets::kActivate, a_player, a_target) :
                                           Presets::AnimData{};
    if (!a_activation.animations.empty() && a_activation.delay > 0) {
        const auto delay = manager->PlayAnimation({Presets::kActivate, a_player, a_target}, std::move(a_activation));
        if (delay > 0) {
            CallDelayed(delay, [interaction] { interaction.StartPickup(); }, Submit);
            return true;
        }
        a_activation = {};
    }

    auto a_pickup = uses_pickup ? manager->SelectAnimation(Presets::kItemPickup, a_player, a_target) :
                                 Presets::AnimData{};
    if (!a_pickup.animations.empty() && a_pickup.delay > 0) {
        manager->PlayAnimation({Presets::kActivate, a_player, a_target}, std::move(a_activation));
        return interaction.StartPickup(&a_pickup);
    }
    return interaction.Replay(a_hooked_activate ? &a_activation : nullptr, uses_pickup ? &a_pickup : nullptr);
}

bool Hooks::PlayerInteraction::Interaction::StartPickup(Presets::AnimData* a_pickup) const {
    const auto a_target = action.ReadyTarget();
    if (!a_target) {
        return false;
    }

    Presets::AnimData a_activation_finished{};
    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto a_base = GetDirectActivationBase(*player, *a_target);
    if (!a_base || !UsesNativePickup(*a_base, GetHookedActivation(*a_base))) {
        return Replay(&a_activation_finished, nullptr);
    }
    const auto manager = Manager::GetSingleton();
    auto a_selected = a_pickup ? std::move(*a_pickup) : manager->SelectAnimation(Presets::kItemPickup, player, a_target.get());
    if (!a_selected.animations.empty() && a_selected.delay > 0) {
        const auto delay = manager->PlayAnimation({Presets::kItemPickup, player, a_target.get()}, std::move(a_selected));
        a_selected = {};
        if (delay > 0) {
            CallDelayed(delay, [interaction = *this] {
                Presets::AnimData a_finished{};
                interaction.Replay(&a_finished, &a_finished);
            }, Submit);
            return true;
        }
    }
    return Replay(&a_activation_finished, &a_selected);
}

bool Hooks::PlayerInteraction::Interaction::Replay(Presets::AnimData* a_activation, Presets::AnimData* a_pickup) const {
    ReplayAnimations a_animations{a_activation, a_pickup};
    const auto a_previous = std::exchange(replaying, &a_animations);
    const SKSE::stl::scope_exit a_restore{[a_previous]() noexcept { replaying = a_previous; }};
    return action.Replay();
}

void Hooks::PlayerInteraction::Animate(DAF_API::AnimEventID a_event, RE::TESObjectREFR* a_actor,
                                      RE::TESObjectREFR* a_target) {
    const auto manager = Manager::GetSingleton();
    if (replaying && PlayerActivationHooks::IsReplaying(a_actor, a_target)) {
        auto& a_queued = a_event == Presets::kActivate ? replaying->activation : replaying->pickup;
        if (const auto a_animation = std::exchange(a_queued, nullptr)) {
            manager->PlayAnimation({a_event, a_actor, a_target}, std::move(*a_animation));
            return;
        }
    }
    manager->PlayAnimation({a_event, a_actor, a_target});
}

void Hooks::PlayerInteraction::Submit(SKSE::TaskInterface::TaskFn a_call) {
    std::lock_guard lock(ready_mutex);
    ready_calls.push_back(std::move(a_call));
}

void Hooks::PlayerInteraction::Drain() {
    std::vector<SKSE::TaskInterface::TaskFn> a_calls;
    {
        std::lock_guard lock(ready_mutex);
        a_calls.swap(ready_calls);
    }
    for (const auto& a_call : a_calls) {
        a_call();
    }
}
