#include "Utils.h"

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
    return nullptr;}

const char* Utils::GetModelPath(RE::TESForm* a_form, [[maybe_unused]] RE::Actor* a_actor) {
    switch (a_form->GetFormType()) {
        case RE::FormType::Weapon: {
            const auto form = a_form->As<RE::TESObjectWEAP>();
            return form->GetModel();
        }
        case RE::FormType::Armor: {
            const auto armor = a_form->As<RE::TESObjectARMO>();
            return armor->worldModels->GetModel();
        }
        case RE::FormType::Ammo: {
            const auto ammo = a_form->As<RE::TESAmmo>();
			return ammo->GetModel();
        }
        case RE::FormType::KeyMaster: {
            const auto key = a_form->As<RE::TESKey>();
            return key->GetModel();
		}
        case RE::FormType::Misc: {
            const auto misc = a_form->As<RE::TESObjectMISC>();
            return misc->GetModel();
		}
        case RE::FormType::Scroll: {
            const auto scroll = a_form->As<RE::ScrollItem>();
            return scroll->GetModel();
		}
        case RE::FormType::AlchemyItem: {
            const auto alchemy = a_form->As<RE::AlchemyItem>();
            return alchemy->GetModel();
		}
        case RE::FormType::Ingredient: {
            const auto ingredient = a_form->As<RE::IngredientItem>();
			return ingredient->GetModel();
            }
        case RE::FormType::Book: {
            const auto book = a_form->As<RE::TESObjectBOOK>();
			return book->GetModel();
        }
        case RE::FormType::Note: {
			const auto note = a_form->As<RE::BGSNote>();
			return note->GetModel();
        }
        case RE::FormType::SoulGem: {
			const auto soulgem = a_form->As<RE::TESSoulGem>();
			return soulgem->GetModel();
        }
        case RE::FormType::Apparatus: {
			const auto apparatus = a_form->As<RE::BGSApparatus>();
			return apparatus->GetModel();
        }
        case RE::FormType::Container: {
			const auto container = a_form->As<RE::TESObjectCONT>();
			return container->GetModel();
        }
        case RE::FormType::Door: {
            const auto door = a_form->As<RE::TESObjectDOOR>();
            return door->GetModel();
		}
        case RE::FormType::Static: {
            const auto stat = a_form->As<RE::TESObjectSTAT>();
            return stat->GetModel();
        }
        case RE::FormType::Activator: {
            const auto activator = a_form->As<RE::TESObjectACTI>();
            return activator->GetModel();
        }
        case RE::FormType::Light: {
            const auto light = a_form->As<RE::TESObjectLIGH>();
            return light->GetModel();
        }
        case RE::FormType::Tree: {
            const auto tree = a_form->As<RE::TESObjectTREE>();
            return tree->GetModel();
        }
        default:
			break;
    }
        
    return nullptr;
}

void Utils::GetModel(RE::TESForm* a_form, RE::NiPointer<RE::NiAVObject>& a_out) {
    if (const auto model_path = GetModelPath(a_form)) {
        RE::NiPointer<RE::NiNode> a_model;
        if (const auto res = RE::BSModelDB::Demand(model_path,a_model,{}); 
            res == RE::BSResource::ErrorCode::kNone) {
            RE::NiAVObject* constructedObject = a_model && a_model.get() ? a_model.get() : nullptr;
			a_out.reset(constructedObject);
        }
    }
}

