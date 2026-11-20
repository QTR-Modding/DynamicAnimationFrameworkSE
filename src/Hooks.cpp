#include "Hooks.h"
#include "InventoryActionHooks.h"
#include "InventorySelection.h"
#include "Manager.h"
#include "PlayerInteraction.h"
#include "Utils.h"

namespace {
    class HarvestEventSink final : public RE::BSTEventSink<RE::TESHarvestedEvent::ItemHarvested> {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHarvestedEvent::ItemHarvested* a_event,
                                              RE::BSTEventSource<RE::TESHarvestedEvent::ItemHarvested>*) override {
            if (a_event && a_event->harvester && a_event->produceItem) {
                Manager::GetSingleton()->OnHarvest(a_event->harvester, a_event->produceItem);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    HarvestEventSink harvest_event_sink;
}

using namespace Utils;

void Hooks::CallDelayed(int a_delay, SKSE::TaskInterface::TaskFn a_task, DelayedCallDispatcher a_dispatch) {
    const auto a_generation = delayed_call_generation.load();
    clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        [a_generation, a_task = std::move(a_task), a_dispatch]() mutable {
            if (a_generation != delayed_call_generation.load()) {
                return;
            }
            auto run = [a_generation, a_task = std::move(a_task)] {
                if (a_generation == delayed_call_generation.load()) {
                    a_task();
                }
            };
            if (a_dispatch) {
                a_dispatch(std::move(run));
            } else {
                SKSE::GetTaskInterface()->AddUITask(std::move(run));
            }
        },
        a_delay);
}

void Hooks::CancelDelayedCalls() {
    ++delayed_call_generation;
}

void Hooks::Install() {

    MoveItemHooks<RE::PlayerCharacter>::install();
    MoveItemHooks<RE::TESObjectREFR>::install(false);
    MoveItemHooks<RE::Character>::install();
    MagicEffectHooks::Install();
    RE::TESHarvestedEvent::GetEventSource()->AddEventSink(&harvest_event_sink);

    MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0]);
    MenuHook<RE::InventoryMenu>::InstallHook(RE::VTABLE_InventoryMenu[0]);
    MenuHook<RE::BarterMenu>::InstallHook(RE::VTABLE_BarterMenu[0]);
    MenuHook<RE::MagicMenu>::InstallHook(RE::VTABLE_MagicMenu[0]);
    MenuHook<RE::FavoritesMenu>::InstallHook(RE::VTABLE_FavoritesMenu[0]);
    MenuHook<RE::MapMenu>::InstallHook(RE::VTABLE_MapMenu[0]);
    MenuHook<RE::JournalMenu>::InstallHook(RE::VTABLE_JournalMenu[0]);

    auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
    trampoline.create(size_per_hook * 12);
    InventoryActionHooks::Install(trampoline);

    GenericEquipObjectHook::InstallHook(trampoline);
    UnEquipObjectHook::InstallHook(trampoline);

    /*const REL::Relocation<std::uintptr_t> add_item_functor_hook{ RELOCATION_ID(55946, 56490) };
    add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);*/

    const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
    InventoryHoverHook::originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c),
                                                                    InventoryHoverHook::thunk);

    const REL::Relocation<std::uintptr_t> target{REL::RelocationID(42420, 43576), REL::Relocate(0x22A, 0x21F)};
    // AnimationObjects::Load
    AnimObjectHook::_LoadAnimObject = trampoline.write_call<5>(target.address(), AnimObjectHook::thunk);

    const REL::Relocation<std::uintptr_t> target2{REL::RelocationID(75461, 77246)}; // BSGraphics::Renderer::End
    DrawHook::func = trampoline.write_call<5>(target2.address() + 0x9, DrawHook::thunk);

    WndProcHook::Install();

    []<class... Forms>(std::tuple<Forms*...>*) {
        (ActivateHook<Forms>::install(), ...);
    }(static_cast<ActivationForms*>(nullptr));
    PlayerInteraction::Install(trampoline);
}

