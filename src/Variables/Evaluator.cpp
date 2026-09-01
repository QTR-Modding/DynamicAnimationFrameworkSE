#include "Variables/Evaluator.h"
#include "Variables/Providers.h"


namespace Variables {
    namespace {
        struct PreparedOutput {
            std::string_view name;
            GraphType type;
            std::variant<bool, std::int32_t, float> value;
        };

        struct EvaluationFailure {
            std::string definition;
            std::string reason;
        };

        class Evaluation {
        public:
            Evaluation(const CompiledGroup& a_group, RE::TESObjectREFR* a_subject, RE::TESObjectREFR* a_target)
                : group(a_group),
                  subject(a_subject),
                  target(a_target),
                  holder(a_subject ? static_cast<RE::IAnimationGraphManagerHolder*>(a_subject) : nullptr) {
            }

            bool Run(const std::optional<std::size_t> a_durationIndex, unsigned int& a_duration) {
                if (!subject || !holder) return Fail("animation graph subject is unavailable");
                if (a_durationIndex) {
                    float duration;
                    if (!Calculate(*a_durationIndex, duration)) return false;
                    const auto durationValue = std::round(static_cast<double>(duration));
                    if (duration < 0.0f ||
                        durationValue > static_cast<double>(std::numeric_limits<unsigned int>::max())) {
                        return Fail("duration is outside the supported millisecond range",
                                    group.definitions[*a_durationIndex].name);
                    }
                    a_duration = static_cast<unsigned int>(durationValue);
                }
                std::vector<PreparedOutput> prepared;
                prepared.reserve(group.graph_variable_indices.size());
                for (const auto index : group.graph_variable_indices) {
                    if (index >= group.definitions.size()) {
                        return Fail("root definition index is out of range");
                    }
                    const auto& definition = group.definitions[index];
                    if (!definition.output_type) {
                        return Fail("root definition has no output type", definition.name);
                    }
                    float value;
                    if (!Calculate(index, value)) {
                        return false;
                    }
#ifndef NDEBUG
                    logger::trace("{} [{}] = {}", group.context, definition.name, value);
#endif
                    if (!Prepare(definition, value, prepared)) return false;
                }
                for (const auto& output : prepared) {
                    const RE::BSFixedString name(output.name);
                    bool success = false;
                    switch (output.type) {
                        case GraphType::kBool:
                            success = holder->SetGraphVariableBool(name, std::get<bool>(output.value));
                            break;
                        case GraphType::kInt:
                            success = holder->SetGraphVariableInt(name, std::get<std::int32_t>(output.value));
                            break;
                        case GraphType::kFloat:
                            success = holder->SetGraphVariableFloat(name, std::get<float>(output.value));
                            break;
                    }
                    if (!success) {
                        return Fail("graph variable setter returned false", output.name);
                    }
                }
                return true;
            }

            [[nodiscard]] const std::optional<EvaluationFailure>& Failure() const noexcept { return failure; }

        private:
            bool Fail(std::string a_reason, const std::string_view a_definition = {}) {
                if (!failure)
                    failure = EvaluationFailure{.definition = std::string(a_definition.empty() ? currentDefinition : a_definition),
                                                .reason = std::move(a_reason)};
                return false;
            }

            bool Calculate(const std::size_t a_index, float& a_result) {
                if (a_index >= group.definitions.size()) {
                    return Fail("referenced definition index is out of range");
                }
                const auto& definition = group.definitions[a_index];
                const auto previousDefinition = currentDefinition;
                currentDefinition = definition.name;
                struct Restore {
                    std::string_view& slot;
                    std::string_view value;
                    ~Restore() { slot = value; }
                } restore{.slot = currentDefinition, .value = previousDefinition};
                bool gatePassed = definition.conditions.empty();
                if (!gatePassed) {
                    for (const auto perk : definition.conditions) {
                        if (perk->perkConditions.IsTrue(subject, target)) {
                            gatePassed = true;
                            break;
                        }
                    }
                }
                if (!gatePassed) {
                    if (!definition.else_val) {
                        a_result = 0.0f;
                        return true;
                    }
                    if (!EvaluateSource(*definition.else_val, a_result)) return false;
                    return std::isfinite(a_result) || Fail("else value is not finite");
                }
                if (!EvaluateSource(definition.value, a_result)) {
                    return false;
                }
                if (!std::isfinite(a_result)) return Fail("value is not finite");
                for (const auto& operation : definition.post) {
                    if (!Apply(operation, a_result)) {
                        return false;
                    }
                    if (!std::isfinite(a_result)) return Fail("post operation produced a non-finite value");
                }
                return true;
            }

