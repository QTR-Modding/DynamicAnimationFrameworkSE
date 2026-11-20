#include "PlayerActivationHooks.h"
#include "REL/Pattern.h"

void Hooks::PlayerActivationHooks::Install(SKSE::Trampoline& a_trampoline, Dispatcher a_dispatch, Drain a_drain) {
    const REL::Relocation<std::uintptr_t> activate{RE::PlayerCharacter::ACTIVATE_PICK_REF};
    const REL::Relocation<std::uintptr_t> button_call{
        RE::ActivateHandler::PROCESS_BUTTON, RE::ActivateHandler::ACTIVATE_PICK_REF_CALL_OFFSET};
    const REL::Relocation<std::uintptr_t> poll_call{RE::Main::UPDATE, RE::Main::GetPollInputDevicesCallOffset()};
    const REL::Relocation<std::uintptr_t> poll{RE::BSInputDeviceManager::POLL_INPUT_DEVICES};
    const auto touchpad_call = REL::Module::IsVR() ? RE::ActivateHandler::VR_TOUCHPAD_ACTIVATE_PICK_REF_CALL.address() : 0;

    if (!IsDirectCallTo(button_call.address(), activate.address()) ||
        (touchpad_call && !IsDirectCallTo(touchpad_call, activate.address())) ||
        !IsDirectCallTo(poll_call.address(), poll.address())) {
        logger::warn("Skyrim's player activation or input polling call was changed. Player activation delay is unavailable.");
        return;
    }

    const auto target_read = activate.address() + RE::PlayerCharacter::ACTIVATE_PICK_REF_TARGET_READ_OFFSET.offset();
    if (!InstallTargetRead(a_trampoline, target_read)) {
        logger::warn("Skyrim's player activation target read was changed. Player activation delay is unavailable.");
        return;
    }

    dispatch = a_dispatch;
    drain = a_drain;
    if (touchpad_call) {
        original_activate_vr = a_trampoline.write_call<5>(button_call.address(), ActivatePickRefVR);
        a_trampoline.write_call<5>(touchpad_call, ActivatePickRefVR);
    } else {
        original_activate = a_trampoline.write_call<5>(button_call.address(), ActivatePickRef);
    }
    original_poll = a_trampoline.write_call<5>(poll_call.address(), PollInputDevices);
}

bool Hooks::PlayerActivationHooks::IsDirectCallTo(std::uintptr_t a_call, std::uintptr_t a_target) {
    const auto instruction = reinterpret_cast<const std::uint8_t*>(a_call);
    if (*instruction != 0xE8) {
        return false;
    }
    std::int32_t displacement;
    std::memcpy(&displacement, instruction + 1, sizeof(displacement));
    const auto next_instruction = static_cast<std::intptr_t>(a_call + 5);
    return static_cast<std::uintptr_t>(next_instruction + displacement) == a_target;
}

bool Hooks::PlayerActivationHooks::InstallTargetRead(SKSE::Trampoline& a_trampoline, std::uintptr_t a_address) {
    constexpr std::array<std::uint8_t, 10> desktop_se_call{
        0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCF, 0x90, 0x90};
    constexpr std::array<std::uint8_t, 10> desktop_ae_call{
        0xE8, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xD0, 0x48, 0x8B, 0xCF};
    constexpr std::array<std::uint8_t, 12> vr_device_call{
        0x41, 0x8B, 0xCC, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCF, 0x90};

    constexpr auto desktop_se_read = REL::make_pattern<"48 8B 05 ?? ?? ?? ?? 8B 40 04">();
    constexpr auto desktop_ae_read = REL::make_pattern<"48 8B 05 ?? ?? ?? ?? 8B 50 04">();
    constexpr auto vr_device_read = REL::make_pattern<"48 8B 05 ?? ?? ?? ?? 42 8B 44 A0 04">();

    std::span<const std::uint8_t> a_call = desktop_se_call;
    std::size_t call_offset = 0;
    if (REL::Module::IsVR()) {
        if (!vr_device_read.match(a_address)) {
            return false;
        }
        a_call = vr_device_call;
        call_offset = 3;
    } else if (REL::Module::IsAE()) {
        if (!desktop_ae_read.match(a_address)) {
            return false;
        }
        a_call = desktop_ae_call;
    } else if (!desktop_se_read.match(a_address)) {
        return false;
    }

    std::array<std::uint8_t, vr_device_call.size()> original;
    std::memcpy(original.data(), reinterpret_cast<const void*>(a_address), a_call.size());

    constexpr std::size_t indirect_jump_size = 6;
    std::array<std::uint8_t, indirect_jump_size + sizeof(std::uintptr_t)> target_reader_jump{0xFF, 0x25};
    const auto target_reader = reinterpret_cast<std::uintptr_t>(ReadActivatePickRefTarget);
    std::memcpy(target_reader_jump.data() + indirect_jump_size, &target_reader, sizeof(target_reader));

    const auto a_relay = a_trampoline.allocate(target_reader_jump.size());
    const auto next_instruction = static_cast<std::intptr_t>(a_address + call_offset + 5);
    const auto relative_offset = reinterpret_cast<std::intptr_t>(a_relay) - next_instruction;
    if (!std::in_range<std::int32_t>(relative_offset)) {
        return false;
    }
    std::memcpy(a_relay, target_reader_jump.data(), target_reader_jump.size());

    std::array<std::uint8_t, vr_device_call.size()> replacement;
    std::ranges::copy(a_call, replacement.begin());
    const auto displacement = static_cast<std::int32_t>(relative_offset);
    std::memcpy(replacement.data() + call_offset + 1, &displacement, sizeof(displacement));
    return REL::safe_write(a_address, replacement.data(), a_call.size(), original.data(), a_call.size());
}

