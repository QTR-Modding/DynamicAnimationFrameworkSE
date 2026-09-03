#include "Utils.h"

namespace {
    void LogConditionResults(const RE::TESCondition* a_condition, RE::Actor* actor, RE::TESObjectREFR* target) {
        if (!a_condition) {
            return;
        }

        auto a_head = a_condition->head;

        while (a_head) {
            RE::ConditionCheckParams params(actor, target);

            logger::trace("Condition {} = {}", a_head->data.functionData.function.underlying(), a_head->IsTrue(params));

            a_head = a_head->next;
        }
    }
}

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

bool Utils::GetModel(RE::TESForm* a_form, RE::NiPointer<RE::NiAVObject>& a_out) {
    if (const auto model_path = GetModelPath(a_form)) {
        RE::NiPointer<RE::NiNode> a_model;
        if (const auto res = RE::BSModelDB::Demand(model_path, a_model, {});
            res == RE::BSResource::ErrorCode::kNone) {
            if (RE::NiAVObject* constructedObject = a_model && a_model.get() ? a_model.get() : nullptr) {
                a_out.reset(constructedObject);
                return true;
            }
        } else {
            logger::warn("Failed to load model for form {:x}: {}. Error: {}", a_form->GetFormID(), model_path,
                         static_cast<int>(res));
        }
    }
    return false;
}

bool Utils::ShouldSkip([[maybe_unused]] const RE::TESConditionItem* it) {
    return false;
    //const auto func = it->data.functionData.function.get();

    //return func == RE::FUNCTION_DATA::FunctionID::kIsLastHostileActor ||
    //       func == RE::FUNCTION_DATA::FunctionID::kShouldAttackKill ||
    //       func == RE::FUNCTION_DATA::FunctionID::kGetDetected ||
    //       func == RE::FUNCTION_DATA::FunctionID::kGetRandomPercent;
}

bool Utils::EvalConditionsFiltered(const RE::TESCondition& conds, RE::Actor* actor, RE::TESObjectREFR* target) {
    RE::ConditionCheckParams p(actor, target);

    bool overall = true;
    bool blockHasAny = false;
    bool blockValue = false;
    bool prevWasOR = false;

    for (auto it = conds.head; it; it = it->next) {

        const bool v = ShouldSkip(it) ? true : it->IsTrue(p);

        if (!blockHasAny) {
            blockValue = v;
            blockHasAny = true;
        } else {
            if (prevWasOR) {
                blockValue = blockValue || v;
            } else {
                overall = overall && blockValue;
                if (!overall) {
                    return false;
                }
                blockValue = v;
            }
        }

        prevWasOR = it->data.flags.isOR;
    }

    overall = overall && (blockHasAny ? blockValue : true);
    return overall;
}

bool Utils::ParentCheck(const RE::TESIdleForm* idle, RE::Actor* actor, RE::TESObjectREFR* target) {
    return idle && EvalConditionsFiltered(idle->conditions, actor, target);
}

void Utils::CollectIdles(RE::TESIdleForm* parent_idle, std::vector<RE::TESIdleForm*>& out, RE::Actor* actor,
    RE::TESObjectREFR* target, std::unordered_set<RE::TESIdleForm*>& seen) {
    if (!parent_idle || !seen.emplace(parent_idle).second) {
        return;
    }

    CollectIdles(parent_idle->prevIdle, out, actor, target, seen);

    if (!parent_idle->CheckConditions(actor, target, false)) {
#ifndef NDEBUG
        LogConditionResults(&parent_idle->conditions, actor, target);
#endif
        return;
    }

    const bool hasChildren =
        parent_idle->childIdles && parent_idle->childIdles->begin() != parent_idle->childIdles->end();

    if (!hasChildren && (!parent_idle->animEventName.empty() || !parent_idle->animFileName.empty())) {
        out.push_back(parent_idle);
    }

    if (parent_idle->childIdles) {
        for (const auto& childIdle : *parent_idle->childIdles) {
            const auto child = childIdle ? childIdle->As<RE::TESIdleForm>() : nullptr;
            CollectIdles(child, out, actor, target, seen);
        }
    }
}

bool ModCompatibility::ModInfo::IsInstalled() {
    if (!is_checked) {
        constexpr auto plugins_folder = R"(Data\SKSE\Plugins\)";
        const auto mod_path = std::string(plugins_folder) + name + ".dll";
        isLoaded = std::filesystem::exists(mod_path);
        is_checked = true;
    }
    return isLoaded;
}