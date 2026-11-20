#include "InventorySelection.h"
#include "PlayerActivationHooks.h"
#include "REL/Pattern.h"
#include "Utils.h"

namespace {
    void LogConditionResults(const RE::TESCondition* a_condition, RE::Actor* actor, RE::TESObjectREFR* target) {
        if (!a_condition) {
            return;
        }

        auto a_head = a_condition->head;

        while (a_head) {
            RE::ConditionCheckParams params(actor, target);

            logger::trace("Condition {} = {}", a_head->data.functionData.function.underlying(), a_head->IsTrue(params));

            a_head = a_head->next;
        }
    }
}

RE::StandardItemData* Utils::GetSelectedItemDataInMenu(std::string& a_menuOut) {
    if (const auto ui = RE::UI::GetSingleton()) {
        if (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
            a_menuOut = RE::InventoryMenu::MENU_NAME;
            return GetSelectedItemData<RE::InventoryMenu>();
        }
        if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            a_menuOut = RE::ContainerMenu::MENU_NAME;
            return GetSelectedItemData<RE::ContainerMenu>();
        }
        if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            a_menuOut = RE::BarterMenu::MENU_NAME;
            return GetSelectedItemData<RE::BarterMenu>();
        }
    }
    return nullptr;
}

const char* Utils::GetModelPath(RE::TESForm* a_form, [[maybe_unused]] RE::Actor* a_actor) {
    switch (a_form->GetFormType()) {
        case RE::FormType::Weapon: {
            return a_form->As<RE::TESObjectWEAP>()->GetModel();
        }
        case RE::FormType::Armor: {
            return a_form->As<RE::TESObjectARMO>()->worldModels->GetModel();
        }
        case RE::FormType::Ammo: {
            return a_form->As<RE::TESAmmo>()->GetModel();
        }
        case RE::FormType::KeyMaster: {
            return a_form->As<RE::TESKey>()->GetModel();
        }
        case RE::FormType::Misc: {
            return a_form->As<RE::TESObjectMISC>()->GetModel();
        }
        case RE::FormType::Scroll: {
            return a_form->As<RE::ScrollItem>()->GetModel();
        }
        case RE::FormType::AlchemyItem: {
            return a_form->As<RE::AlchemyItem>()->GetModel();
        }
        case RE::FormType::Ingredient: {
            return a_form->As<RE::IngredientItem>()->GetModel();
        }
        case RE::FormType::Book: {
            return a_form->As<RE::TESObjectBOOK>()->GetModel();
        }
        case RE::FormType::Note: {
            return a_form->As<RE::BGSNote>()->GetModel();
        }
        case RE::FormType::SoulGem: {
            return a_form->As<RE::TESSoulGem>()->GetModel();
        }
        case RE::FormType::Apparatus: {
            return a_form->As<RE::BGSApparatus>()->GetModel();
        }
        case RE::FormType::Container: {
            return a_form->As<RE::TESObjectCONT>()->GetModel();
        }
        case RE::FormType::Door: {
            return a_form->As<RE::TESObjectDOOR>()->GetModel();
        }
        case RE::FormType::Static: {
            return a_form->As<RE::TESObjectSTAT>()->GetModel();
        }
        case RE::FormType::Activator: {
            return a_form->As<RE::TESObjectACTI>()->GetModel();
        }
        case RE::FormType::Light: {
            return a_form->As<RE::TESObjectLIGH>()->GetModel();
        }
        case RE::FormType::Tree: {
            return a_form->As<RE::TESObjectTREE>()->GetModel();
        }
        case RE::FormType::Reference: {
            const auto ref = a_form->AsReference();
            return GetModelPath(ref->GetBaseObject());
        }
        default:
            break;
    }

    return nullptr;
}

bool Utils::GetModel(RE::TESForm* a_form, RE::NiPointer<RE::NiAVObject>& a_out) {
    if (const auto model_path = GetModelPath(a_form)) {
        RE::NiPointer<RE::NiNode> a_model;
        if (const auto res = RE::BSModelDB::Demand(model_path, a_model, {});
            res == RE::BSResource::ErrorCode::kNone) {
            if (RE::NiAVObject* constructedObject = a_model && a_model.get() ? a_model.get() : nullptr) {
                a_out.reset(constructedObject);
                const auto ref = a_form->AsReference();
                const auto form = ref ? ref->GetBaseObject() : a_form;
                const auto armor = form->As<RE::TESObjectARMO>();
                const auto model = armor ? armor->worldModels : form->As<RE::TESModelTextureSwap>();
                if (model && model->numAlternateTextures) {
                    // Keep texture changes off the shared model in BSModelDB.
                    a_out.reset(static_cast<RE::NiAVObject*>(constructedObject->Clone()));
                    model->ApplyAlternateTextures(a_out.get());
                }
                return static_cast<bool>(a_out);
            }
        } else {
            logger::warn("Failed to load model for form {:x}: {}. Error: {}", a_form->GetFormID(), model_path,
                         static_cast<int>(res));
        }
    }
    return false;
}