template <class MenuType>
void Hooks::InventoryActionHooks::MenuAction<MenuType>::Install(SKSE::Trampoline& a_trampoline) {
    constexpr auto action = is_barter ? RE::BarterMenu::ITEM_SELECT : RE::ContainerMenu::ITEM_TRANSFER;
    const REL::Relocation<std::uintptr_t> a_selection_call{action, MenuType::GET_SELECTED_ITEM_CALL_OFFSET};
    if (*reinterpret_cast<const std::uint8_t*>(a_selection_call.address()) != 0xe8) {
        logger::warn("{} item selection call has already been changed.", MenuType::MENU_NAME);
        return;
    }

    original_action = REL::Relocation<RE::FxDelegateHandler::CallbackFn*>{action};
    get_selected_item = a_trampoline.write_call<5>(a_selection_call.address(), GetSelectedItem);
    REL::Relocation<std::uintptr_t> vtable{MenuType::VTABLE[0]};
    original_accept = vtable.write_vfunc(0x1, Accept);
}

template <class MenuType>
void Hooks::InventoryActionHooks::MenuAction<MenuType>::Accept(MenuType* a_menu,
                                                  RE::FxDelegateHandler::CallbackProcessor* a_processor) {
    Registration registration{a_processor};
    original_accept(a_menu, &registration);
}

template <class MenuType>
Hooks::InventoryActionHooks::MenuAction<MenuType>::Registration::Registration(
    RE::FxDelegateHandler::CallbackProcessor* a_processor) : processor(a_processor) {}

template <class MenuType>
void Hooks::InventoryActionHooks::MenuAction<MenuType>::Registration::Process(
    const RE::GString& a_name, RE::FxDelegateHandler::CallbackFn* a_method) {
    constexpr auto action_name = is_barter ? "ItemSelect" : "ItemTransfer";
    const bool intercept = std::string_view{a_name.c_str()} == action_name &&
        reinterpret_cast<std::uintptr_t>(a_method) == original_action.address();
    processor->Process(a_name, intercept ? Queue : a_method);
}

template <class MenuType>
RE::ItemList::Item* Hooks::InventoryActionHooks::MenuAction<MenuType>::GetSelectedItem(RE::ItemList* a_list) {
    if (replay.list == a_list && replay.item) {
        return std::exchange(replay.item, nullptr);
    }
    return get_selected_item(a_list);
}

template <class MenuType>
RE::ContainerMenu::ContainerMode Hooks::InventoryActionHooks::MenuAction<MenuType>::GetContainerMode() {
    if constexpr (std::is_same_v<MenuType, RE::ContainerMenu>) {
        return RE::ContainerMenu::GetContainerMode();
    }
    return RE::ContainerMenu::ContainerMode::kLoot;
}

