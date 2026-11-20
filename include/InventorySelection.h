#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace Hooks {
    class InventorySelection {
    public:
        struct Resolved {
            RE::TESBoundObject* item;
            RE::ExtraDataList* extra_list;
            std::int32_t count;
        };

        [[nodiscard]] static bool Install();
        [[nodiscard]] static std::optional<InventorySelection> Capture(
            const RE::TESBoundObject& a_item, RE::ExtraDataList* a_extra_list, std::int32_t a_count);
        [[nodiscard]] static std::optional<InventorySelection> Capture(
            const RE::InventoryEntryData& a_entry, std::int32_t a_count, RE::TESObjectREFR* a_source);

        [[nodiscard]] std::optional<Resolved> ResolveDrop(RE::PlayerCharacter& a_player) const;
        [[nodiscard]] bool ValidateSources() const;
        [[nodiscard]] bool Matches(const RE::InventoryEntryData& a_entry) const;

    private:
        struct StackWatch {
            explicit StackWatch(RE::ExtraDataList* a_list);
            ~StackWatch();

            RE::ExtraDataList* list;
            std::atomic_bool live{true};
        };

        struct StackSelection {
            std::shared_ptr<StackWatch> watch;
            std::int32_t count;
            RE::ObjectRefHandle origin;
            bool worn{};
            bool stackable{};
        };

        struct RequiredStack {
            RE::ObjectRefHandle owner;
            const RE::ExtraDataList* list;
            std::int32_t count;
        };

        InventorySelection(RE::FormID a_form_id, std::int32_t a_count,
                           std::vector<StackSelection> a_stacks);

        [[nodiscard]] static bool HasItems(RE::TESObjectREFR& a_source, RE::TESBoundObject& a_item,
                                            const RE::ExtraDataList* a_extra_list, std::int32_t a_count, bool a_for_drop = false);

        [[nodiscard]] static std::shared_ptr<StackWatch> WatchStack(RE::ExtraDataList* a_list);
        static void DestroyExtraDataList(RE::ExtraDataList* a_list);
        static inline void (*original_destructor)(RE::ExtraDataList*){nullptr};
        static inline void (*original_cleanup)(RE::ExtraDataList*, bool){nullptr};
        static inline std::mutex stack_watches_lock;
        static inline std::unordered_map<RE::ExtraDataList*, std::weak_ptr<StackWatch>> stack_watches;

        RE::FormID form_id;
        std::int32_t count;
        std::vector<StackSelection> stacks;
        std::vector<RequiredStack> required_stacks;
    };
}
