#include "ProviderParameters.h"

namespace Variables::Providers::detail {
    namespace {
        constexpr std::array kParamTypeTable{
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kChar, ParamKind::kUnsupported, "Char/String"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kInt, ParamKind::kInt, "Int"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFloat, ParamKind::kFloat, "Float"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kInventoryObject, ParamKind::kForm, "InventoryObject"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kObjectRef, ParamKind::kForm, "ObjectRef"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kActorValue, ParamKind::kInt, "ActorValue"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kActor, ParamKind::kForm, "Actor"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kSpellItem, ParamKind::kForm, "SpellItem"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kAxis, ParamKind::kInt, "Axis"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kCell, ParamKind::kForm, "Cell"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kAnimGroup, ParamKind::kInt, "AnimGroup"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMagicItem, ParamKind::kForm, "MagicItem"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kSound, ParamKind::kForm, "Sound"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kTopic, ParamKind::kForm, "Topic"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kQuest, ParamKind::kForm, "Quest"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kRace, ParamKind::kForm, "Race"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kClass, ParamKind::kForm, "Class"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFaction, ParamKind::kForm, "Faction"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kSex, ParamKind::kInt, "Sex"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kGlobal, ParamKind::kForm, "Global"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFurnitureOrFormList, ParamKind::kForm, "FurnitureOrFormList"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kObject, ParamKind::kForm, "Object"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kScriptVar, ParamKind::kUnsupported, "ScriptVar"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kStage, ParamKind::kInt, "Stage"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMapMarker, ParamKind::kForm, "MapMarker"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kActorBase, ParamKind::kForm, "ActorBase"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kContainerRef, ParamKind::kForm, "ContainerRef"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kWorldOrList, ParamKind::kForm, "WorldOrList"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kCrimeType, ParamKind::kInt, "CrimeType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kPackage, ParamKind::kForm, "Package"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kCombatStyle, ParamKind::kForm, "CombatStyle"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMagicEffect, ParamKind::kForm, "MagicEffect"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFormType, ParamKind::kInt, "FormType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kWeather, ParamKind::kForm, "Weather"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kNPC, ParamKind::kForm, "NPC"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kOwner, ParamKind::kForm, "Owner"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kShaderEffect, ParamKind::kForm, "ShaderEffect"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFormList, ParamKind::kForm, "FormList"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMenuIcon, ParamKind::kForm, "MenuIcon"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kPerk, ParamKind::kForm, "Perk"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kNote, ParamKind::kForm, "Note"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMiscStat, ParamKind::kInt, "MiscStat"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kImagespaceMod, ParamKind::kForm, "ImagespaceMod"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kImagespace, ParamKind::kForm, "Imagespace"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kVATSValue, ParamKind::kUnsupported, "VATSValue"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kVATSValueData, ParamKind::kUnsupported, "VATSValueData"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kEventFunction, ParamKind::kUnsupported, "EventFunction"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kEventFunctionMember, ParamKind::kUnsupported, "EventFunctionMember"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kEventFunctionData, ParamKind::kUnsupported, "EventFunctionData"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kVoiceType, ParamKind::kForm, "VoiceType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kEncounterZone, ParamKind::kForm, "EncounterZone"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kIdleForm, ParamKind::kForm, "IdleForm"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMessage, ParamKind::kForm, "Message"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kInvObjectOrFormList, ParamKind::kForm, "InvObjectOrFormList"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kAlignment, ParamKind::kInt, "Alignment"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kEquipType, ParamKind::kForm, "EquipType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kObjectOrFormList, ParamKind::kForm, "ObjectOrFormList"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kMusic, ParamKind::kForm, "Music"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kCritStage, ParamKind::kInt, "CritStage"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kKeyword, ParamKind::kForm, "Keyword"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kRefType, ParamKind::kForm, "RefType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kLocation, ParamKind::kForm, "Location"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kForm, ParamKind::kForm, "Form"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kAlias, ParamKind::kInt, "Alias"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kShout, ParamKind::kForm, "Shout"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kWordOfPower, ParamKind::kForm, "WordOfPower"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kRelationshipRank, ParamKind::kInt, "RelationshipRank"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kBGSScene, ParamKind::kForm, "BGSScene"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kCastingSource, ParamKind::kInt, "CastingSource"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kAssociationType, ParamKind::kForm, "AssociationType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kWardState, ParamKind::kInt, "WardState"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kPackageDataCanBeNull, ParamKind::kUnsupported,
                          "PackageDataCanBeNull"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kPackageDataNumeric, ParamKind::kUnsupported, "PackageDataNumeric"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFurnitureAnimType, ParamKind::kInt, "FurnitureAnimType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kFurnitureEntryType, ParamKind::kInt, "FurnitureEntryType"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kVMScriptVar, ParamKind::kUnsupported, "VMScriptVar"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kReferenceEffect, ParamKind::kForm, "ReferenceEffect"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kPackageData, ParamKind::kUnsupported, "PackageData"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kSkillAction, ParamKind::kInt, "SkillAction"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kKnowableForm, ParamKind::kForm, "KnowableForm"},
            ParamTypeInfo{RE::SCRIPT_PARAM_TYPE::kRegion, ParamKind::kForm, "Region"}};

        static_assert(kParamTypeTable.size() == 81);

        consteval bool HasUniqueParamTypes() {
            for (std::size_t i = 0; i < kParamTypeTable.size(); ++i) {
                for (std::size_t j = i + 1; j < kParamTypeTable.size(); ++j) {
                    if (kParamTypeTable[i].type == kParamTypeTable[j].type) {
                        return false;
                    }
                }
            }
            return true;
        }