template <class MenuType>
void Hooks::InventoryActionHooks::MenuAction<MenuType>::Queue(const RE::FxDelegateArgs& a_args) {
    constexpr std::uint32_t direction_arg = is_barter ? 2 : 1;
    if (a_args.GetArgCount() <= direction_arg || !a_args[0].IsNumber() || !a_args[direction_arg].IsBool() ||
        (is_barter && !a_args[1].IsNumber())) {
        return original_action(a_args);
    }

    const double quantity = a_args[0].GetNumber();
    if (!std::isfinite(quantity) || quantity < 1 || quantity > std::numeric_limits<std::int32_t>::max()) {
        return original_action(a_args);
    }

    const auto a_menu = static_cast<MenuType*>(a_args.GetHandler());
    const auto a_list = a_menu->GetRuntimeData().itemList;
    const auto a_row = a_list ? get_selected_item(a_list) : nullptr;
    const auto a_entry = a_row ? a_row->data.objDesc : nullptr;
    const auto a_item = a_entry ? a_entry->object : nullptr;
    if (!a_item) {
        return original_action(a_args);
    }

    const bool from_other = a_args[direction_arg].GetBool();
    if constexpr (!is_barter) {
        if (!from_other) {
            return original_action(a_args);
        }
    }

    const auto player = RE::PlayerCharacter::GetSingleton();
    const auto count = static_cast<std::int32_t>(quantity);
    const auto target = MenuType::GetTargetRefHandle();
    const double price = is_barter ? a_args[1].GetNumber() : 0;
    std::int64_t total = 0;
    if constexpr (is_barter) {
        if (!std::isfinite(price) || price < 0 || price > std::numeric_limits<std::int32_t>::max()) {
            return original_action(a_args);
        }
        total = static_cast<std::int64_t>(count) * static_cast<std::int32_t>(price);
        if (total > std::numeric_limits<std::int32_t>::max() ||
            (from_other && total > a_menu->GetRuntimeData().playerGold)) {
            return original_action(a_args);
        }
    }

    const auto event = is_barter ? (from_other ? Presets::kBuy : Presets::kSell) : Presets::kItemAdd;
    const auto form_id = a_item->GetFormID();
    auto manager = Manager::GetSingleton();
    int delay = 0;
    switch (event) {
    case Presets::kBuy:
        delay = manager->OnBuy(player, a_item);
        if (delay <= 0) {
            delay = manager->OnItemAdd(player, a_item);
        }
        break;
    case Presets::kSell:
        delay = manager->OnSell(player, a_item);
        break;
    case Presets::kItemAdd:
        delay = manager->OnItemAdd(player, a_item);
        break;

    default:
        break;
    }

    const auto previous = std::exchange(InventoryActionHooks::started_animation, {form_id, event});
    const SKSE::stl::scope_exit restore{[previous]() noexcept {
        InventoryActionHooks::started_animation = previous;
    }};
    if (delay <= 0) {
        return original_action(a_args);
    }

    RE::TESObjectREFRPtr a_container;
    if constexpr (!is_barter) {
        if (!RE::LookupReferenceByHandle(target, a_container)) {
            return original_action(a_args);
        }
    }
    const auto a_source = from_other ? a_container.get() : player;
    auto a_selection = InventorySelection::Capture(*a_entry, count, a_source);
    if (!a_selection) {
        return original_action(a_args);
    }

    CallDelayed(delay, [selection = std::move(*a_selection), quantity, price, total, from_other, form_id, event,
                        owner = a_row->data.owner, target,
                        original_mode = GetContainerMode(), original_session = session] {
        const auto ui = RE::UI::GetSingleton();
        if (session != original_session || !ui->IsMenuOpen(MenuType::MENU_NAME) ||
            MenuType::GetTargetRefHandle() != target || GetContainerMode() != original_mode ||
            !selection.ValidateSources()) {
            return;
        }

        const auto a_menu = ui->GetMenu<MenuType>();
        const auto a_list = a_menu ? a_menu->GetRuntimeData().itemList : nullptr;
        if (!a_list) {
            return;
        }

        const auto a_matching = std::ranges::find_if(a_list->items, [&](const RE::ItemList::Item* a_row) {
            return a_row && a_row->data.owner == owner && a_row->data.objDesc &&
                   selection.Matches(*a_row->data.objDesc);
        });
        if (a_matching == a_list->items.end()) {
            return;
        }

        std::array<RE::GFxValue, direction_arg + 1> values;
        values[0] = RE::GFxValue{quantity};
        if constexpr (is_barter) {
            if (from_other && total > RE::PlayerCharacter::GetSingleton()->GetGoldAmount(true)) {
                return;
            }
            values[1] = RE::GFxValue{price};
        }
        values[direction_arg] = RE::GFxValue{from_other};
        const RE::FxDelegateArgs args{RE::GFxValue{}, a_menu.get(), a_menu->uiMovie.get(), values.data(),
                                      static_cast<std::uint32_t>(values.size())};

        const auto previous_replay = std::exchange(replay, {a_list, *a_matching});
        const auto previous_animation = std::exchange(InventoryActionHooks::started_animation, {form_id, event});
        const SKSE::stl::scope_exit restore{[previous_replay, previous_animation]() noexcept {
            replay = previous_replay;
            InventoryActionHooks::started_animation = previous_animation;
        }};
        original_action(args);
    });
}

void Hooks::InventoryActionHooks::MenuSessionChanged(std::string_view a_name) {
    if (a_name == RE::BarterMenu::MENU_NAME) {
        ++MenuAction<RE::BarterMenu>::session;
    } else if (a_name == RE::ContainerMenu::MENU_NAME) {
        ++MenuAction<RE::ContainerMenu>::session;
    }
}

