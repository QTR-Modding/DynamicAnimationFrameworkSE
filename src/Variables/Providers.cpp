#include "Variables/Providers.h"

#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "CLibUtilsQTR/FormReader.hpp"

namespace Variables::Providers {
    namespace detail {
        constexpr std::uint32_t kVanillaProviderCount = 736;
        constexpr std::uint32_t kGetPosID = 6;
        constexpr std::uint32_t kGetWithinDistanceID = 639;

        enum class ParamKind : std::uint8_t { kInt, kFloat, kForm, kUnsupported };

        enum class SlotCodec : std::uint8_t { kFormPointer, kAxisDirect, kUnsignedDirectToFloat, kUnsupported };

        struct ParamTypeInfo {
            RE::SCRIPT_PARAM_TYPE type;
            ParamKind kind;
            std::string_view name;
        };

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

        constexpr const ParamTypeInfo* GetParamTypeInfo(RE::SCRIPT_PARAM_TYPE a_type) noexcept {
            for (const auto& info : kParamTypeTable) {
                if (info.type == a_type) {
                    return std::addressof(info);
                }
            }
            return nullptr;
        }

        std::string GetParamTypeName(RE::SCRIPT_PARAM_TYPE a_type) {
            if (const auto* info = GetParamTypeInfo(a_type)) {
                return std::string(info->name);
            }
            return "unknown SCRIPT_PARAM_TYPE " + std::to_string(static_cast<std::uint32_t>(a_type));
        }

        struct ParameterDescriptor {
            RE::SCRIPT_PARAM_TYPE type{RE::SCRIPT_PARAM_TYPE::kChar};
            ParamKind kind{ParamKind::kUnsupported};
            SlotCodec codec{SlotCodec::kUnsupported};
            bool optional{false};
        };

        using ConditionCallback = RE::SCRIPT_FUNCTION::Condition_t*;

        struct ProviderDescriptor {
            std::uint32_t id{0};
            std::string name;
            ConditionCallback callback{nullptr};
            std::uint16_t numParams{0};
            std::array<ParameterDescriptor, 2> params{};
        };

        struct CommunityParameterDefinition {
            RE::SCRIPT_PARAM_TYPE type;
            bool optional;
        };

        struct CommunityProviderDefinition {
            std::uint32_t id;
            std::string_view name;
            ConditionCallback callback;
            std::uint16_t numParams;
            std::array<CommunityParameterDefinition, 2> params;
        };

        // Compile-time import seam. The community provider repository will populate
        // this table; DAF deliberately has no runtime DLL registration mechanism.
        inline constexpr std::array<CommunityProviderDefinition, 0> kCommunityProviders{};

        consteval bool HasValidCommunityProviders() {
            for (std::size_t i = 0; i < kCommunityProviders.size(); ++i) {
                const auto& provider = kCommunityProviders[i];
                if (provider.id < kVanillaProviderCount || !provider.callback || provider.numParams > 2) {
                    return false;
                }
                for (std::size_t j = i + 1; j < kCommunityProviders.size(); ++j) {
                    if (provider.id == kCommunityProviders[j].id) {
                        return false;
                    }
                }
            }
            return true;
        }

        static_assert(HasValidCommunityProviders());

        SlotCodec GetSlotCodec(std::uint32_t a_providerID, std::size_t a_parameterIndex,
                               const ParamTypeInfo& a_typeInfo) noexcept {
            if (a_typeInfo.kind == ParamKind::kForm) {
                return SlotCodec::kFormPointer;
            }
            if (a_providerID == kGetPosID && a_parameterIndex == 0 && a_typeInfo.type == RE::SCRIPT_PARAM_TYPE::kAxis) {
                return SlotCodec::kAxisDirect;
            }
            if (a_providerID == kGetWithinDistanceID && a_parameterIndex == 1 &&
                a_typeInfo.type == RE::SCRIPT_PARAM_TYPE::kFloat) {
                return SlotCodec::kUnsignedDirectToFloat;
            }
            return SlotCodec::kUnsupported;
        }

