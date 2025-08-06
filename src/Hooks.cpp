#include "Hooks.h"
#include "Manager.h"
#include "Utils.h"

using namespace Utils;

void Hooks::Install()
{
	MoveItemHooks<RE::PlayerCharacter>::install();
	MoveItemHooks<RE::TESObjectREFR>::install(false);
	MoveItemHooks<RE::Character>::install();

    MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0]);
	MenuHook<RE::InventoryMenu>::InstallHook(RE::VTABLE_InventoryMenu[0]);
	MenuHook<RE::BarterMenu>::InstallHook(RE::VTABLE_BarterMenu[0]);
	MenuHook<RE::MagicMenu>::InstallHook(RE::VTABLE_MagicMenu[0]);
	MenuHook<RE::FavoritesMenu>::InstallHook(RE::VTABLE_FavoritesMenu[0]);
	MenuHook<RE::MapMenu>::InstallHook(RE::VTABLE_MapMenu[0]);
	MenuHook<RE::JournalMenu>::InstallHook(RE::VTABLE_JournalMenu[0]);

	auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
	trampoline.create(size_per_hook*5);

    GenericEquipObjectHook::InstallHook(trampoline);
    UnEquipObjectHook::InstallHook(trampoline);

	/*const REL::Relocation<std::uintptr_t> add_item_functor_hook{ RELOCATION_ID(55946, 56490) };
	add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);*/

	const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
    InventoryHoverHook::originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c), InventoryHoverHook::thunk);

	const REL::Relocation<std::uintptr_t> target{REL::RelocationID(42420, 43576),REL::Relocate(0x22A, 0x21F)};  // AnimationObjects::Load
    AnimObjectHook::_LoadAnimObject = trampoline.write_call<5>(target.address(), AnimObjectHook::thunk);

	const REL::Relocation<std::uintptr_t> target2{REL::RelocationID(75461, 77246)}; // BSGraphics::Renderer::End
    DrawHook::func = trampoline.write_call<5>(target2.address() + 0x9, DrawHook::thunk);

	ActivateHook<RE::TESObjectARMO>::install();
	ActivateHook<RE::TESObjectWEAP>::install();
	ActivateHook<RE::ScrollItem>::install();
	ActivateHook<RE::AlchemyItem>::install();
	ActivateHook<RE::IngredientItem>::install();
	ActivateHook<RE::TESObjectBOOK>::install();
	ActivateHook<RE::TESObjectCONT>::install();
	ActivateHook<RE::TESObjectDOOR>::install();
	ActivateHook<RE::TESObjectSTAT>::install();
	ActivateHook<RE::TESObjectACTI>::install();
	ActivateHook<RE::TESObjectLIGH>::install();
	ActivateHook<RE::TESObjectTREE>::install();
}

void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4, bool a5)
{
	if (!a_this || !a_object) {
		return add_item_functor_(a_this, a_object, a_count, a4, a5);
	}
    //Manager::GetSingleton()->OnPickup(a_this, a_object->GetBaseObject());
	return add_item_functor_(a_this, a_object, a_count, a4, a5);
}

