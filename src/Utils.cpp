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