bool Utils::ShouldSkip([[maybe_unused]] const RE::TESConditionItem* it) {
    return false;
    //const auto func = it->data.functionData.function.get();

    //return func == RE::FUNCTION_DATA::FunctionID::kIsLastHostileActor ||
    //       func == RE::FUNCTION_DATA::FunctionID::kShouldAttackKill ||
    //       func == RE::FUNCTION_DATA::FunctionID::kGetDetected ||
    //       func == RE::FUNCTION_DATA::FunctionID::kGetRandomPercent;
}

bool Utils::EvalConditionsFiltered(const RE::TESCondition& conds, RE::Actor* actor, RE::TESObjectREFR* target) {
    RE::ConditionCheckParams p(actor, target);

    bool overall = true;
    bool blockHasAny = false;
    bool blockValue = false;
    bool prevWasOR = false;

    for (auto it = conds.head; it; it = it->next) {

        const bool v = ShouldSkip(it) ? true : it->IsTrue(p);

        if (!blockHasAny) {
            blockValue = v;
            blockHasAny = true;
        } else {
            if (prevWasOR) {
                blockValue = blockValue || v;
            } else {
                overall = overall && blockValue;
                if (!overall) {
                    return false;
                }
                blockValue = v;
            }
        }

        prevWasOR = it->data.flags.isOR;
    }

    overall = overall && (blockHasAny ? blockValue : true);
    return overall;
}

bool Utils::ParentCheck(const RE::TESIdleForm* idle, RE::Actor* actor, RE::TESObjectREFR* target) {
    return idle && EvalConditionsFiltered(idle->conditions, actor, target);
}

void Utils::CollectIdles(RE::TESIdleForm* parent_idle, std::vector<RE::TESIdleForm*>& out, RE::Actor* actor,
    RE::TESObjectREFR* target, std::unordered_set<RE::TESIdleForm*>& seen) {
    if (!parent_idle || !seen.emplace(parent_idle).second) {
        return;
    }

    CollectIdles(parent_idle->prevIdle, out, actor, target, seen);

    if (!parent_idle->CheckConditions(actor, target, false)) {
#ifndef NDEBUG
        LogConditionResults(&parent_idle->conditions, actor, target);
#endif
        return;
    }

    const bool hasChildren =
        parent_idle->childIdles && parent_idle->childIdles->begin() != parent_idle->childIdles->end();

    if (!hasChildren && (!parent_idle->animEventName.empty() || !parent_idle->animFileName.empty())) {
        out.push_back(parent_idle);
    }

    if (parent_idle->childIdles) {
        for (const auto& childIdle : *parent_idle->childIdles) {
            const auto child = childIdle ? childIdle->As<RE::TESIdleForm>() : nullptr;
            CollectIdles(child, out, actor, target, seen);
        }
    }
}

bool ModCompatibility::ModInfo::IsInstalled() {
    if (!is_checked) {
        constexpr auto plugins_folder = R"(Data\SKSE\Plugins\)";
        const auto mod_path = std::string(plugins_folder) + name + ".dll";
        isLoaded = std::filesystem::exists(mod_path);
        is_checked = true;
    }
    return isLoaded;
}
Hooks::InventorySelection::StackWatch::StackWatch(RE::ExtraDataList* a_list) : list(a_list) {}

Hooks::InventorySelection::StackWatch::~StackWatch() {
    const std::scoped_lock lock(stack_watches_lock);
    const auto found = stack_watches.find(list);
    if (found != stack_watches.end() && found->second.expired()) {
        stack_watches.erase(found);
    }
}

Hooks::InventorySelection::InventorySelection(RE::FormID a_form_id, std::int32_t a_count,
                                              std::vector<StackSelection> a_stacks) :
    form_id(a_form_id), count(a_count), stacks(std::move(a_stacks)) {}