namespace {
	bool IsGameFrozen() {
        if (const auto main = RE::Main::GetSingleton()) {
            if (main->freezeTime) return true;
            if (!main->gameActive) return true;
        }
	    else return true;
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

void Hooks::DrawHook::thunk(std::uint32_t a_timer)
{
	func(a_timer);

    if (IsGameFrozen() || !IsGameWindowInFocus()) {
        Manager::GetSingleton()->PauseAnimators();
    }
	else {
        Manager::GetSingleton()->ResumeAnimators();
    }
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType * a_this, RE::TESObjectREFR * a_object, int32_t a_count, bool a_arg3, bool a_play_sound)
{
	if (!a_this || !a_object) {
		return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
	}

	if (auto delay = Manager::GetSingleton()->OnPickup(a_this, a_object->GetBaseObject()); delay > 0) {
		CallOriginalMethodDelayed(delay, a_this, pick_up_object_.get(), a_this, a_object, a_count, a_arg3, a_play_sound);
	    return;
	}
	pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extraList, std::int32_t a_count, RE::TESObjectREFR* a_fromRefr)
{
	if (!a_this || !a_object || a_count<=0) {
		return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
	}

    int delay = 0;

    if (a_fromRefr && RE::UI::GetSingleton()->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
        delay = Manager::GetSingleton()->OnBuy(a_this, a_object);
    }
    if (delay > 0) {
		CallOriginalMethodDelayed(delay, a_this, add_object_to_container_.get(), a_this, a_object, a_extraList, a_count, a_fromRefr);
	    return;
	}
    
    delay = Manager::GetSingleton()->OnItemAdd(a_this, a_object);
    if (delay > 0) {
		CallOriginalMethodDelayed(delay, a_this, add_object_to_container_.get(), a_this, a_object, a_extraList, a_count, a_fromRefr);
	    return;
	}


    add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
}

template<typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType * a_this, RE::ObjectRefHandle& a_hidden_return_argument, RE::TESBoundObject * a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList * a_extra_list, RE::TESObjectREFR * a_move_to_ref, const RE::NiPoint3 * a_drop_loc, const RE::NiPoint3 * a_rotate)
{
	if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
        if (auto delay = Manager::GetSingleton()->OnDrop(a_this, a_item); delay > 0) {
            CallOriginalMethodDelayed(delay, a_this, remove_item_.get(), a_this, std::ref(a_hidden_return_argument), a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	        return &a_hidden_return_argument;
	    }

		return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	}

    if (a_reason == RE::ITEM_REMOVE_REASON::kSelling) {
        if (auto delay = Manager::GetSingleton()->OnSell(a_this, a_item); delay > 0) {
			CallOriginalMethodDelayed(delay, a_this, remove_item_.get(), a_this, std::ref(a_hidden_return_argument), a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	        return &a_hidden_return_argument;
	    }
        
		return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	}

    if (a_reason == RE::ITEM_REMOVE_REASON::kRemove) {
        if (auto delay = Manager::GetSingleton()->OnItemRemove(a_this, a_item); delay > 0) {
			CallOriginalMethodDelayed(delay, a_this, remove_item_.get(), a_this, std::ref(a_hidden_return_argument), a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	        return &a_hidden_return_argument;
	    }
        
		return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
	}

	//if (!a_move_to_ref) {
        //if (const auto a_formid = a_item->GetFormID()) {
			//if (!ModCompatibility::Mods::doppelgangers.contains(a_this->GetBaseObject()->GetFormID())) {
			//    logger::info("Item removed from {} {:x} to nowhere for reason {}. Count {}", a_this->GetName(), a_this->GetFormID(),static_cast<int>(a_reason),a_count);
			//}
			//M->OnConsume(a_formid, a_this);
	    //}
	//}

	return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
}

template<typename MenuType>
RE::UI_MESSAGE_RESULTS Hooks::MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message)
{
	const auto msg_type = static_cast<int>(a_message.type.get());
	if (msg_type != 3 && msg_type != 1) {
		return _ProcessMessage(this, a_message);
	}

    if (menu_blocks.contains(MenuType::MENU_NAME)) {
          clib_utilsQTR::Tasker::GetSingleton()->PushTask(
            [msg_type] {
		        menu_blocks.at(MenuType::MENU_NAME) = msg_type == 3;
            },
            500
		);
    }

	if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu==menuname) {

	    RE::TESObjectREFRPtr refr;
        RE::ContainerMenu::ContainerMode container_mode = RE::ContainerMenu::ContainerMode::kLoot;
	    if (menuname == RE::ContainerMenu::MENU_NAME) {
            if (LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), refr)) {
                container_mode = RE::ContainerMenu::GetContainerMode();
            }
        }
        if (const auto delay = Manager::GetSingleton()->OnMenuOpenClose(menuname,msg_type==1); delay > 0) {
              clib_utilsQTR::Tasker::GetSingleton()->PushTask(
                [menuname, msg_type,refr,container_mode] {
                    if (msg_type == 1 && menuname == RE::ContainerMenu::MENU_NAME && refr.get()) {
                        refr->OpenContainer(static_cast<std::int32_t>(container_mode));
                    }
                    else {
                        RE::UIMessageQueue::GetSingleton()->AddMessage(
                            menuname,msg_type == 1 ? RE::UI_MESSAGE_TYPE::kShow:RE::UI_MESSAGE_TYPE::kHide,nullptr);
                    }
                },
                delay
            );

            RE::UIMessageQueue::GetSingleton()->AddMessage(
                menuname,msg_type == 1 ? RE::UI_MESSAGE_TYPE::kHide:RE::UI_MESSAGE_TYPE::kShow,nullptr);
            if (menuname == RE::InventoryMenu::MENU_NAME) {
                RE::UIMessageQueue::GetSingleton()->AddMessage(
                    RE::TweenMenu::MENU_NAME,msg_type == 1 ? RE::UI_MESSAGE_TYPE::kHide:RE::UI_MESSAGE_TYPE::kShow,nullptr);
            }

			return RE::UI_MESSAGE_RESULTS::kHandled;
        }
	}
    return _ProcessMessage(this, a_message);
}