        std::string ProviderPrefix(std::uint32_t a_id, std::string_view a_name) {
            std::string prefix = "provider " + std::to_string(a_id);
            if (!a_name.empty()) {
                prefix += " (";
                prefix += a_name;
                prefix += ')';
            }
            return prefix;
        }

        void SetEvaluationError(std::string& a_error, std::uint32_t a_id, std::string_view a_name,
                                std::string_view a_reason) noexcept {
            try {
                a_error = ProviderPrefix(a_id, a_name) + ": " + std::string(a_reason);
            } catch (...) {
                try {
                    a_error = "provider evaluation failed while reporting an error";
                } catch (...) {
                }
            }
        }

        void SetEvaluationException(std::string& a_error, std::uint32_t a_id, std::string_view a_name,
                                    const std::exception& a_exception) noexcept {
            try {
                SetEvaluationError(a_error, a_id, a_name,
                                   std::string("evaluation raised an exception: ") + a_exception.what());
            } catch (...) {
                SetEvaluationError(a_error, a_id, a_name, "evaluation raised an exception");
            }
        }

        bool NormalizeParameters(ProviderDescriptor& a_descriptor, std::string& a_error) {
            for (std::size_t i = 0; i < a_descriptor.numParams; ++i) {
                auto& parameter = a_descriptor.params[i];
                const auto* typeInfo = GetParamTypeInfo(parameter.type);
                if (!typeInfo) {
                    a_error = ProviderPrefix(a_descriptor.id, a_descriptor.name) + ": parameter " + std::to_string(i) +
                              " uses an unknown SCRIPT_PARAM_TYPE (" +
                              std::to_string(static_cast<std::uint32_t>(parameter.type)) + ")";
                    return false;
                }

                parameter.kind = typeInfo->kind;
                if (parameter.kind == ParamKind::kUnsupported) {
                    a_error = ProviderPrefix(a_descriptor.id, a_descriptor.name) + ": parameter " + std::to_string(i) +
                              " has unsupported metadata type " + std::string(typeInfo->name);
                    return false;
                }

                parameter.codec = GetSlotCodec(a_descriptor.id, i, *typeInfo);
                if (parameter.codec == SlotCodec::kUnsupported) {
                    a_error = ProviderPrefix(a_descriptor.id, a_descriptor.name) + ": parameter " + std::to_string(i) +
                              " (" + std::string(typeInfo->name) +
                              ") has no callback-slot codec verified on both Skyrim 1.5.97 and 1.6.1170";
                    return false;
                }
            }

            if (a_descriptor.id == kGetWithinDistanceID &&
                (a_descriptor.numParams != 2 || a_descriptor.params[0].type != RE::SCRIPT_PARAM_TYPE::kObjectRef ||
                 a_descriptor.params[1].type != RE::SCRIPT_PARAM_TYPE::kFloat)) {
                a_error = ProviderPrefix(a_descriptor.id, a_descriptor.name) +
                          ": runtime metadata does not match verified signature ObjectRef, Float";
                return false;
            }

            return true;
        }

        std::optional<ProviderDescriptor> ResolveVanillaProvider(std::uint32_t a_providerID, std::string& a_error) {
            auto* table = RE::SCRIPT_FUNCTION::GetFirstScriptCommand();
            if (!table) {
                a_error = "provider " + std::to_string(a_providerID) + ": Skyrim script-command table is unavailable";
                return std::nullopt;
            }

            const auto& function = table[a_providerID];
            const std::string_view name = function.functionName ? function.functionName : "";
            const auto expectedOpcode = 0x1000u + a_providerID;
            const auto actualOpcode = static_cast<std::uint32_t>(function.output);
            if (actualOpcode != expectedOpcode) {
                a_error = ProviderPrefix(a_providerID, name) + ": opcode mismatch; expected " +
                          std::to_string(expectedOpcode) + ", got " + std::to_string(actualOpcode);
                return std::nullopt;
            }
            if (!function.conditionFunction) {
                a_error = ProviderPrefix(a_providerID, name) + ": no condition callback is available";
                return std::nullopt;
            }
            if (function.numParams > 2) {
                a_error = ProviderPrefix(a_providerID, name) + ": declares " + std::to_string(function.numParams) +
                          " parameters; DAF supports at most 2";
                return std::nullopt;
            }
            if (function.numParams != 0 && !function.params) {
                a_error = ProviderPrefix(a_providerID, name) + ": declares parameters but has null parameter metadata";
                return std::nullopt;
            }

            ProviderDescriptor descriptor{.id = a_providerID,
                                          .name = std::string(name),
                                          .callback = function.conditionFunction,
                                          .numParams = function.numParams};
            for (std::size_t i = 0; i < descriptor.numParams; ++i) {
                descriptor.params[i].type = function.params[i].paramType.get();
                descriptor.params[i].optional = function.params[i].optional;
            }

            if (!NormalizeParameters(descriptor, a_error)) {
                return std::nullopt;
            }
            return descriptor;
        }

