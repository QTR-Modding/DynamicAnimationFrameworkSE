#include "Variables/Providers.h"
#include "ProviderParameters.h"

namespace Variables::Providers {
    namespace detail {
        constexpr std::uint32_t kGetPosID = 6;
        constexpr std::uint32_t kGetWithinDistanceID = 639;

        enum class SlotCodec : std::uint8_t { kFormPointer, kAxisDirect, kUnsignedDirectToFloat, kUnsupported };

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
            if (a_providerID == kGetPosID && a_parameterIndex == 0 && a_typeInfo.type == RE::SCRIPT_PARAM_TYPE::kAxis) {
                return SlotCodec::kAxisDirect;
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

        bool NormalizeParameters(ProviderDescriptor& a_descriptor, std::string& a_error) {
            for (std::size_t i = 0; i < a_descriptor.numParams; ++i) {
                auto& parameter = a_descriptor.params[i];
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

        const RE::SCRIPT_FUNCTION* ResolveVanillaFunction(const std::uint32_t a_providerID, std::string& a_error) {
            if (a_providerID >= RE::SCRIPT_FUNCTION::Commands::kScriptCommandsEnd) {
                a_error = "provider " + std::to_string(a_providerID) +
                          ": IDs 736 and above are reserved and unavailable in this build";
                return nullptr;
            }
            auto table = RE::SCRIPT_FUNCTION::GetFirstScriptCommand();
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
        ResolveVanillaProvider(const std::uint32_t a_providerID, std::string& a_error) {
            const auto function = ResolveVanillaFunction(a_providerID, a_error);
            if (!function) {
                return std::nullopt;
            }
            const std::string_view name = function->functionName ? function->functionName : "";
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

            ProviderDescriptor descriptor{
                .id = a_providerID, .name = std::string(name), .numParams = function->numParams};
            for (std::size_t i = 0; i < descriptor.numParams; ++i) {
                descriptor.params[i].type = function->params[i].paramType.get();
                descriptor.params[i].optional = function->params[i].optional;
            }

            if (!NormalizeParameters(descriptor, a_error)) {
                return std::nullopt;
            }
            return descriptor;
        }

        struct TargetSlot {
        };

        using CompiledSlot = std::variant<std::monostate, TargetSlot, RE::TESForm*, std::uintptr_t>;
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
                case SlotCodec::kAxisDirect:
                    if (*number != 88.0 && *number != 89.0 && *number != 90.0) {
                        a_error = prefix + "must be an exact Axis value: X=88, Y=89, or Z=90";
                        return false;
                    }
                    a_slot = static_cast<std::uintptr_t>(static_cast<std::uint32_t>(*number));
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
            std::size_t firstLiteralParameter = 0;
            if (a_insertTarget) {
                layout[0] = TargetSlot{};
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
                if (!CompileArgument(a_provider, parameterIndex, i, a_arguments[i], layout[parameterIndex], a_error)) {
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

        bool MaterializeSlot(const ProviderDescriptor& a_provider, const std::size_t a_parameterIndex,
                             const CompiledSlot& a_slot, RE::TESObjectREFR* a_target, void*& a_result,
                             std::string& a_error) {
            a_result = nullptr;
            if (std::holds_alternative<TargetSlot>(a_slot)) {
                if (!a_target) {
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
            auto provider = detail::ResolveVanillaProvider(a_providerID, a_error);
            if (!provider) {
                return {};
            }

            ProviderCall call{.provider = std::move(*provider)};
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
            const auto function = detail::ResolveVanillaFunction(a_call.provider.id, a_error);
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