            bool EvaluateSource(const ValueSource& a_source, float& a_result) {
                return std::visit(
                    [this, &a_result]<typename T>(const T& a_value) -> bool {
                        if constexpr (std::is_same_v<T, float>) {
                            a_result = a_value;
                            return true;
                        } else if constexpr (std::is_same_v<T, std::size_t>) {
                            return Calculate(a_value, a_result);
                        } else if constexpr (std::is_same_v<T, RE::TESGlobal*>) {
                            a_result = a_value->value;
                            return std::isfinite(a_result) || Fail("TESGlobal source is not finite");
                        } else if constexpr (std::is_same_v<T, std::shared_ptr<const Providers::ProviderCall>>) {
                            std::string error;
                            if (!Providers::Evaluate(*a_value, subject, target, a_result, error)) {
                                return Fail(error.empty() ? "provider evaluation failed" : std::move(error));
                            }
                            return std::isfinite(a_result) || Fail("provider result is not finite");
                        } else if constexpr (std::is_same_v<T, GraphRead>) {
                            const RE::BSFixedString name(a_value.name);
                            switch (a_value.type) {
                                case GraphType::kBool: {
                                    bool value = false;
                                    if (!holder->GetGraphVariableBool(name, value)) {
                                        return Fail("bool graph variable getter returned false for '" + a_value.name +
                                                    "'");
                                    }
                                    a_result = value ? 1.0f : 0.0f;
                                    return true;
                                }
                                case GraphType::kInt: {
                                    std::int32_t value = 0;
                                    if (!holder->GetGraphVariableInt(name, value)) {
                                        return Fail("int graph variable getter returned false for '" + a_value.name +
                                                    "'");
                                    }
                                    a_result = static_cast<float>(value);
                                    return true;
                                }
                                case GraphType::kFloat:
                                    if (!holder->GetGraphVariableFloat(name, a_result)) {
                                        return Fail("float graph variable getter returned false for '" + a_value.name +
                                                    "'");
                                    }
                                    return std::isfinite(a_result) || Fail("float graph variable is not finite");
                            }
                        }
                        return Fail("source type is unsupported");
                    },
                    a_source);
            }

            bool EvaluateOperand(const Operand& a_operand, float& a_result) {
                return std::visit(
                           [this, &a_result]<typename T>(const T& a_value) -> bool {
                               if constexpr (std::is_same_v<T, float>) {
                                   a_result = a_value;
                                   return true;
                               } else {
                                   return Calculate(a_value, a_result);
                               }
                           },
                           a_operand) &&
                       (std::isfinite(a_result) || Fail("post operand is not finite"));
            }

