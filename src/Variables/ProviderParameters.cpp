#include "ProviderParameters.h"

namespace Variables::Providers::detail {
    namespace {
        constexpr std::array kParamTypeTable{
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kChar, .kind = ParamKind::kUnsupported, .name = "Char/String"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kInt, .kind = ParamKind::kInt, .name = "Int"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFloat, .kind = ParamKind::kFloat, .name = "Float"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kInventoryObject, .kind = ParamKind::kForm,
                          .name = "InventoryObject"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kObjectRef, .kind = ParamKind::kForm, .name = "ObjectRef"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kActorValue, .kind = ParamKind::kInt, .name = "ActorValue"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kActor, .kind = ParamKind::kForm, .name = "Actor"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kSpellItem, .kind = ParamKind::kForm, .name = "SpellItem"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kAxis, .kind = ParamKind::kInt, .name = "Axis"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kCell, .kind = ParamKind::kForm, .name = "Cell"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kAnimGroup, .kind = ParamKind::kInt, .name = "AnimGroup"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMagicItem, .kind = ParamKind::kForm, .name = "MagicItem"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kSound, .kind = ParamKind::kForm, .name = "Sound"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kTopic, .kind = ParamKind::kForm, .name = "Topic"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kQuest, .kind = ParamKind::kForm, .name = "Quest"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kRace, .kind = ParamKind::kForm, .name = "Race"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kClass, .kind = ParamKind::kForm, .name = "Class"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFaction, .kind = ParamKind::kForm, .name = "Faction"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kSex, .kind = ParamKind::kInt, .name = "Sex"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kGlobal, .kind = ParamKind::kForm, .name = "Global"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFurnitureOrFormList, .kind = ParamKind::kForm,
                          .name = "FurnitureOrFormList"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kObject, .kind = ParamKind::kForm, .name = "Object"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kScriptVar, .kind = ParamKind::kUnsupported,
                          .name = "ScriptVar"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kStage, .kind = ParamKind::kInt, .name = "Stage"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMapMarker, .kind = ParamKind::kForm, .name = "MapMarker"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kActorBase, .kind = ParamKind::kForm, .name = "ActorBase"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kContainerRef, .kind = ParamKind::kForm,
                          .name = "ContainerRef"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kWorldOrList, .kind = ParamKind::kForm, .name = "WorldOrList"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kCrimeType, .kind = ParamKind::kInt, .name = "CrimeType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kPackage, .kind = ParamKind::kForm, .name = "Package"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kCombatStyle, .kind = ParamKind::kForm, .name = "CombatStyle"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMagicEffect, .kind = ParamKind::kForm, .name = "MagicEffect"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFormType, .kind = ParamKind::kInt, .name = "FormType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kWeather, .kind = ParamKind::kForm, .name = "Weather"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kNPC, .kind = ParamKind::kForm, .name = "NPC"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kOwner, .kind = ParamKind::kForm, .name = "Owner"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kShaderEffect, .kind = ParamKind::kForm,
                          .name = "ShaderEffect"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFormList, .kind = ParamKind::kForm, .name = "FormList"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMenuIcon, .kind = ParamKind::kForm, .name = "MenuIcon"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kPerk, .kind = ParamKind::kForm, .name = "Perk"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kNote, .kind = ParamKind::kForm, .name = "Note"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMiscStat, .kind = ParamKind::kInt, .name = "MiscStat"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kImagespaceMod, .kind = ParamKind::kForm,
                          .name = "ImagespaceMod"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kImagespace, .kind = ParamKind::kForm, .name = "Imagespace"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kVATSValue, .kind = ParamKind::kUnsupported,
                          .name = "VATSValue"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kVATSValueData, .kind = ParamKind::kUnsupported,
                          .name = "VATSValueData"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kEventFunction, .kind = ParamKind::kUnsupported,
                          .name = "EventFunction"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kEventFunctionMember, .kind = ParamKind::kUnsupported,
                          .name = "EventFunctionMember"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kEventFunctionData, .kind = ParamKind::kUnsupported,
                          .name = "EventFunctionData"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kVoiceType, .kind = ParamKind::kForm, .name = "VoiceType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kEncounterZone, .kind = ParamKind::kForm,
                          .name = "EncounterZone"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kIdleForm, .kind = ParamKind::kForm, .name = "IdleForm"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMessage, .kind = ParamKind::kForm, .name = "Message"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kInvObjectOrFormList, .kind = ParamKind::kForm,
                          .name = "InvObjectOrFormList"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kAlignment, .kind = ParamKind::kInt, .name = "Alignment"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kEquipType, .kind = ParamKind::kForm, .name = "EquipType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kObjectOrFormList, .kind = ParamKind::kForm,
                          .name = "ObjectOrFormList"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kMusic, .kind = ParamKind::kForm, .name = "Music"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kCritStage, .kind = ParamKind::kInt, .name = "CritStage"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kKeyword, .kind = ParamKind::kForm, .name = "Keyword"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kRefType, .kind = ParamKind::kForm, .name = "RefType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kLocation, .kind = ParamKind::kForm, .name = "Location"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kForm, .kind = ParamKind::kForm, .name = "Form"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kAlias, .kind = ParamKind::kInt, .name = "Alias"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kShout, .kind = ParamKind::kForm, .name = "Shout"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kWordOfPower, .kind = ParamKind::kForm, .name = "WordOfPower"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kRelationshipRank, .kind = ParamKind::kInt,
                          .name = "RelationshipRank"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kBGSScene, .kind = ParamKind::kForm, .name = "BGSScene"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kCastingSource, .kind = ParamKind::kInt,
                          .name = "CastingSource"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kAssociationType, .kind = ParamKind::kForm,
                          .name = "AssociationType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kWardState, .kind = ParamKind::kInt, .name = "WardState"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kPackageDataCanBeNull, .kind = ParamKind::kUnsupported,
                          .name = "PackageDataCanBeNull"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kPackageDataNumeric, .kind = ParamKind::kUnsupported,
                          .name = "PackageDataNumeric"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFurnitureAnimType, .kind = ParamKind::kInt,
                          .name = "FurnitureAnimType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kFurnitureEntryType, .kind = ParamKind::kInt,
                          .name = "FurnitureEntryType"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kVMScriptVar, .kind = ParamKind::kUnsupported,
                          .name = "VMScriptVar"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kReferenceEffect, .kind = ParamKind::kForm,
                          .name = "ReferenceEffect"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kPackageData, .kind = ParamKind::kUnsupported,
                          .name = "PackageData"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kSkillAction, .kind = ParamKind::kInt, .name = "SkillAction"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kKnowableForm, .kind = ParamKind::kForm,
                          .name = "KnowableForm"},
            ParamTypeInfo{.type = RE::SCRIPT_PARAM_TYPE::kRegion, .kind = ParamKind::kForm, .name = "Region"}};

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
            if (auto result = a_form->As<T>()) {
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
        if (const auto info = GetParamTypeInfo(a_type)) {
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
                if (const auto result = AsForm<RE::TESFurniture>(a_form)) {
                    return result;
                }
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kObject:
                return AsForm<RE::TESObject>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kMapMarker:
                if (const auto reference = a_form->As<RE::TESObjectREFR>();
                    reference && reference->extraList.HasType<RE::ExtraMapMarker>()) {
                    return reference;
                }
                return nullptr;
            case RE::SCRIPT_PARAM_TYPE::kActorBase:
            case RE::SCRIPT_PARAM_TYPE::kNPC:
                return AsForm<RE::TESNPC>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kContainerRef:
                if (const auto reference = a_form->As<RE::TESObjectREFR>(); reference && reference->GetContainer()) {
                    return reference;
                }
                return nullptr;
            case RE::SCRIPT_PARAM_TYPE::kWorldOrList:
                if (const auto result = AsForm<RE::TESWorldSpace>(a_form)) {
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
                if (const auto result = AsForm<RE::TESNPC>(a_form)) {
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
                if (const auto result = AsForm<RE::TESBoundObject>(a_form)) {
                    return result;
                }
                return AsForm<RE::BGSListForm>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kEquipType:
                return AsForm<RE::BGSEquipSlot>(a_form);
            case RE::SCRIPT_PARAM_TYPE::kObjectOrFormList:
                if (const auto result = AsForm<RE::TESObject>(a_form)) {
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
                return a_form;
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
                if (const auto result = AsForm<RE::EffectSetting>(a_form)) {
                    return result;
                }
                if (const auto result = AsForm<RE::TESWordOfPower>(a_form)) {
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