#include "Variables/Providers.h"
#include "ProviderParameters.h"
#include "CommunityFunctionsSE/API.hpp"

namespace Variables::Providers {
    namespace detail {
        constexpr std::uint32_t kGetWithinDistanceID = 639;

        enum class SlotCodec : std::uint8_t {
            kTarget,
            kFormPointer,
            kInteger,
            kFloat,
            kUnsignedDirectToFloat,
            kUnsupported
        };

        struct ParameterDescriptor {
            RE::SCRIPT_PARAM_TYPE type{RE::SCRIPT_PARAM_TYPE::kChar};
            SlotCodec codec{SlotCodec::kUnsupported};
            bool optional{false};
        };

        struct ProviderDescriptor {
            std::uint32_t id{0};
            std::string name;
            std::uint16_t numParams{0};
            std::array<ParameterDescriptor, 2> params{};
        };

        SlotCodec GetSlotCodec(const std::uint32_t a_providerID, const std::size_t a_parameterIndex,
                               const ParamTypeInfo& a_typeInfo) noexcept {
            if (a_typeInfo.kind == ParamKind::kForm) {
                return SlotCodec::kFormPointer;
            }
            if (a_typeInfo.kind == ParamKind::kInt) {
                return SlotCodec::kInteger;
            }
            if (a_providerID == kGetWithinDistanceID && a_parameterIndex == 1 &&
                a_typeInfo.type == RE::SCRIPT_PARAM_TYPE::kFloat) {
                return SlotCodec::kUnsignedDirectToFloat;
            }
            return SlotCodec::kUnsupported;
        }

        std::string ProviderPrefix(const std::uint32_t a_id, const std::string_view a_name) {
            std::string prefix = "provider " + std::to_string(a_id);
            if (!a_name.empty()) {
                prefix += " (";
                prefix += a_name;
                prefix += ')';
            }
            return prefix;
        }

        void SetEvaluationError(std::string& a_error, const std::uint32_t a_id, const std::string_view a_name,
                                const std::string_view a_reason) noexcept {
            try {
                a_error = ProviderPrefix(a_id, a_name) + ": " + std::string(a_reason);
            } catch (...) {
                try {
                    a_error = "provider evaluation failed while reporting an error";
                } catch (...) {
                }
            }
        }

        void SetEvaluationException(std::string& a_error, const std::uint32_t a_id, const std::string_view a_name,
                                    const std::exception& a_exception) noexcept {
            try {
                SetEvaluationError(a_error, a_id, a_name,
                                   std::string("evaluation raised an exception: ") + a_exception.what());
            } catch (...) {
                SetEvaluationError(a_error, a_id, a_name, "evaluation raised an exception");
            }
        }

