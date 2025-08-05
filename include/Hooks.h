#pragma once
#include "CLibUtilsQTR/Tasker.hpp"

namespace Hooks {

	void Install();

    inline std::map<std::string_view,bool> menu_blocks = {
         {RE::InventoryMenu::MENU_NAME, true},
        {RE::ContainerMenu::MENU_NAME, true},
        {RE::BarterMenu::MENU_NAME, true},
    };

    template <typename Func, typename... Args>
    void CallOriginalMethodDelayed(int delay, RE::TESObjectREFR* AoI, Func func, Args&&... args)
    {
        auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
		auto smart = RE::NiPointer(AoI);

        clib_utilsQTR::Tasker::GetSingleton()->PushTask(
            [smart, func, args_tuple = std::move(args_tuple)]() mutable {
                SKSE::GetTaskInterface()->AddTask(
                    [smart, func, args_tuple = std::move(args_tuple)]() mutable {
                        if (!smart) {
                            return;
						}
                        std::apply([&]<typename... T0>(T0&&... unpacked) {
                            std::invoke(func, std::forward<T0>(unpacked)...);
                        }, std::move(args_tuple));

                        if (auto ui = RE::UI::GetSingleton();
                            ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) ||
                            ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
                            ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
                            RE::SendUIMessage::SendInventoryUpdateMessage(smart.get(), nullptr);
                        }
                    }
                );
            },
            delay
        );
    }

    template <typename Callable>
    void CallLambdaDelayed(int delay, Callable&& func)
    {
        auto wrapper = std::forward<Callable>(func);
        clib_utilsQTR::Tasker::GetSingleton()->PushTask(
            [wrapper]() mutable {
                SKSE::GetTaskInterface()->AddTask([wrapper = std::move(wrapper)]() mutable {
                    wrapper();
                });
            },
            delay
        );
    }


    template <typename MenuType>
    RE::StandardItemData* GetSelectedItemData() {
        if (menu_blocks.contains(MenuType::MENU_NAME) && menu_blocks.at(MenuType::MENU_NAME)) {
            return nullptr;
		}
        if (const auto ui = RE::UI::GetSingleton()) {
            if (const auto menu = ui->GetMenu<MenuType>()) {
	            if (RE::ItemList* a_itemList = menu->GetRuntimeData().itemList) {
		            if (auto* item = a_itemList->GetSelectedItem()) {
			            return &item->data;
		            }
	            }
            }
        }
		return nullptr;
    }

    RE::StandardItemData* GetSelectedItemDataInMenu(std::string& a_menuOut);

    struct DrawHook {
		static void thunk(std::uint32_t a_timer);
        static inline REL::Relocation<decltype(thunk)> func;
	};

	// Source: https://github.com/RavenKZP/Immersive-Weapon-Switch/blob/main/src/Hooks.cpp
    struct GenericEquipObjectHook {
        static void InstallHook(SKSE::Trampoline& a_trampoline);
        static void thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object,
                          std::uint64_t a_unk);
        static inline REL::Relocation<decltype(thunk)> func;
    };

    struct UnEquipObjectHook {
        static void InstallHook(SKSE::Trampoline& a_trampoline);
        static void thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object,
                          std::uint64_t a_unk);
        static inline REL::Relocation<decltype(thunk)> func;
    };

    template <typename FormType>
    class ActivateHook : public FormType {
        static bool Activate_Hook(FormType* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3, RE::TESBoundObject* a_obj, std::int32_t a_targetCount);
        static inline REL::Relocation<decltype(&FormType::Activate)> _Activate;
    public:
        static void install();
    };

    // Credits: SkyrimThiago
    struct InventoryHoverHook {
        static int64_t thunk(RE::InventoryEntryData* a1);
        static inline REL::Relocation<decltype(thunk)> originalFunction;
    };

    template <typename MenuType>
    class MenuHook : public MenuType {
        using ProcessMessage_t = decltype(&MenuType::ProcessMessage);
        static inline REL::Relocation<ProcessMessage_t> _ProcessMessage;
        RE::UI_MESSAGE_RESULTS ProcessMessage_Hook(RE::UIMessage& a_message);
    public:
        static void InstallHook(const REL::VariantID& varID);
    };

	template <typename RefType>
    class MoveItemHooks {
    public:
        static void install(const bool is_actor = true) {
			REL::Relocation<std::uintptr_t> _vtbl{ RefType::VTABLE[0] };
			if (is_actor) {
			    pick_up_object_ = _vtbl.write_vfunc(0xCC, pickUpObject);
			}
			remove_item_ = _vtbl.write_vfunc(0x56, RemoveItem);
			add_object_to_container_ = _vtbl.write_vfunc(0x5A, addObjectToContainer);
        }

    private:
        static void pickUpObject(RefType* a_this,
                                   RE::TESObjectREFR* a_object,
                                   int32_t a_count,
                                   bool a_arg3,
                                   bool a_play_sound);
        static inline REL::Relocation<decltype(pickUpObject)> pick_up_object_;

        static RE::ObjectRefHandle* RemoveItem(RefType* a_this,
            RE::ObjectRefHandle& a_hidden_return_argument,
            RE::TESBoundObject* a_item,
            std::int32_t a_count,
            RE::ITEM_REMOVE_REASON a_reason,
            RE::ExtraDataList* a_extra_list,
            RE::TESObjectREFR* a_move_to_ref,
            const RE::NiPoint3* a_drop_loc,
            const RE::NiPoint3* a_rotate);
        static inline REL::Relocation<decltype(RemoveItem)> remove_item_;

        static void addObjectToContainer(RefType* a_this,
                                    RE::TESBoundObject* a_object, 
                                    RE::ExtraDataList* a_extraList, 
                                    std::int32_t a_count,
                                    RE::TESObjectREFR* a_fromRefr
                                );
        static inline REL::Relocation<decltype(addObjectToContainer)> add_object_to_container_;

    };

    class AnimObjectHook {
    public:
        static RE::NiAVObject* thunk(RE::TESModel* a_model, RE::BIPED_OBJECT a_bipedObj,
                                              RE::TESObjectREFR* a_actor, RE::BSTSmartPointer<RE::BipedAnim>& a_biped,
                                              RE::NiAVObject* a_root);

        static inline REL::Relocation<decltype(thunk)> _LoadAnimObject;
    };

	inline std::map<RE::FormID,RE::NiPointer<RE::NiAVObject>> item_meshes;
	inline RE::FormID item_mesh;
    inline std::string attach_node;
    inline RE::NiPointer<RE::NiAVObject> objectNode = nullptr;

    static void add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4, bool a5);
	static inline REL::Relocation<decltype(add_item_functor)> add_item_functor_;
};