bool Hooks::InventorySelection::Install() {
    const REL::Relocation<std::uintptr_t> destructor{RE::ExtraDataList::DESTRUCTOR};
    const auto bytes = reinterpret_cast<const std::uint8_t*>(destructor.address());
    auto& trampoline = SKSE::GetTrampoline();
    if (trampoline.free_size() < 14) {
        return false;
    }

    if (REL::Module::IsAE()) {
        if (bytes[0] != 0xE9) {
            return false;
        }

        std::int32_t displacement;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        original_destructor = reinterpret_cast<decltype(original_destructor)>(
            destructor.address() + 5 + displacement);
    } else {
        constexpr std::array<std::uint8_t, 16> wrapper_start{
            0x48, 0x83, 0xEC, 0x38, 0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF, 0xB2, 0x01, 0xE8
        };
        constexpr std::array<std::uint8_t, 6> wrapper_end{0x90, 0x48, 0x83, 0xC4, 0x38, 0xC3};
        std::int32_t displacement;
        const auto return_offset = wrapper_start.size() + sizeof(displacement);
        if (std::memcmp(bytes, wrapper_start.data(), wrapper_start.size()) != 0 ||
            std::memcmp(bytes + return_offset, wrapper_end.data(), wrapper_end.size()) != 0) {
            return false;
        }

        std::memcpy(&displacement, bytes + wrapper_start.size(), sizeof(displacement));
        original_cleanup = reinterpret_cast<decltype(original_cleanup)>(
            destructor.address() + return_offset + displacement);
    }

    trampoline.write_branch<5>(destructor.address(), DestroyExtraDataList);
    return true;
}

void Hooks::InventorySelection::DestroyExtraDataList(RE::ExtraDataList* a_list) {
    std::shared_ptr<StackWatch> a_stack;
    {
        const std::scoped_lock lock(stack_watches_lock);
        if (const auto found = stack_watches.find(a_list); found != stack_watches.end()) {
            a_stack = found->second.lock();
            if (a_stack) {
                a_stack->live.store(false);
            }
            stack_watches.erase(found);
        }
    }

    if (original_destructor) {
        original_destructor(a_list);
    } else {
        original_cleanup(a_list, true);
    }
}

std::shared_ptr<Hooks::InventorySelection::StackWatch> Hooks::InventorySelection::WatchStack(
    RE::ExtraDataList* a_list) {
    if (!original_destructor && !original_cleanup) {
        return {};
    }

    std::shared_ptr<StackWatch> a_stack;
    {
        const std::scoped_lock lock(stack_watches_lock);
        auto& a_watch = stack_watches[a_list];
        a_stack = a_watch.lock();
        if (!a_stack) {
            a_stack = std::make_shared<StackWatch>(a_list);
            a_watch = a_stack;
        }
    }
    return a_stack;
}

std::optional<Hooks::InventorySelection> Hooks::InventorySelection::Capture(
    const RE::TESBoundObject& a_item, RE::ExtraDataList* a_extra_list, std::int32_t a_count) {
    std::vector<StackSelection> a_stacks;
    if (a_extra_list) {
        auto a_watch = WatchStack(a_extra_list);
        if (!a_watch) {
            return std::nullopt;
        }
        a_stacks.push_back({std::move(a_watch), a_extra_list->GetCount()});
    }

    return InventorySelection(a_item.GetFormID(), a_count, std::move(a_stacks));
}

std::optional<Hooks::InventorySelection> Hooks::InventorySelection::Capture(
    const RE::InventoryEntryData& a_entry, std::int32_t a_count, RE::TESObjectREFR* a_source) {
    if (!a_entry.object || a_count <= 0 || a_entry.countDelta < a_count) {
        return std::nullopt;
    }

    std::vector<StackSelection> a_stacks;
    if (a_entry.extraLists) {
        for (const auto a_list : *a_entry.extraLists) {
            if (!a_list) {
                break;
            }
            auto a_watch = WatchStack(a_list);
            if (!a_watch) {
                return std::nullopt;
            }
            const auto a_origin = a_list->GetByType<RE::ExtraOriginalReference>();
            a_stacks.push_back({std::move(a_watch), a_list->GetCount(),
                                a_origin ? a_origin->reference : RE::ObjectRefHandle{},
                                a_list->GetWorn(), a_list->IsInventoryStackable(true)});
        }
    }

    InventorySelection selection(a_entry.object->GetFormID(), a_count, std::move(a_stacks));
    if (a_source) {
        const auto a_source_handle = a_source->GetHandle();
        if (!a_source_handle) {
            return std::nullopt;
        }

        const RE::ExtraDataList* a_list = nullptr;
        if (!selection.stacks.empty()) {
            const auto& first_stack = selection.stacks.front();
            if (first_stack.count >= a_count) {
                const auto first_list = first_stack.watch->list;
                if (!first_stack.worn) {
                    a_list = first_list;
                } else if (first_stack.count >= a_entry.countDelta) {
                    return std::nullopt;
                }
            }
        }
        selection.required_stacks.push_back({a_source_handle, a_list, a_count});
    } else {
        auto remaining = a_count;
        for (const auto& a_stack : selection.stacks) {
            if (remaining == 0) {
                break;
            }

            if (!a_stack.origin || a_stack.count <= 0) {
                return std::nullopt;
            }

            const auto a_required_list = a_stack.stackable ? nullptr : a_stack.watch->list;
            const auto a_amount = std::min(remaining, a_stack.count);
            const auto found = std::ranges::find_if(selection.required_stacks, [&](const RequiredStack& a_required) {
                return a_required.owner == a_stack.origin && a_required.list == a_required_list;
            });
            if (found == selection.required_stacks.end()) {
                selection.required_stacks.push_back({a_stack.origin, a_required_list, a_amount});
            } else {
                found->count += a_amount;
            }
            remaining -= a_amount;
        }
        if (remaining != 0) {
            return std::nullopt;
        }
    }

    if (!selection.ValidateSources()) {
        return std::nullopt;
    }
    return selection;
}

