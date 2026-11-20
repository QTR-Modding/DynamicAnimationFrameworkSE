#pragma once

#include "Presets/PresetInterface.h"

namespace Hooks {
    class InventoryActionHooks {
    public:
        static void Install(SKSE::Trampoline& a_trampoline);
        static void MenuSessionChanged(std::string_view a_name);
        static bool HasStarted(RE::TESObjectREFR* a_actor, const RE::TESForm* a_item, Presets::AnimEvent a_event);

    private:
        template <class MenuType>
        class MenuAction;

        struct StartedAnimation {
            RE::FormID item{};
            Presets::AnimEvent event{Presets::kNone};
        };

        static inline thread_local StartedAnimation started_animation;

        static RE::ObjectRefHandle* DropObject(RE::PlayerCharacter* a_player, RE::ObjectRefHandle& a_result,
                                               RE::TESBoundObject* a_item, RE::ExtraDataList* a_extra_list,
                                               std::int32_t a_count, const RE::NiPoint3* a_drop_location,
                                               const RE::NiPoint3* a_rotation);
    };

    template <class MenuType>
    class InventoryActionHooks::MenuAction {
    public:
        static void Install(SKSE::Trampoline& a_trampoline);
        static inline std::uint64_t session{};

    private:
        class Registration final : public RE::FxDelegateHandler::CallbackProcessor {
        public:
            explicit Registration(RE::FxDelegateHandler::CallbackProcessor* a_processor);
            void Process(const RE::GString& a_name, RE::FxDelegateHandler::CallbackFn* a_method) override;

        private:
            RE::FxDelegateHandler::CallbackProcessor* processor;
        };

        struct Replay {
            RE::ItemList* list{};
            RE::ItemList::Item* item{};
        };

        static constexpr bool is_barter = std::is_same_v<MenuType, RE::BarterMenu>;
        static RE::ContainerMenu::ContainerMode GetContainerMode();
        static void Accept(MenuType* a_menu, RE::FxDelegateHandler::CallbackProcessor* a_processor);
        static RE::ItemList::Item* GetSelectedItem(RE::ItemList* a_list);
        static void Queue(const RE::FxDelegateArgs& a_args);

        static inline thread_local Replay replay;
        static inline REL::Relocation<RE::FxDelegateHandler::CallbackFn*> original_action;
        static inline REL::Relocation<decltype(Accept)*> original_accept;
        static inline REL::Relocation<decltype(GetSelectedItem)*> get_selected_item;
    };
}