            bool Apply(const PostOp& a_operation, float& a_value) {
                bool unary = true;
                switch (a_operation.type) {
                    case PostOperationType::kAbs:
                        a_value = std::abs(a_value);
                        break;
                    case PostOperationType::kFloor:
                        a_value = std::floor(a_value);
                        break;
                    case PostOperationType::kCeil:
                        a_value = std::ceil(a_value);
                        break;
                    case PostOperationType::kLog:
                        if (a_value <= 0.0f) return Fail("log input is not positive");
                        a_value = std::log(a_value);
                        break;
                    case PostOperationType::kExp:
                        a_value = std::exp(a_value);
                        break;
                    case PostOperationType::kSin:
                        a_value = std::sin(a_value);
                        break;
                    case PostOperationType::kCos:
                        a_value = std::cos(a_value);
                        break;
                    case PostOperationType::kTan:
                        a_value = std::tan(a_value);
                        break;
                    case PostOperationType::kAsin:
                        if (a_value < -1.0f || a_value > 1.0f) return Fail("asin input is outside [-1, 1]");
                        a_value = std::asin(a_value);
                        break;
                    case PostOperationType::kAcos:
                        if (a_value < -1.0f || a_value > 1.0f) return Fail("acos input is outside [-1, 1]");
                        a_value = std::acos(a_value);
                        break;
                    case PostOperationType::kAtan:
                        a_value = std::atan(a_value);
                        break;
                    default:
                        unary = false;
                        break;
                }
                if (unary) return std::isfinite(a_value) || Fail("unary post operation produced a non-finite value");

                float first;
                if (!EvaluateOperand(a_operation.first, first)) {
                    return false;
                }
                switch (a_operation.type) {
                    case PostOperationType::kAdd:
                        a_value += first;
                        break;
                    case PostOperationType::kSubtract:
                        a_value -= first;
                        break;
                    case PostOperationType::kMultiply:
                        a_value *= first;
                        break;
                    case PostOperationType::kDivide:
                        a_value = first == 0.0f ? 0.0f : a_value / first;
                        break;
                    case PostOperationType::kPow:
                        a_value = std::pow(a_value, first);
                        break;
                    case PostOperationType::kRound: {
                        constexpr auto minimum = static_cast<float>(std::numeric_limits<std::int32_t>::min());
                        constexpr auto maximumExclusive = -minimum;
                        if (first < minimum || first >= maximumExclusive) {
                            return Fail("round precision is outside the signed 32-bit range");
                        }
                        const auto precision = static_cast<std::int32_t>(first);
                        const auto quantum = std::pow(10.0, -static_cast<double>(precision));
                        if (precision > 0 && quantum < std::numeric_limits<float>::denorm_min()) {
                            break;
                        }
                        const auto scale = std::pow(10.0, precision);
                        if (!std::isfinite(scale) || scale == 0.0) {
                            return Fail("round precision cannot be represented");
                        }
                        const auto rounded = std::round(static_cast<double>(a_value) * scale) / scale;
                        if (!std::isfinite(rounded) ||
                            rounded < static_cast<double>(std::numeric_limits<float>::lowest()) ||
                            rounded > static_cast<double>(std::numeric_limits<float>::max())) {
                            return Fail("round produced a value outside the finite float range");
                        }
                        a_value = static_cast<float>(rounded);
                        break;
                    }
                    case PostOperationType::kMin:
                        a_value = std::min(a_value, first);
                        break;
                    case PostOperationType::kMax:
                        a_value = std::max(a_value, first);
                        break;
                    case PostOperationType::kAtan2:
                        a_value = a_value == 0.0f && first == 0.0f ? 0.0f : std::atan2(a_value, first);
                        break;
                    case PostOperationType::kClamp: {
                        if (!a_operation.second) {
                            return Fail("clamp maximum operand is missing");
                        }
                        float second;
                        if (!EvaluateOperand(*a_operation.second, second)) {
                            return false;
                        }
                        if (first > second) return Fail("clamp minimum is greater than maximum");
                        a_value = std::clamp(a_value, first, second);
                        break;
                    }
                    case PostOperationType::kLessThan:
                    case PostOperationType::kLessThanOrEqual:
                    case PostOperationType::kGreaterThan:
                    case PostOperationType::kGreaterThanOrEqual:
                    case PostOperationType::kEqual:
                    case PostOperationType::kNotEqual: {
                        const auto left = static_cast<double>(a_value);
                        const auto right = static_cast<double>(first);
                        const auto scale = std::max({1.0, std::abs(left), std::abs(right)});
                        const auto equal = std::abs(left - right) <= std::numeric_limits<float>::epsilon() * scale;
                        switch (a_operation.type) {
                            case PostOperationType::kLessThan:
                                a_value = left < right && !equal ? 1.0f : 0.0f;
                                break;
                            case PostOperationType::kLessThanOrEqual:
                                a_value = left < right || equal ? 1.0f : 0.0f;
                                break;
                            case PostOperationType::kGreaterThan:
                                a_value = left > right && !equal ? 1.0f : 0.0f;
                                break;
                            case PostOperationType::kGreaterThanOrEqual:
                                a_value = left > right || equal ? 1.0f : 0.0f;
                                break;
                            case PostOperationType::kEqual:
                                a_value = equal ? 1.0f : 0.0f;
                                break;
                            case PostOperationType::kNotEqual:
                                a_value = equal ? 0.0f : 1.0f;
                                break;
                            default:
                                return Fail("invalid comparison post operation");
                        }
                        break;
                    }
                    case PostOperationType::kAbs:
                    case PostOperationType::kFloor:
                    case PostOperationType::kCeil:
                    case PostOperationType::kLog:
                    case PostOperationType::kExp:
                    case PostOperationType::kSin:
                    case PostOperationType::kCos:
                    case PostOperationType::kTan:
                    case PostOperationType::kAsin:
                    case PostOperationType::kAcos:
                    case PostOperationType::kAtan:
                        return Fail("invalid unary post operation");
                }
                return std::isfinite(a_value) || Fail("post operation produced a non-finite value");
            }