bool Hooks::InventorySelection::HasItems(RE::TESObjectREFR& a_source, RE::TESBoundObject& a_item,
                                         const RE::ExtraDataList* a_extra_list, std::int32_t a_count, bool a_for_drop) {
    if (!a_source.GetContainer() && a_source.GetBaseObject() == &a_item) {
        return !a_source.IsDisabled() && !a_source.IsDeleted() && !a_source.IsMarkedForDeletion() &&
               a_source.extraList.GetCount() >= a_count;
    }

    const auto inventory = a_source.GetInventory([&a_item](RE::TESBoundObject& a_candidate) {
        return &a_candidate == &a_item;
    }, true);
    const auto found = inventory.find(&a_item);
    if (found == inventory.end() || found->second.first < a_count) {
        return false;
    }

    const auto a_lists = found->second.second->extraLists;
    if (a_extra_list) {
        if (a_lists) {
            for (const auto a_list : *a_lists) {
                if (a_list == a_extra_list) {
                    return (!a_for_drop || !a_list->HasQuestObjectAlias()) && a_list->GetCount() >= a_count;
                }
            }
        }
        return false;
    }

    std::int64_t available = found->second.first;
    if (a_lists) {
        for (const auto a_list : *a_lists) {
            if (!a_list) {
                continue;
            }
            if (!a_list->IsInventoryStackable(true)) {
                available -= a_list->GetCount();
            } else if (a_for_drop && a_list->HasQuestObjectAlias()) {
                return false;
            }
        }
    }
    return available >= a_count;
}

std::optional<Hooks::InventorySelection::Resolved> Hooks::InventorySelection::ResolveDrop(
    RE::PlayerCharacter& a_player) const {
    const auto a_watch = stacks.empty() ? nullptr : stacks.front().watch.get();
    if (a_watch && !a_watch->live.load()) {
        return std::nullopt;
    }

    const auto a_item = RE::TESForm::LookupByID<RE::TESBoundObject>(form_id);
    const auto a_list = a_watch ? a_watch->list : nullptr;
    if (!a_item || !HasItems(a_player, *a_item, a_list, count, true)) {
        return std::nullopt;
    }
    return Resolved{a_item, a_list, count};
}

bool Hooks::InventorySelection::ValidateSources() const {
    if (std::ranges::any_of(stacks, [](const StackSelection& a_stack) {
        return !a_stack.watch->live.load();
    })) {
        return false;
    }

    const auto a_item = RE::TESForm::LookupByID<RE::TESBoundObject>(form_id);
    if (!a_item) {
        return false;
    }

    for (const auto& a_required : required_stacks) {
        const auto a_owner = a_required.owner.get();
        if (!a_owner || !HasItems(*a_owner, *a_item, a_required.list, a_required.count)) {
            return false;
        }
    }
    return true;
}

bool Hooks::InventorySelection::Matches(const RE::InventoryEntryData& a_entry) const {
    if (!a_entry.object || a_entry.object != RE::TESForm::LookupByID<RE::TESBoundObject>(form_id) ||
        a_entry.countDelta < count ||
        (!stacks.empty() && stacks.front().worn && a_entry.countDelta <= stacks.front().count)) {
        return false;
    }

    auto a_stack = stacks.begin();
    if (a_entry.extraLists) {
        for (const auto a_list : *a_entry.extraLists) {
            if (!a_list) {
                break;
            }
            if (a_stack == stacks.end() || !a_stack->watch->live.load() || a_stack->watch->list != a_list ||
                a_list->GetCount() != a_stack->count || a_list->GetWorn() != a_stack->worn ||
                a_list->IsInventoryStackable(true) != a_stack->stackable) {
                return false;
            }
            const auto a_origin = a_list->GetByType<RE::ExtraOriginalReference>();
            if ((a_origin ? a_origin->reference : RE::ObjectRefHandle{}) != a_stack->origin) {
                return false;
            }
            ++a_stack;
        }
    }
    return a_stack == stacks.end();
}

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
