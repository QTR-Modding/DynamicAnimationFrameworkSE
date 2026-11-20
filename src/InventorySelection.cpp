#include "InventorySelection.h"

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