        std::optional<ProviderDescriptor> ResolveCommunityProvider(std::uint32_t a_providerID, std::string& a_error) {
            for (const auto& definition : kCommunityProviders) {
                if (definition.id != a_providerID) {
                    continue;
                }

                ProviderDescriptor descriptor{.id = definition.id,
                                              .name = std::string(definition.name),
                                              .callback = definition.callback,
                                              .numParams = definition.numParams};
                for (std::size_t i = 0; i < descriptor.numParams; ++i) {
                    descriptor.params[i].type = definition.params[i].type;
                    descriptor.params[i].optional = definition.params[i].optional;
                }

                if (!NormalizeParameters(descriptor, a_error)) {
                    return std::nullopt;
                }
                return descriptor;
            }

            a_error = "community provider " + std::to_string(a_providerID) +
                      " is unavailable because no imported community provider definition exists";
            return std::nullopt;
        }

        std::optional<ProviderDescriptor> ResolveProvider(std::uint32_t a_providerID, std::string& a_error) {
            if (a_providerID < kVanillaProviderCount) {
                return ResolveVanillaProvider(a_providerID, a_error);
            }
            return ResolveCommunityProvider(a_providerID, a_error);
        }

        template <class T>
        void* AsForm(RE::TESForm* a_form) noexcept {
            if (auto* result = a_form->As<T>()) {
                return static_cast<void*>(result);
            }
            return nullptr;
        }