void Hooks::InventoryActionHooks::Install(SKSE::Trampoline& a_trampoline) {
    if (!InventorySelection::Install()) {
        logger::warn("Inventory delays are unavailable because Skyrim's stack cleanup could not be hooked.");
        return;
    }

    MenuAction<RE::BarterMenu>::Install(a_trampoline);
    MenuAction<RE::ContainerMenu>::Install(a_trampoline);
    const REL::Relocation<std::uintptr_t> a_drop_call{RE::InventoryMenu::ITEM_DROP,
                                                     RE::InventoryMenu::DROP_OBJECT_CALL_OFFSET};
    const std::array<std::uint8_t, 6> drop_instruction{
        0xff, REL::Module::IsAE() ? std::uint8_t{0x96} : std::uint8_t{0x97},
        REL::Module::IsVR() ? std::uint8_t{0x68} : std::uint8_t{0x58}, 0x06, 0x00, 0x00
    };
    if (std::memcmp(reinterpret_cast<const void*>(a_drop_call.address()), drop_instruction.data(), drop_instruction.size()) != 0) {
        logger::warn("The InventoryMenu drop call has already been changed.");
        return;
    }

    a_trampoline.write_call<6>(a_drop_call.address(), DropObject);
}

bool Hooks::InventoryActionHooks::HasStarted(RE::TESObjectREFR* a_actor, const RE::TESForm* a_item,
                                            Presets::AnimEvent a_event) {
    return a_actor && a_actor->IsPlayerRef() && a_item && started_animation.item == a_item->GetFormID() &&
           (started_animation.event == a_event ||
            (started_animation.event == Presets::kBuy && a_event == Presets::kItemAdd));
}

RE::ObjectRefHandle* Hooks::InventoryActionHooks::DropObject(RE::PlayerCharacter* a_player,
                                                            RE::ObjectRefHandle& a_result,
                                                            RE::TESBoundObject* a_item,
                                                            RE::ExtraDataList* a_extra_list,
                                                            std::int32_t a_count,
                                                            const RE::NiPoint3* a_drop_location,
                                                            const RE::NiPoint3* a_rotation) {
    if (!a_item || a_count <= 0 || a_drop_location || a_rotation) {
        return std::construct_at(std::addressof(a_result),
                                 a_player->DropObject(a_item, a_extra_list, a_count, a_drop_location, a_rotation));
    }

    const int delay = Manager::GetSingleton()->OnDrop(a_player, a_item);
    if (delay > 0) {
        if (auto a_selection = InventorySelection::Capture(*a_item, a_extra_list, a_count)) {
            CallDelayed(delay, [selection = std::move(*a_selection)] {
                const auto player = RE::PlayerCharacter::GetSingleton();
                const auto selection_now = player ? selection.ResolveDrop(*player) : std::nullopt;
                if (!selection_now) {
                    return;
                }
                const auto previous = std::exchange(started_animation,
                    {selection_now->item->GetFormID(), Presets::kItemDrop});
                const SKSE::stl::scope_exit restore{[previous]() noexcept { started_animation = previous; }};
                player->DropObject(selection_now->item, selection_now->extra_list, selection_now->count);
                if (RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
                    RE::SendUIMessage::SendInventoryUpdateMessage(player, selection_now->item);
                }
            });
            return std::construct_at(std::addressof(a_result));
        }
    }

    const auto previous = std::exchange(started_animation, {a_item->GetFormID(), Presets::kItemDrop});
    const SKSE::stl::scope_exit restore{[previous]() noexcept { started_animation = previous; }};
    return std::construct_at(std::addressof(a_result), a_player->DropObject(a_item, a_extra_list, a_count));
}

void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4,
                             bool a5) {
    if (!a_this || !a_object) {
        return add_item_functor_(a_this, a_object, a_count, a4, a5);
    }
    return add_item_functor_(a_this, a_object, a_count, a4, a5);
}

namespace {
    bool IsGameFrozen() {
        if (const auto main = RE::Main::GetSingleton()) {
            if (main->GetRuntimeData().freezeTime) return true;
            if (!main->GetRuntimeData().gameActive) return true;
        } else return true;
        if (RE::UI::GetSingleton()->GameIsPaused()) return true;
        return false;
    }