        static_assert(HasUniqueParamTypes());

        template <class T>
        void* AsForm(RE::TESForm* a_form) noexcept {
            if (auto* result = a_form->As<T>()) {
                return static_cast<void*>(result);
            }
            return nullptr;
        }
    }

    const ParamTypeInfo* GetParamTypeInfo(const RE::SCRIPT_PARAM_TYPE a_type) noexcept {
        for (const auto& info : kParamTypeTable) {
            if (info.type == a_type) {
                return &info;
            }
        }
        return nullptr;
    }

    std::string GetParamTypeName(const RE::SCRIPT_PARAM_TYPE a_type) {
        if (const auto* info = GetParamTypeInfo(a_type)) {
            return std::string(info->name);
        }
        return "unknown SCRIPT_PARAM_TYPE " + std::to_string(static_cast<std::uint32_t>(a_type));
    }

    void* ResolveFormPointer(const RE::SCRIPT_PARAM_TYPE a_type, RE::TESForm* a_form) noexcept {
        if (!a_form) {
            return nullptr;
        }

        switch (a_type) {
            case RE::SCRIPT_PARAM_TYPE::kInventoryObject:
                return AsForm<RE::TESBoundObject>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kObjectRef:
                return AsForm<RE::TESObjectREFR>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kActor:
                return AsForm<RE::Actor>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kSpellItem:
                return AsForm<RE::SpellItem>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kCell:
                return AsForm<RE::TESObjectCELL>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMagicItem:
                return AsForm<RE::MagicItem>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kSound:
                return AsForm<RE::TESSound>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kTopic:
                return AsForm<RE::TESTopic>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kQuest:
                return AsForm<RE::TESQuest>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kRace:
                return AsForm<RE::TESRace>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kClass:
                return AsForm<RE::TESClass>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kFaction:
                return AsForm<RE::TESFaction>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kGlobal:
                return AsForm<RE::TESGlobal>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kFurnitureOrFormList:
                if (auto* result = AsForm<RE::TESFurniture>(a_form)) {
                    return result;
                }
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kObject:
                return AsForm<RE::TESObject>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMapMarker:
                if (auto* reference = a_form->As<RE::TESObjectREFR>();
                    reference && reference->extraList.HasType<RE::ExtraMapMarker>()) {
                    return static_cast<void*>(reference);
                }
                return nullptr;
            case RE::SCRIPT_PARAM_TYPE::kActorBase:
            case RE::SCRIPT_PARAM_TYPE::kNPC:
                return AsForm<RE::TESNPC>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kContainerRef:
                if (auto* reference = a_form->As<RE::TESObjectREFR>(); reference && reference->GetContainer()) {
                    return static_cast<void*>(reference);
                }
                return nullptr;
            case RE::SCRIPT_PARAM_TYPE::kWorldOrList:
                if (auto* result = AsForm<RE::TESWorldSpace>(a_form)) {
                    return result;
                }
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kPackage:
                return AsForm<RE::TESPackage>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kCombatStyle:
                return AsForm<RE::TESCombatStyle>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMagicEffect:
                return AsForm<RE::EffectSetting>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kWeather:
                return AsForm<RE::TESWeather>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kOwner:
                if (auto* result = AsForm<RE::TESNPC>(a_form)) {
                    return result;
                }
                return AsForm<RE::TESFaction>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kShaderEffect:
                return AsForm<RE::TESEffectShader>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kFormList:
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMenuIcon:
                return AsForm<RE::BGSMenuIcon>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kPerk:
                return AsForm<RE::BGSPerk>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kNote:
                return AsForm<RE::BGSNote>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kImagespaceMod:
                return AsForm<RE::TESImageSpaceModifier>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kImagespace:
                return AsForm<RE::TESImageSpace>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kVoiceType:
                return AsForm<RE::BGSVoiceType>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kEncounterZone:
                return AsForm<RE::BGSEncounterZone>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kIdleForm:
                return AsForm<RE::TESIdleForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMessage:
                return AsForm<RE::BGSMessage>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kInvObjectOrFormList:
                if (auto* result = AsForm<RE::TESBoundObject>(a_form)) {
                    return result;
                }
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kEquipType:
                return AsForm<RE::BGSEquipSlot>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kObjectOrFormList:
                if (auto* result = AsForm<RE::TESObject>(a_form)) {
                    return result;
                }
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMusic:
                return AsForm<RE::BGSMusicType>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kKeyword:
                return AsForm<RE::BGSKeyword>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kRefType:
                return AsForm<RE::BGSLocationRefType>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kLocation:
                return AsForm<RE::BGSLocation>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kForm:
                return static_cast<void*>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kShout:
                return AsForm<RE::TESShout>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kWordOfPower:
                return AsForm<RE::TESWordOfPower>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kBGSScene:
                return AsForm<RE::BGSScene>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kAssociationType:
                return AsForm<RE::BGSAssociationType>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kReferenceEffect:
                return AsForm<RE::BGSReferenceEffect>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kKnowableForm:
                if (auto* result = AsForm<RE::EffectSetting>(a_form)) {
                    return result;
                }
                if (auto* result = AsForm<RE::TESWordOfPower>(a_form)) {
                    return result;
                }
                return AsForm<RE::EnchantmentItem>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kRegion:
                return AsForm<RE::TESRegion>(a_form);
            default:
                return nullptr;
        }
    }
}