        void* ResolveFormPointer(RE::SCRIPT_PARAM_TYPE a_type, RE::TESForm* a_form) noexcept {
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

        struct ResolvedLiteral {
            const ProviderLiteral* literal{nullptr};
            RE::TESForm* form{nullptr};
        };

        std::optional<std::vector<ResolvedLiteral>> ResolveLiterals(std::span<const ProviderLiteral> a_arguments,
                                                                    std::uint32_t a_providerID,
                                                                    std::string_view a_providerName,
                                                                    std::string& a_error) {
            std::vector<ResolvedLiteral> resolved;
            resolved.reserve(a_arguments.size());
            for (std::size_t i = 0; i < a_arguments.size(); ++i) {
                ResolvedLiteral literal{.literal = std::addressof(a_arguments[i])};
                if (const auto* formString = std::get_if<std::string>(literal.literal)) {
                    literal.form = FormReader::GetFormFromString(*formString);
                    if (!literal.form) {
                        a_error = ProviderPrefix(a_providerID, a_providerName) + ": argument " + std::to_string(i) +
                                  " could not resolve Form identifier '" + *formString + "'";
                        return std::nullopt;
                    }
                }
                resolved.push_back(literal);
            }
            return resolved;
        }

        struct TargetSlot {};

        struct FormSlot {
            void* pointer{nullptr};
        };

        struct NumericSlot {
            std::uintptr_t value{0};
        };

        using CompiledSlot = std::variant<std::monostate, TargetSlot, FormSlot, NumericSlot>;

        struct BindingLayout {
            std::array<CompiledSlot, 2> slots{};
        };

        bool CompileArgument(const ProviderDescriptor& a_provider, std::size_t a_parameterIndex,
                             std::size_t a_argumentIndex, const ResolvedLiteral& a_argument, CompiledSlot& a_slot,
                             std::string& a_error) {
            const auto& parameter = a_provider.params[a_parameterIndex];
            const auto typeName = GetParamTypeName(parameter.type);
            const auto prefix = ProviderPrefix(a_provider.id, a_provider.name) + ": argument " +
                                std::to_string(a_argumentIndex) + " for parameter " + std::to_string(a_parameterIndex) +
                                " (" + typeName + ") ";

            if (parameter.kind == ParamKind::kForm) {
                if (!std::holds_alternative<std::string>(*a_argument.literal)) {
                    a_error = prefix + "must be a Form identifier string";
                    return false;
                }
                if (auto* pointer = ResolveFormPointer(parameter.type, a_argument.form)) {
                    a_slot = FormSlot{pointer};
                    return true;
                }
                a_error = prefix + "resolved to an incompatible Form";
                return false;
            }

            const auto* number = std::get_if<double>(a_argument.literal);
            if (!number) {
                a_error = prefix + "must be a number";
                return false;
            }
            if (!std::isfinite(*number)) {
                a_error = prefix + "must be finite";
                return false;
            }

            switch (parameter.codec) {
                case SlotCodec::kAxisDirect:
                    if (*number != 88.0 && *number != 89.0 && *number != 90.0) {
                        a_error = prefix + "must be an exact Axis value: X=88, Y=89, or Z=90";
                        return false;
                    }
                    a_slot = NumericSlot{static_cast<std::uintptr_t>(static_cast<std::uint32_t>(*number))};
                    return true;
                case SlotCodec::kUnsignedDirectToFloat:
                    if (*number < 0.0 || std::trunc(*number) != *number ||
                        *number > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
                        a_error = prefix + "must be a finite, non-negative integral value in the uint32 range";
                        return false;
                    }
                    a_slot = NumericSlot{static_cast<std::uintptr_t>(static_cast<std::uint32_t>(*number))};
                    return true;
                default:
                    a_error = prefix + "has no verified callback-slot codec";
                    return false;
            }
        }

        std::optional<BindingLayout> CompileLayout(const ProviderDescriptor& a_provider,
                                                   std::span<const ResolvedLiteral> a_arguments, bool a_insertTarget,
                                                   std::string& a_error) {
            BindingLayout layout;
            std::size_t firstLiteralParameter = 0;
            if (a_insertTarget) {
                if (a_provider.numParams == 0 || a_provider.params[0].type != RE::SCRIPT_PARAM_TYPE::kObjectRef) {
                    a_error = "parameter 0 is not ObjectRef and cannot receive Target";
                    return std::nullopt;
                }
                layout.slots[0] = TargetSlot{};
                firstLiteralParameter = 1;
            }

            const auto availableLiteralParameters =
                static_cast<std::size_t>(a_provider.numParams) - firstLiteralParameter;
            if (a_arguments.size() > availableLiteralParameters) {
                a_error = "received " + std::to_string(a_arguments.size()) +
                          " author arguments but this binding accepts at most " +
                          std::to_string(availableLiteralParameters);
                return std::nullopt;
            }

            for (std::size_t i = 0; i < a_arguments.size(); ++i) {
                const auto parameterIndex = firstLiteralParameter + i;
                if (!CompileArgument(a_provider, parameterIndex, i, a_arguments[i], layout.slots[parameterIndex],
                                     a_error)) {
                    return std::nullopt;
                }
            }

            for (std::size_t i = firstLiteralParameter + a_arguments.size(); i < a_provider.numParams; ++i) {
                if (!a_provider.params[i].optional) {
                    a_error = "missing required parameter " + std::to_string(i) + " (" +
                              GetParamTypeName(a_provider.params[i].type) + ")";
                    return std::nullopt;
                }
            }

            return layout;
        }

        void* MaterializeSlot(const CompiledSlot& a_slot, RE::TESObjectREFR* a_target) noexcept {
            if (std::holds_alternative<TargetSlot>(a_slot)) {
                return static_cast<void*>(a_target);
            }
            if (const auto* form = std::get_if<FormSlot>(std::addressof(a_slot))) {
                return form->pointer;
            }
            if (const auto* number = std::get_if<NumericSlot>(std::addressof(a_slot))) {
                return reinterpret_cast<void*>(number->value);
            }
            return nullptr;
        }
    }