    bool IsGameWindowInFocus() {
        const HWND foregroundWindow = GetForegroundWindow();
        if (!foregroundWindow) {
            return false;
        }

        DWORD foregroundProcessId;
        GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);

        const DWORD currentProcessId = GetCurrentProcessId();
        return foregroundProcessId == currentProcessId;
    }
}

void Hooks::DrawHook::thunk(std::uint32_t a_timer) {
    func(a_timer);

    if (IsGameFrozen() || !IsGameWindowInFocus()) {
        Manager::GetSingleton()->PauseAnimators();
    } else {
        Manager::GetSingleton()->ResumeAnimators();
    }
}

LRESULT Hooks::WndProcHook::thunk(HWND a_window, UINT a_message, WPARAM a_wParam, LPARAM a_lParam) {
    if (a_message == WM_ACTIVATEAPP && !a_wParam) {
        Manager::GetSingleton()->PauseAnimators();
    }

    return CallWindowProcA(func, a_window, a_message, a_wParam, a_lParam);
}

void Hooks::MagicEffectHooks::Install() {
    REL::Relocation<std::uintptr_t> value_modifier_vtbl{RE::ValueModifierEffect::VTABLE[0]};
    _value_modifier_effect = value_modifier_vtbl.write_vfunc(0x14, ValueModifierEffectCast);

    REL::Relocation<std::uintptr_t> peak_value_modifier_vtbl{RE::PeakValueModifierEffect::VTABLE[0]};
    _peak_value_modifier_effect = peak_value_modifier_vtbl.write_vfunc(0x14, PeakValueModifierEffectCast);

    REL::Relocation<std::uintptr_t> script_effect_vtbl{RE::ScriptEffect::VTABLE[0]};
    _script_effect = script_effect_vtbl.write_vfunc(0x14, ScriptEffectCast);

    REL::Relocation<std::uintptr_t> calm_effect_vtbl{RE::CalmEffect::VTABLE[0]};
    _calm_effect = calm_effect_vtbl.write_vfunc(0x14, CalmEffectCast);
}

void Hooks::MagicEffectHooks::DispatchEffectAnimation(RE::ActiveEffect* a_effect) {
    if (!a_effect) {
        return;
    }

    const auto base_effect = a_effect->GetBaseObject();
    if (!base_effect) {
        return;
    }

    const auto manager = Manager::GetSingleton();

    if (const auto a_caster = a_effect->caster.get().get()) {
        manager->OnMagicEffectCast(a_caster, base_effect);
    }

    if (const auto a_target = a_effect->target ? a_effect->target->GetTargetStatsObject() : nullptr) {
        manager->OnMagicEffectTarget(a_target, base_effect);
    }
}

void Hooks::MagicEffectHooks::ValueModifierEffectCast(RE::ValueModifierEffect* a_this) {
    _value_modifier_effect(a_this);
    DispatchEffectAnimation(a_this);
}

void Hooks::MagicEffectHooks::PeakValueModifierEffectCast(RE::PeakValueModifierEffect* a_this) {
    _peak_value_modifier_effect(a_this);
    DispatchEffectAnimation(a_this);
}

void Hooks::MagicEffectHooks::ScriptEffectCast(RE::ScriptEffect* a_this) {
    _script_effect(a_this);
    DispatchEffectAnimation(a_this);
}

void Hooks::MagicEffectHooks::CalmEffectCast(RE::CalmEffect* a_this) {
    _calm_effect(a_this);
    DispatchEffectAnimation(a_this);
}

template <typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType* a_this, RE::TESObjectREFR* a_object, int32_t a_count,
                                                 bool a_arg3, bool a_play_sound) {
    if (!a_this || !a_object || a_count <= 0 || a_object->IsDisabled() || a_object->IsDeleted() || a_object->
        IsMarkedForDeletion()) {
        return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
    }

    PlayerInteraction::Animate(Presets::kItemPickup, a_this, a_object);
    pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template <typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object,
                                                         RE::ExtraDataList* a_extraList, std::int32_t a_count,
                                                         RE::TESObjectREFR* a_fromRefr) {
    if (!a_this || !a_object || a_count <= 0) {
        return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
    }

    auto manager = Manager::GetSingleton();
    if (!InventoryActionHooks::HasStarted(a_this, a_object, Presets::kBuy) && a_fromRefr &&
        RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME) && manager->OnBuy(a_this, a_object) > 0) {
        return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
    }
    if (!InventoryActionHooks::HasStarted(a_this, a_object, Presets::kItemAdd)) {
        manager->OnItemAdd(a_this, a_object);
    }

    add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
}

