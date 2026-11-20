#include "InventoryActionHooks.h"
#include "Hooks.h"
#include "InventorySelection.h"
#include "Manager.h"

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