std::optional<Hooks::PlayerActivationHooks::Action> Hooks::PlayerActivationHooks::Capture(
    RE::PlayerCharacter* a_player, RE::TESObjectREFR* a_target, RE::VR_DEVICE a_device) {
    const auto a_player_handle = a_player->GetHandle();
    const auto a_target_handle = a_target->GetHandle();
    if (!a_player_handle || !a_target_handle) {
        return std::nullopt;
    }
    return Action{a_player_handle, a_target_handle, a_device};
}

RE::TESObjectREFRPtr Hooks::PlayerActivationHooks::Action::ReadyTarget() const {
    if (!in_main_input_pass || replay || !RE::ControlMap::GetSingleton()->IsActivateControlsEnabled() ||
        RE::UI::GetSingleton()->GameIsPaused()) {
        return {};
    }

    const auto camera = RE::PlayerCamera::GetSingleton();
    if (camera && camera->currentState == (REL::Module::IsVR() ?
        camera->GetVRRuntimeData().cameraStates[RE::CameraState::kPCTransition] :
        camera->GetRuntimeData().cameraStates[RE::CameraState::kPCTransition])) {
        return {};
    }

    const auto a_player = player.get();
    const auto a_target = target.get();
    const auto player_character = RE::PlayerCharacter::GetSingleton();
    if (!a_player || a_player.get() != player_character || player_character->IsDead() || !a_target ||
        a_target->IsDisabled() || a_target->IsDeleted() || a_target->IsMarkedForDeletion() || !a_target->Is3DLoaded()) {
        return {};
    }
    return a_target;
}

bool Hooks::PlayerActivationHooks::Action::Replay() const {
    const auto a_target = ReadyTarget();
    if (!a_target) {
        return false;
    }

    const auto player_character = RE::PlayerCharacter::GetSingleton();
    ReplayContext a_context{player_character, a_target.get(), target.native_handle()};
    replay = &a_context;
    const SKSE::stl::scope_exit clear_replay{[]() noexcept { replay = nullptr; }};
    if (REL::Module::IsVR()) {
        original_activate_vr(player_character, device);
    } else {
        original_activate(player_character);
    }
    return true;
}

bool Hooks::PlayerActivationHooks::IsReplaying(const RE::TESObjectREFR* a_player, const RE::TESObjectREFR* a_target) {
    return replay && replay->player == a_player && replay->target == a_target;
}

bool Hooks::PlayerActivationHooks::Dispatch(RE::PlayerCharacter* a_player, RE::VR_DEVICE a_device) {
    if (!in_main_input_pass || replay) {
        return false;
    }

    const auto a_target = RE::CrosshairPickData::GetSingleton()->GetTarget(a_device).get();
    return a_target && dispatch(a_player, a_target.get(), a_device);
}

void Hooks::PlayerActivationHooks::ActivatePickRef(RE::PlayerCharacter* a_player) {
    if (!Dispatch(a_player, RE::VR_DEVICE::kNone)) {
        original_activate(a_player);
    }
}

void Hooks::PlayerActivationHooks::ActivatePickRefVR(RE::PlayerCharacter* a_player, RE::VR_DEVICE a_device) {
    if (!Dispatch(a_player, a_device)) {
        original_activate_vr(a_player, a_device);
    }
}

RE::ObjectRefHandle::native_handle_type Hooks::PlayerActivationHooks::ReadActivatePickRefTarget(
    RE::VR_DEVICE a_device) noexcept {
    if (replay && replay->target_handle) {
        return *std::exchange(replay->target_handle, std::nullopt);
    }
    return RE::CrosshairPickData::GetSingleton()->GetTarget(a_device).native_handle();
}

void Hooks::PlayerActivationHooks::PollInputDevices(RE::BSInputDeviceManager* a_input, float a_delta) {
    const auto was_in_input_pass = std::exchange(in_main_input_pass, true);
    const SKSE::stl::scope_exit restore_input_state{[was_in_input_pass]() noexcept { in_main_input_pass = was_in_input_pass; }};
    original_poll(a_input, a_delta);
    if (!was_in_input_pass) {
        drain();
    }
}