template <typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType* a_this,
                                                               RE::ObjectRefHandle& a_hidden_return_argument,
                                                               RE::TESBoundObject* a_item, std::int32_t a_count,
                                                               RE::ITEM_REMOVE_REASON a_reason,
                                                               RE::ExtraDataList* a_extra_list,
                                                               RE::TESObjectREFR* a_move_to_ref,
                                                               const RE::NiPoint3* a_drop_loc,
                                                               const RE::NiPoint3* a_rotate) {
    auto manager = Manager::GetSingleton();
    switch (a_reason) {
    case RE::ITEM_REMOVE_REASON::kDropping:
        if (!InventoryActionHooks::HasStarted(a_this, a_item, Presets::kItemDrop)) {
            manager->OnDrop(a_this, a_item);
        }
        break;
    case RE::ITEM_REMOVE_REASON::kSelling:
        if (!InventoryActionHooks::HasStarted(a_this, a_item, Presets::kSell)) {
            manager->OnSell(a_this, a_item);
        }
        break;
    case RE::ITEM_REMOVE_REASON::kRemove:
        manager->OnItemRemove(a_this, a_item);
        break;
    default:
        break;
    }

    return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref,
                        a_drop_loc, a_rotate);
}

template <typename MenuType>
RE::UI_MESSAGE_RESULTS Hooks::MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message) {
    const auto type = a_message.type.get();
    if constexpr (std::is_same_v<MenuType, RE::BarterMenu> || std::is_same_v<MenuType, RE::ContainerMenu>) {
        if ((type == RE::UI_MESSAGE_TYPE::kShow || type == RE::UI_MESSAGE_TYPE::kHide ||
             type == RE::UI_MESSAGE_TYPE::kForceHide) && a_message.menu == MenuType::MENU_NAME) {
            InventoryActionHooks::MenuSessionChanged(MenuType::MENU_NAME);
        }
    }

    bool is_opening;

    {
        const auto msg_type = static_cast<int>(a_message.type.get());

        is_opening = msg_type == 1;
        const bool is_closing = msg_type == 3;

        if (!is_opening && !is_closing) {
            return _ProcessMessage(this, a_message);
        }
    }

    if (menu_blocks.contains(MenuType::MENU_NAME)) {
        CallDelayed(500, [is_opening] {
            std::scoped_lock lock(menu_blocks_mutex);
            menu_blocks.at(MenuType::MENU_NAME) = !is_opening;
        });
    }

    if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu == menuname) {
        const auto manager = Manager::GetSingleton();

        if (manager->IsMenuQueued(menuname, is_opening)) {
            manager->UnSetMenuQueued(menuname);
            return _ProcessMessage(this, a_message);
        }

        if (const auto delay = manager->OnMenuOpenClose(menuname, is_opening); is_opening && delay > 0) {
            RE::RefHandle container_handle{};
            auto container_mode = RE::ContainerMenu::ContainerMode::kLoot;
            if constexpr (std::is_same_v<MenuType, RE::ContainerMenu>) {
                container_handle = RE::ContainerMenu::GetTargetRefHandle();
                if (!RE::TESObjectREFR::LookupByHandle(container_handle)) {
                    return _ProcessMessage(this, a_message);
                }
                container_mode = RE::ContainerMenu::GetContainerMode();
            }

            CallDelayed(delay, [menuname, container_handle, container_mode] {
                const auto ui = RE::UI::GetSingleton();
                if (ui->IsMenuOpen(menuname)) {
                    return;
                }

                const auto manager = Manager::GetSingleton();
                if constexpr (std::is_same_v<MenuType, RE::ContainerMenu>) {
                    const auto container = RE::TESObjectREFR::LookupByHandle(container_handle);
                    if (!container || container->IsDisabled() || container->IsDeleted() ||
                        container->IsMarkedForDeletion() || ui->IsPauseMenuDisabled()) {
                        return;
                    }

                    manager->SetMenuQueued(menuname, true);
                    RE::ContainerMenu::OpenMenu(container.get(), container_mode);
                } else {
                    manager->SetMenuQueued(menuname, true);
                    manager->OpenCloseMenu(menuname, true);
                }
            });

            manager->OpenCloseMenu(menuname, !is_opening);

            return RE::UI_MESSAGE_RESULTS::kHandled;
        }
    }
    return _ProcessMessage(this, a_message);
}