            bool Prepare(const Definition& a_definition, const float a_value, std::vector<PreparedOutput>& a_outputs) {
                if (!std::isfinite(a_value) || !a_definition.output_type) {
                    return Fail(!std::isfinite(a_value) ? "output value is not finite" : "output type is unavailable",
                                a_definition.name);
                }
                PreparedOutput output{.name = a_definition.name, .type = *a_definition.output_type, .value = false};
                switch (*a_definition.output_type) {
                    case GraphType::kBool:
                        output.value = a_value != 0.0f;
                        break;
                    case GraphType::kInt: {
                        constexpr auto minimum = static_cast<float>(std::numeric_limits<std::int32_t>::min());
                        constexpr auto maximumExclusive = -minimum;
                        if (a_value < minimum || a_value >= maximumExclusive) {
                            return Fail("integer output is outside [-2147483648, 2147483648)", a_definition.name);
                        }
                        output.value = static_cast<std::int32_t>(a_value);
                        break;
                    }
                    case GraphType::kFloat:
                        output.value = a_value;
                        break;
                }
                a_outputs.push_back(std::move(output));
                return true;
            }

            const CompiledGroup& group;
            RE::TESObjectREFR* subject;
            RE::TESObjectREFR* target;
            RE::IAnimationGraphManagerHolder* holder;
            std::string_view currentDefinition;
            std::optional<EvaluationFailure> failure;
        };
    }

    void LogFailure(const CompiledGroup& a_group, const std::string_view a_definition,
                    const std::string_view a_reason) noexcept {
        try {
            logger::error("Variable group '{}' failed at '{}': {}", a_group.context,
                          a_definition.empty() ? "<group>" : a_definition, a_reason);
        } catch (...) {
        }
    }

    bool EvaluateAndWrite(const CompiledGroup& a_group, RE::TESObjectREFR* a_subject,
                          RE::TESObjectREFR* a_target, const std::optional<std::size_t> a_durationIndex,
                          unsigned int& a_duration) noexcept {
        try {
            Evaluation evaluation(a_group, a_subject, a_target);
            if (evaluation.Run(a_durationIndex, a_duration)) return true;
            const auto& failure = evaluation.Failure();
            LogFailure(a_group, failure ? failure->definition : std::string_view{},
                       failure ? failure->reason : "unknown evaluation failure");
        } catch (const std::exception& exception) {
            LogFailure(a_group, {}, exception.what());
        } catch (...) {
            LogFailure(a_group, {}, "unknown evaluation exception");
        }
        return false;
    }
}