        bool NormalizeParameters(
            ProviderDescriptor& a_descriptor,
            const std::optional<std::array<CommunityFunctionsSE::ConditionParameter, 2>>& a_communityParameters,
            std::string& a_error) {
            for (std::size_t i = 0; i < a_descriptor.numParams; ++i) {
                auto& parameter = a_descriptor.params[i];
                if (a_communityParameters) {
                    switch ((*a_communityParameters)[i]) {
                        case CommunityFunctionsSE::ConditionParameter::kTarget:
                            parameter.codec = SlotCodec::kTarget;
                            break;
                        case CommunityFunctionsSE::ConditionParameter::kForm:
                        case CommunityFunctionsSE::ConditionParameter::kReference:
                            parameter.codec = SlotCodec::kFormPointer;
                            break;
                        case CommunityFunctionsSE::ConditionParameter::kInteger:
                            parameter.codec = SlotCodec::kInteger;
                            break;
                        case CommunityFunctionsSE::ConditionParameter::kFloat:
                            parameter.codec = SlotCodec::kFloat;
                            break;
                        default:
                            a_error = ProviderPrefix(a_descriptor.id, a_descriptor.name) + ": parameter " +
                                      std::to_string(i) + " has no community binding metadata";
                            return false;
                    }
                    continue;
                }

                const auto typeInfo = GetParamTypeInfo(parameter.type);
                if (!typeInfo) {
                    a_error = ProviderPrefix(a_descriptor.id, a_descriptor.name) + ": parameter " + std::to_string(i) +
                              " uses an unknown SCRIPT_PARAM_TYPE (" +
                              std::to_string(static_cast<std::uint32_t>(parameter.type)) + ")";
                    return false;
                }

                if (typeInfo->kind == ParamKind::kUnsupported) {
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

        const RE::SCRIPT_FUNCTION* ResolveFunction(const std::uint32_t a_providerID, std::string& a_error) {
            if (a_providerID >= CommunityFunctionsSE::kFunctionBase &&
                a_providerID < CommunityFunctionsSE::kFunctionLimit) {
                const auto function = CommunityFunctionsSE::GetFunction(a_providerID);
                if (!function) {
                    a_error = "provider " + std::to_string(a_providerID) + ": community function is unavailable";
                }
                return function;
            }
            if (a_providerID >= RE::SCRIPT_FUNCTION::Commands::kScriptCommandsEnd) {
                a_error = "provider " + std::to_string(a_providerID) +
                          ": provider ID is reserved";
                return nullptr;
            }
            const auto table = RE::SCRIPT_FUNCTION::GetFirstScriptCommand();
            if (!table) {
                a_error = "provider " + std::to_string(a_providerID) + ": Skyrim script-command table is unavailable";
                return nullptr;
            }

            const auto& function = table[a_providerID];
            const std::string_view name = function.functionName ? function.functionName : "";
            const auto expectedOpcode = RE::SCRIPT_FUNCTION::Commands::kScriptOpBase + a_providerID;
            const auto actualOpcode = static_cast<std::uint32_t>(function.output);
            if (actualOpcode != expectedOpcode) {
                a_error = ProviderPrefix(a_providerID, name) + ": opcode mismatch; expected " +
                          std::to_string(expectedOpcode) + ", got " + std::to_string(actualOpcode);
                return nullptr;
            }
            return std::addressof(function);
        }

        std::optional<ProviderDescriptor>
        ResolveProvider(const std::uint32_t a_providerID, std::string& a_error) {
            const auto function = ResolveFunction(a_providerID, a_error);
            if (!function) {
                return std::nullopt;
            }
            const std::string_view name = function->functionName ? function->functionName : "";
            switch (static_cast<RE::FUNCTION_DATA::FunctionID>(a_providerID)) {
                case RE::FUNCTION_DATA::FunctionID::kEPAlchemyGetMakingPoison:
                case RE::FUNCTION_DATA::FunctionID::kEPAlchemyEffectHasKeyword:
                case RE::FUNCTION_DATA::FunctionID::kEPTemperingItemIsEnchanted:
                case RE::FUNCTION_DATA::FunctionID::kEPTemperingItemHasKeyword:
                case RE::FUNCTION_DATA::FunctionID::kEPModSkillUsage_IsAdvanceSkill:
                case RE::FUNCTION_DATA::FunctionID::kEPModSkillUsage_AdvanceObjectHasKeyword:
                case RE::FUNCTION_DATA::FunctionID::kEPModSkillUsage_IsAdvanceAction:
                case RE::FUNCTION_DATA::FunctionID::kEPMagic_SpellHasKeyword:
                case RE::FUNCTION_DATA::FunctionID::kEPMagic_SpellHasSkill:
                    a_error = ProviderPrefix(a_providerID, name) +
                              ": requires perk-entry-point context that DAF cannot supply";
                    return std::nullopt;
                default:
                    break;
            }
            if (!function->conditionFunction) {
                a_error = ProviderPrefix(a_providerID, name) + ": no condition callback is available";
                return std::nullopt;
            }
            if (function->numParams > 2) {
                a_error = ProviderPrefix(a_providerID, name) + ": declares " + std::to_string(function->numParams) +
                          " parameters; DAF supports at most 2";
                return std::nullopt;
            }
            if (function->numParams != 0 && !function->params) {
                a_error = ProviderPrefix(a_providerID, name) + ": declares parameters but has null parameter metadata";
                return std::nullopt;
            }

            const auto communityParameters = CommunityFunctionsSE::GetConditionParameters(a_providerID);
            ProviderDescriptor descriptor{
                .id = a_providerID, .name = std::string(name), .numParams = function->numParams};
            for (std::size_t i = 0; i < descriptor.numParams; ++i) {
                descriptor.params[i].type = function->params[i].paramType.get();
                descriptor.params[i].optional = function->params[i].optional;
            }

            if (!NormalizeParameters(descriptor, communityParameters, a_error)) {
                return std::nullopt;
            }
            return descriptor;
        }

        struct TargetSlot {
        };

        using CompiledSlot =
            std::variant<std::monostate, TargetSlot, RE::TESForm*, std::int32_t, float, std::uintptr_t>;
        using BindingLayout = std::array<CompiledSlot, 2>;

        bool CompileArgument(const ProviderDescriptor& a_provider, const std::size_t a_parameterIndex,
                             const std::size_t a_argumentIndex, const ProviderLiteral& a_argument, CompiledSlot& a_slot,
                             std::string& a_error) {
            const auto& parameter = a_provider.params[a_parameterIndex];
            const auto typeName = GetParamTypeName(parameter.type);
            const auto prefix = ProviderPrefix(a_provider.id, a_provider.name) + ": argument " +
                                std::to_string(a_argumentIndex) + " for parameter " + std::to_string(a_parameterIndex) +
                                " (" + typeName + ") ";

            if (parameter.codec == SlotCodec::kFormPointer) {
                const auto form = std::get_if<RE::TESForm*>(std::addressof(a_argument));
                if (!form) {
                    a_error = prefix + "must be a Form identifier string";
                    return false;
                }
                if (!ResolveFormPointer(parameter.type, *form)) {
                    a_error = prefix + "resolved to an incompatible Form";
                    return false;
                }
                a_slot = *form;
                return true;
            }

            const auto number = std::get_if<double>(std::addressof(a_argument));
            if (!number) {
                a_error = prefix + "must be a number";
                return false;
            }
            if (!std::isfinite(*number)) {
                a_error = prefix + "must be finite";
                return false;
            }

            switch (parameter.codec) {
                case SlotCodec::kInteger:
                    if (std::trunc(*number) != *number ||
                        *number < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
                        *number > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
                        a_error = prefix + "must be an integral value in the int32 range";
                        return false;
                    }
                    a_slot = static_cast<std::int32_t>(*number);
                    return true;
                case SlotCodec::kFloat:
                    if (*number < static_cast<double>(std::numeric_limits<float>::lowest()) ||
                        *number > static_cast<double>(std::numeric_limits<float>::max())) {
                        a_error = prefix + "must be in the finite float range";
                        return false;
                    }
                    a_slot = static_cast<float>(*number);
                    return true;
                case SlotCodec::kUnsignedDirectToFloat:
                    if (*number < 0.0 || std::trunc(*number) != *number ||
                        *number > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
                        a_error = prefix + "must be a finite, non-negative integral value in the uint32 range";
                        return false;
                    }
                    a_slot = static_cast<std::uintptr_t>(static_cast<std::uint32_t>(*number));
                    return true;
                default:
                    a_error = prefix + "has no verified callback-slot codec";
                    return false;
            }
        }

        std::optional<BindingLayout> CompileLayout(const ProviderDescriptor& a_provider,
                                                   const std::span<const ProviderLiteral> a_arguments,
                                                   const bool a_insertTarget,
                                                   std::string& a_error) {
            BindingLayout layout;
            std::size_t argumentIndex = 0;
            for (std::size_t parameterIndex = 0; parameterIndex < a_provider.numParams; ++parameterIndex) {
                const auto& parameter = a_provider.params[parameterIndex];
                if (parameter.codec == SlotCodec::kTarget || (a_insertTarget && parameterIndex == 0)) {
                    layout[parameterIndex] = TargetSlot{};
                    continue;
                }
                if (argumentIndex < a_arguments.size()) {
                    if (!CompileArgument(a_provider, parameterIndex, argumentIndex, a_arguments[argumentIndex],
                                         layout[parameterIndex], a_error)) {
                        return std::nullopt;
                    }
                    ++argumentIndex;
                } else if (!parameter.optional) {
                    a_error = "missing required parameter " + std::to_string(parameterIndex) + " (" +
                              GetParamTypeName(parameter.type) + ")";
                    return std::nullopt;
                }
            }
            if (argumentIndex != a_arguments.size()) {
                a_error = "received " + std::to_string(a_arguments.size()) +
                          " author arguments but this binding accepts at most " + std::to_string(argumentIndex);
                return std::nullopt;
            }

            return layout;
        }

        bool MaterializeSlot(const ProviderDescriptor& a_provider, const std::size_t a_parameterIndex,
                             const CompiledSlot& a_slot, RE::TESObjectREFR* a_target, void*& a_result,
                             std::string& a_error) {
            a_result = nullptr;
            if (std::holds_alternative<TargetSlot>(a_slot)) {
                if (!a_target && !a_provider.params[a_parameterIndex].optional) {
                    SetEvaluationError(a_error, a_provider.id, a_provider.name, "Target is unavailable");
                    return false;
                }
                a_result = static_cast<void*>(a_target);
                return true;
            }
            if (const auto form = std::get_if<RE::TESForm*>(std::addressof(a_slot))) {
                a_result = ResolveFormPointer(a_provider.params[a_parameterIndex].type, *form);
                if (!a_result) {
                    SetEvaluationError(a_error, a_provider.id, a_provider.name,
                                       "Form argument for parameter " + std::to_string(a_parameterIndex) +
                                       " has an incompatible runtime type");
                    return false;
                }
                return true;
            }
            if (const auto integer = std::get_if<std::int32_t>(std::addressof(a_slot))) {
                a_result = CommunityFunctionsSE::EncodeParameter(*integer);
                return true;
            }
            if (const auto number = std::get_if<float>(std::addressof(a_slot))) {
                a_result = CommunityFunctionsSE::EncodeParameter(*number);
                return true;
            }
            if (const auto number = std::get_if<std::uintptr_t>(std::addressof(a_slot))) {
                a_result = reinterpret_cast<void*>(*number);
            }
            return true;
        }
    }

    struct ProviderCall {
        detail::ProviderDescriptor provider;
        std::optional<detail::BindingLayout> withoutTarget;
        std::optional<detail::BindingLayout> withTarget;
        bool targetEligible{false};
    };

    std::shared_ptr<const ProviderCall> CompileCall(const std::uint32_t a_providerID,
                                                    const std::span<const ProviderLiteral> a_arguments,
                                                    std::string& a_error) {
        a_error.clear();
        try {
            const auto provider = detail::ResolveProvider(a_providerID, a_error);
            if (!provider) {
                return {};
            }

            ProviderCall call{.provider = std::move(*provider)};
            if (call.provider.id >= CommunityFunctionsSE::kFunctionBase &&
                call.provider.id < CommunityFunctionsSE::kFunctionLimit) {
                for (std::size_t i = 0; i < call.provider.numParams; ++i) {
                    if (call.provider.params[i].codec == detail::SlotCodec::kTarget) {
                        call.targetEligible = true;
                        break;
                    }
                }

                std::string layoutError;
                call.withoutTarget = detail::CompileLayout(call.provider, a_arguments, false, layoutError);
                if (!call.withoutTarget) {
                    a_error = detail::ProviderPrefix(call.provider.id, call.provider.name) + ": " + layoutError;
                    return {};
                }
                if (call.targetEligible) {
                    call.withTarget = call.withoutTarget;
                }
                return std::make_shared<const ProviderCall>(std::move(call));
            }

            call.targetEligible =
                call.provider.numParams != 0 && call.provider.params[0].type == RE::SCRIPT_PARAM_TYPE::kObjectRef;

            std::string withoutTargetError;
            call.withoutTarget = detail::CompileLayout(call.provider, a_arguments, false, withoutTargetError);

            std::string withTargetError;
            if (call.targetEligible) {
                call.withTarget = detail::CompileLayout(call.provider, a_arguments, true, withTargetError);
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
            const auto function = detail::ResolveFunction(a_call.provider.id, a_error);
            if (!function || !function->conditionFunction) {
                if (function) {
                    detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                               "condition callback is missing");
                }
                return false;
            }

            const auto layout = a_target && a_call.targetEligible
                                    ? std::addressof(a_call.withTarget)
                                    : std::addressof(a_call.withoutTarget);
            if (!*layout) {
                detail::SetEvaluationError(a_error, a_call.provider.id, a_call.provider.name,
                                           a_target && a_call.targetEligible
                                               ? "no viable Target binding layout"
                                               : "no viable no-Target binding layout");
                return false;
            }

            void* parameter1;
            void* parameter2;
            if (!detail::MaterializeSlot(a_call.provider, 0, (**layout)[0], a_target, parameter1, a_error) ||
                !detail::MaterializeSlot(a_call.provider, 1, (**layout)[1], a_target, parameter2, a_error)) {
                return false;
            }
            double nativeResult = 0.0;
            if (!function->conditionFunction(a_subject, parameter1, parameter2, nativeResult)) {
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

            a_result = static_cast<float>(nativeResult);
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