template <typename FormType>
bool Hooks::ActivateHook<FormType>::Activate_Hook(FormType* a_this, RE::TESObjectREFR* a_targetRef,
                                                  RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3,
                                                  RE::TESBoundObject* a_obj, std::int32_t a_targetCount) {
    if (a_activatorRef && a_targetRef && !a_targetRef->IsDisabled() && !a_targetRef->IsDeleted() &&
        !a_targetRef->IsMarkedForDeletion()) {
        PlayerInteraction::Animate(Presets::kActivate, a_activatorRef, a_targetRef);
    }

    return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
}

template <typename FormType>
void Hooks::ActivateHook<FormType>::install() {
    REL::Relocation<std::uintptr_t> _vtbl{FormType::VTABLE[0]};
    _Activate = _vtbl.write_vfunc(activate_vfunc_slot, Activate_Hook);
}

template <typename MenuType>
void Hooks::MenuHook<MenuType>::InstallHook(const REL::VariantID& varID) {
    REL::Relocation<std::uintptr_t> vTable(varID);
    _ProcessMessage = vTable.write_vfunc(0x4, &MenuHook<MenuType>::ProcessMessage_Hook);
}

namespace {
    // yoinked po3's code
    template <class T, class U>
    auto adjust_pointer(U* a_ptr, const std::ptrdiff_t a_adjust) noexcept {
        auto addr = a_ptr ? reinterpret_cast<std::uintptr_t>(a_ptr) + a_adjust : 0;
        if constexpr (std::is_const_v<U> && std::is_volatile_v<U>) {
            return reinterpret_cast<std::add_cv_t<T>*>(addr);
        } else if constexpr (std::is_const_v<U>) {
            return reinterpret_cast<std::add_const_t<T>*>(addr);
        } else if constexpr (std::is_volatile_v<U>) {
            return reinterpret_cast<std::add_volatile_t<T>*>(addr);
        } else {
            return reinterpret_cast<T*>(addr); // NOLINT(performance-no-int-to-ptr)
        }
    }

    RE::NiNode* GetAttachNode(RE::NiAVObject* animObjectMesh, const std::string& attach_node) {
        const auto root = animObjectMesh->AsFadeNode();
        RE::NiNode* defaultAttachNode = nullptr;
        if (root) {
            if (const auto attachNode = root->GetObjectByName(attach_node)) {
                defaultAttachNode = attachNode->AsNode();
            }
        }
        return defaultAttachNode;
    }

    std::vector<RE::BSGeometry*> GetAllGeometries(RE::NiAVObject* root) {
        std::vector<RE::BSGeometry*> geometries;
        RE::BSVisit::TraverseScenegraphGeometries(
            root, [&geometries](RE::BSGeometry* geom) -> RE::BSVisit::BSVisitControl {
                if (geom && geom->AsGeometry()) {
                    geometries.emplace_back(geom);
                }

                return RE::BSVisit::BSVisitControl::kContinue;
            });
        return geometries;
    }

    RE::NiAVObject* Clone(RE::NiAVObject* original) {
        using func_t = RE::NiAVObject* (*)(RE::NiAVObject* avObj);
        const REL::Relocation<func_t> func{RELOCATION_ID(68835, 70187)};
        return func(original);
    }