template<typename FormType>
bool Hooks::ActivateHook<FormType>::Activate_Hook(FormType* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3, RE::TESBoundObject* a_obj, std::int32_t a_targetCount)
{
    if (a_activatorRef) {
        const auto item = a_targetRef ? a_targetRef->GetBaseObject() : nullptr;
        if (auto delay = Manager::GetSingleton()->OnActivate(a_activatorRef,item); delay > 0) {
            CallLambdaDelayed(delay, [=] {
                _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
            });

            return true;
        }
    }

	return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
}

template <typename FormType>
void Hooks::ActivateHook<FormType>::install() {
    REL::Relocation<std::uintptr_t> _vtbl{ FormType::VTABLE[0] };
    _Activate = _vtbl.write_vfunc(0x37, Activate_Hook);
}

template<typename MenuType>
void Hooks::MenuHook<MenuType>::InstallHook(const REL::VariantID& varID)
{
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
            return reinterpret_cast<T*>(addr);  // NOLINT(performance-no-int-to-ptr)
        }
    }

    RE::NiNode* GetAttachNode(RE::NiAVObject* animObjectMesh, const std::string& attach_node) {
        auto* root = animObjectMesh->AsFadeNode();
        RE::NiNode* defaultAttachNode = nullptr;
        if (root) {
            if (auto* attachNode = root->GetObjectByName(attach_node)) {
                defaultAttachNode = attachNode->AsNode();
            }
        }
        return defaultAttachNode;
    }

    std::vector<RE::BSGeometry*> GetAllGeometries(RE::NiAVObject* root) {
        std::vector<RE::BSGeometry*> geometries;
        RE::BSVisit::TraverseScenegraphGeometries(root, [&geometries](RE::BSGeometry* geom) -> RE::BSVisit::BSVisitControl {
            if (geom && geom->AsGeometry()) {
                geometries.emplace_back(geom);
            }

            return RE::BSVisit::BSVisitControl::kContinue;
        });
        return geometries;
    }

    RE::NiAVObject* Clone(RE::NiAVObject* original) {
        typedef RE::NiAVObject* (*func_t)(RE::NiAVObject* avObj);
        const REL::Relocation<func_t> func{RELOCATION_ID(68835, 70187)};
        return func(original);
    }

    RE::NiAVObject* GetVariableMesh(RE::NiAVObject* original, const Hooks::AttachNodeInfo& attach_node_info) {
        const auto variableMesh = attach_node_info.first.get();
        if (variableMesh == nullptr) {
            return nullptr;
        }

        auto* node = GetAttachNode(original,attach_node_info.second);
	    if (!node) {
		    return nullptr;
	    }

        const auto geometries = GetAllGeometries(variableMesh);

        for (auto* geom : geometries) {
            if (!geom) {
                continue;
            }

            auto* clone = Clone(geom);

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
            if (auto* containerMesh = GetVariableMesh(output,item_meshes.at(actor_id))) {
                output = containerMesh;
				item_meshes.erase(actor_id);
            }
		}
    }

    return output;
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1)
{
#undef GetObject
	if (const auto a_bound = a1->GetObject()) {
		std::string menuName;
        if (const auto sid = GetSelectedItemDataInMenu(menuName); sid && sid->objDesc->GetObject() == a_bound) {
			Manager::GetSingleton()->OnItemHover(menuName, sid);
        }
	}
	return originalFunction(a1);
}

void Hooks::GenericEquipObjectHook::InstallHook(SKSE::Trampoline& a_trampoline)
{
    func = a_trampoline.write_call<5>(REL::RelocationID(37938, 38894).address() + REL::Relocate(0xE5, 0x170), thunk);
}

void Hooks::GenericEquipObjectHook::thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object, std::uint64_t a_unk)
{
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

void Hooks::UnEquipObjectHook::thunk(RE::ActorEquipManager* a_manager, RE::Actor* a_actor, RE::TESBoundObject* a_object,std::uint64_t a_unk)
{
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