    struct ProviderCall {
        detail::ProviderDescriptor provider;
        std::optional<detail::BindingLayout> withoutTarget;
        std::optional<detail::BindingLayout> withTarget;
        bool targetEligible{false};
    };

    std::shared_ptr<const ProviderCall> CompileCall(std::uint32_t a_providerID,
                                                    std::span<const ProviderLiteral> a_arguments,
                                                    std::string& a_error) {
        a_error.clear();
        try {
            auto provider = detail::ResolveProvider(a_providerID, a_error);
            if (!provider) {
                return {};
            }

            auto resolvedArguments = detail::ResolveLiterals(a_arguments, provider->id, provider->name, a_error);
            if (!resolvedArguments) {
                return {};
            }

            ProviderCall call{.provider = std::move(*provider)};
            call.targetEligible =
                call.provider.numParams != 0 && call.provider.params[0].type == RE::SCRIPT_PARAM_TYPE::kObjectRef;

            std::string withoutTargetError;
            call.withoutTarget = detail::CompileLayout(call.provider, *resolvedArguments, false, withoutTargetError);

            std::string withTargetError;
            if (call.targetEligible) {
                call.withTarget = detail::CompileLayout(call.provider, *resolvedArguments, true, withTargetError);
            }

            if (!call.withoutTarget && !call.withTarget) {
                a_error = detail::ProviderPrefix(call.provider.id, call.provider.name) +
                          ": no valid argument binding; without Target: " + withoutTargetError;
                if (call.targetEligible) {
                    a_error += "; with Target: " + withTargetError;
                }
                return {};
            }

            a_error.clear();
            return std::make_shared<const ProviderCall>(std::move(call));
        } catch (const std::exception& exception) {
            a_error =
                "provider " + std::to_string(a_providerID) + ": compilation raised an exception: " + exception.what();
        } catch (...) {
            a_error = "provider " + std::to_string(a_providerID) + ": compilation raised an unknown exception";
        }
        return {};
    }

    bool Evaluate(const ProviderCall& a_call, RE::TESObjectREFR* a_subject, RE::TESObjectREFR* a_target,
                  float& a_result, std::string& a_error) noexcept {
        a_error.clear();
        try {
            if (!a_subject) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name, "Subject is missing");
                return false;
            }
            if (!a_call.provider.callback) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           "condition callback is missing");
                return false;
            }

            const auto* layout = a_target && a_call.targetEligible ? std::addressof(a_call.withTarget)
                                                                   : std::addressof(a_call.withoutTarget);
            if (!*layout) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           a_target && a_call.targetEligible ? "no viable Target binding layout"
                                                                             : "no viable no-Target binding layout");
                return false;
            }

            auto* parameter1 = detail::MaterializeSlot((*layout)->slots[0], a_target);
            auto* parameter2 = detail::MaterializeSlot((*layout)->slots[1], a_target);
            double nativeResult = 0.0;
            if (!a_call.provider.callback(a_subject, parameter1, parameter2, nativeResult)) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           "condition callback returned false");
                return false;
            }
            if (!std::isfinite(nativeResult)) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           "condition callback returned a non-finite result");
                return false;
            }
            if (nativeResult < static_cast<double>(std::numeric_limits<float>::lowest()) ||
                nativeResult > static_cast<double>(std::numeric_limits<float>::max())) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           "condition callback result is outside the finite float range");
                return false;
            }

            const auto converted = static_cast<float>(nativeResult);
            if (!std::isfinite(converted)) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           "condition callback result became non-finite during float conversion");
                return false;
            }
            a_result = converted;
            return true;
        } catch (const std::exception& exception) {
            detail::SetEvaluationException(a_error, a_call.provider.id, a_call.provider.name, exception);
        } catch (...) {
            detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                       "evaluation raised an unknown exception");
        }
        return false;
    }
}