    RE::NiAVObject* GetVariableMesh(RE::NiAVObject* original, const Hooks::AttachNodeInfo& attach_node_info) {
        const auto variableMesh = attach_node_info.first.get();
        if (variableMesh == nullptr) {
            return nullptr;
        }

        const auto node = GetAttachNode(original, attach_node_info.second);
        if (!node) {
            return nullptr;
        }

        const auto geometries = GetAllGeometries(variableMesh);

        for (const auto geom : geometries) {
            if (!geom) {
                continue;
            }

            const auto clone = Clone(geom);

            node->AttachChild(clone, true);
        }
        return original->AsFadeNode();
    }
}

RE::NiAVObject* Hooks::AnimObjectHook::thunk(RE::TESModel* a_model, RE::BIPED_OBJECT a_bipedObj,
                                             RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_biped,
                                             RE::NiAVObject* a_root) {
    RE::NiAVObject* output = _LoadAnimObject(a_model, a_bipedObj, a_actor, a_biped, a_root);
    if (const auto animObject = adjust_pointer<RE::TESObjectANIO>(a_model->GetAsModelTextureSwap(), -0x20);
        animObject) {
        if (const auto actor_id = a_actor ? a_actor->GetFormID() : 0; item_meshes.contains(actor_id)) {
            if (const auto containerMesh = GetVariableMesh(output, item_meshes.at(actor_id))) {
                output = containerMesh;
                item_meshes.erase(actor_id);
            }
        }
    }

    return output;
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1) {
    #undef GetObject
    if (const auto a_bound = a1->GetObject()) {
        std::string menuName;
        if (const auto sid = GetSelectedItemDataInMenu(menuName); sid && sid->objDesc->GetObject() == a_bound) {
            Manager::GetSingleton()->OnItemHover(menuName, sid);
        }
    }
    return originalFunction(a1);
}

void Hooks::WndProcHook::Install() {
    if (const auto window = RE::BSGraphics::Renderer::GetCurrentRenderWindow()) {
        func = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
            reinterpret_cast<HWND>(window->hWnd), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(thunk)));

        if (!func) {
            logger::error("Failed to install WndProc hook");
        }
    } else {
        logger::error("Failed to get current render window for WndProc hook");
    }
}

void Hooks::GenericEquipObjectHook::InstallHook(SKSE::Trampoline& a_trampoline) {
    func = a_trampoline.write_call<5>(REL::RelocationID(37938, 38894).address() + REL::Relocate(0xE5, 0x170), thunk);
}

void Hooks::GenericEquipObjectHook::thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor,
                                          RE::TESBoundObject* a_object, std::uint64_t a_unk) {
    if (!a_actor || !a_object) {
        return func(a_manager, a_actor, a_object, a_unk);
    }
    if (const auto delay = Manager::GetSingleton()->OnEquip(a_actor, a_object); delay > 0) {
        //     clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        //       [a_actor, a_object] {
        //           SKSE::GetTaskInterface()->AddTask([a_actor, a_object] {
        //               RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, a_object);
        //               if (a_actor->IsPlayerRef() && RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
        //    RE::SendUIMessage::SendInventoryUpdateMessage(a_actor, nullptr);
        //}
        //           });
        //       },
        //       delay
        //   );
        //   return;
    }
    func(a_manager, a_actor, a_object, a_unk);
}

void Hooks::UnEquipObjectHook::InstallHook(SKSE::Trampoline& a_trampoline) {
    func = a_trampoline.write_call<5>(REL::RelocationID(37945, 38901).address() + REL::Relocate(0x138, 0x1b9), thunk);
}

void Hooks::UnEquipObjectHook::thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object,
                                     std::uint64_t a_unk) {
    if (!a_actor || !a_object) {
        return func(a_manager, a_actor, a_object, a_unk);
    }
    if (const auto delay = Manager::GetSingleton()->OnUnequip(a_actor, a_object); delay > 0) {
        //     clib_utilsQTR::Tasker::GetSingleton()->PushTask(
        //       [a_actor, a_object] {
        //           SKSE::GetTaskInterface()->AddTask([a_actor, a_object] {
        //               RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, a_object);
        //               if (a_actor->IsPlayerRef() && RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
        //    RE::SendUIMessage::SendInventoryUpdateMessage(a_actor, nullptr);
        //}
        //           });
        //       },
        //       delay
        //   );
        //   return;
    }
    func(a_manager, a_actor, a_object, a_unk);
}