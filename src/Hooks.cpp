#include "Hooks.h"
#include "Animations.h"

void Hooks::Install()
{
	MoveItemHooks<RE::PlayerCharacter>::install();
	MoveItemHooks<RE::TESObjectREFR>::install(false);
	MoveItemHooks<RE::Character>::install();

    MenuHook<RE::ContainerMenu>::InstallHook(RE::VTABLE_ContainerMenu[0]);
	MenuHook<RE::InventoryMenu>::InstallHook(RE::VTABLE_InventoryMenu[0]);

	auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
	trampoline.create(size_per_hook*4);

	const REL::Relocation<std::uintptr_t> add_item_functor_hook{ RELOCATION_ID(55946, 56490) };
	add_item_functor_ = trampoline.write_call<5>(add_item_functor_hook.address() + 0x15D, add_item_functor);

	const REL::Relocation<std::uintptr_t> function{REL::RelocationID(51019, 51897)};
    InventoryHoverHook::originalFunction = trampoline.write_call<5>(function.address() + REL::Relocate(0x114, 0x22c), InventoryHoverHook::thunk);

	REL::Relocation<std::uintptr_t> target{REL::RelocationID(42420, 43576),
                                                   REL::Relocate(0x22A, 0x21F)};  // AnimationObjects::Load
    AnimObjectHook::_LoadAnimObject = trampoline.write_call<5>(target.address(), AnimObjectHook::thunk);

	const REL::Relocation<std::uintptr_t> target2{REL::RelocationID(75461, 77246)}; // BSGraphics::Renderer::End
    DrawHook::func = trampoline.write_call<5>(target2.address() + 0x9, DrawHook::thunk);

	ActivateHook<RE::TESObjectARMO>::install();
	ActivateHook<RE::TESObjectWEAP>::install();
	ActivateHook<RE::ScrollItem>::install();
	ActivateHook<RE::AlchemyItem>::install();
	ActivateHook<RE::IngredientItem>::install();
}

void Hooks::add_item_functor(RE::TESObjectREFR* a_this, RE::TESObjectREFR* a_object, int32_t a_count, bool a4, bool a5)
{
	if (!a_this || !a_object) {
		return add_item_functor_(a_this, a_object, a_count, a4, a5);
	}
    //Manager::GetSingleton()->OnPickup(a_this, a_object);
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
        Animations::MyAnimator::GetSingleton()->Pause();
    }
	else {
        Animations::MyAnimator::GetSingleton()->Resume();
    }
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::pickUpObject(RefType * a_this, RE::TESObjectREFR * a_object, int32_t a_count, bool a_arg3, bool a_play_sound)
{
	if (!a_this || !a_object || a_count>1) {
		return pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
	}
	//Manager::GetSingleton()->OnPickup(a_this, a_object);

	pick_up_object_(a_this, a_object, a_count, a_arg3, a_play_sound);
}

template<typename RefType>
void Hooks::MoveItemHooks<RefType>::addObjectToContainer(RefType* a_this, RE::TESBoundObject* a_object, RE::ExtraDataList* a_extraList, std::int32_t a_count, RE::TESObjectREFR* a_fromRefr)
{
	if (!a_this || !a_object || a_count<=0) {
		return add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
	}
    add_object_to_container_(a_this, a_object, a_extraList, a_count, a_fromRefr);
}

template<typename RefType>
RE::ObjectRefHandle* Hooks::MoveItemHooks<RefType>::RemoveItem(RefType * a_this, RE::ObjectRefHandle & a_hidden_return_argument, RE::TESBoundObject * a_item, std::int32_t a_count, RE::ITEM_REMOVE_REASON a_reason, RE::ExtraDataList * a_extra_list, RE::TESObjectREFR * a_move_to_ref, const RE::NiPoint3 * a_drop_loc, const RE::NiPoint3 * a_rotate)
{
	if (a_reason == RE::ITEM_REMOVE_REASON::kDropping) {
		RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
		if (res && res->get()) {
			//M->HandleDrop(res->get().get());
		}
		return res;
	}

    if (a_reason == RE::ITEM_REMOVE_REASON::kSelling) {
		RE::ObjectRefHandle* res = remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
        //M->HandleSell(a_item->GetFormID(),a_move_to_ref);
		return res;
	}

	if (!a_move_to_ref) {
        if (const auto a_formid = a_item->GetFormID()) {
			//if (!ModCompatibility::Mods::doppelgangers.contains(a_this->GetBaseObject()->GetFormID())) {
			//    logger::info("Item removed from {} {:x} to nowhere for reason {}. Count {}", a_this->GetName(), a_this->GetFormID(),static_cast<int>(a_reason),a_count);
			//}
			//M->OnConsume(a_formid, a_this);
	    }
	}

	return remove_item_(a_this, a_hidden_return_argument, a_item, a_count, a_reason, a_extra_list, a_move_to_ref, a_drop_loc, a_rotate);
}

template<typename MenuType>
RE::UI_MESSAGE_RESULTS Hooks::MenuHook<MenuType>::ProcessMessage_Hook(RE::UIMessage& a_message)
{
	const auto msg_type = static_cast<int>(a_message.type.get());
	if (msg_type != 3 && msg_type != 1) {
		return _ProcessMessage(this, a_message);
	}

	if (const std::string_view menuname = MenuType::MENU_NAME; a_message.menu==menuname) {
	    if (menuname == RE::ContainerMenu::MENU_NAME) {
            if (RE::TESObjectREFRPtr refr; LookupReferenceByHandle(RE::ContainerMenu::GetTargetRefHandle(), refr)) {

			}
        }
	}
    return _ProcessMessage(this, a_message);
}

template<typename FormType>
bool Hooks::ActivateHook<FormType>::Activate_Hook(RE::TESBoundObject* a_this, RE::TESObjectREFR* a_targetRef, RE::TESObjectREFR* a_activatorRef, std::uint8_t a_arg3, RE::TESBoundObject* a_obj, std::int32_t a_targetCount)
{
	return _Activate(a_this, a_targetRef, a_activatorRef, a_arg3, a_obj, a_targetCount);
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
    auto adjust_pointer(U* a_ptr, std::ptrdiff_t a_adjust) noexcept {
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

    RE::NiNode* GetAttachNode(RE::NiAVObject* animObjectMesh) {
        auto* root = animObjectMesh->AsFadeNode();
        RE::NiNode* defaultAttachNode = nullptr;
        if (root) {
            if (auto* attachNode = root->GetObjectByName(Hooks::attach_node)) {
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
        REL::Relocation<func_t> func{RELOCATION_ID(68835, 70187)};
        return func(original);
    }

    RE::NiAVObject* GetContainerMesh(RE::NiAVObject* original, RE::NiAVObject* ContainerMesh) {
        if (ContainerMesh == nullptr) {
            return nullptr;
        }

        auto* node = GetAttachNode(original);
	    if (!node) {
		    return nullptr;
	    }

        auto geometries = GetAllGeometries(ContainerMesh);

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
		if (item_meshes.contains(item_mesh)) {
            if (auto* containerMesh = GetContainerMesh(output,item_meshes.at(item_mesh).get())) {
                output = containerMesh;
            }
		}
    }

    return output;
}

int64_t Hooks::InventoryHoverHook::thunk(RE::InventoryEntryData* a1)
{
#undef GetObject
	if (const auto a_bound = a1->GetObject()) {
		
	}
	return originalFunction(a1);
}

