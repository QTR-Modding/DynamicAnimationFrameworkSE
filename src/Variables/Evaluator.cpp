#include "Variables/Evaluator.h"

#include "Variables/Providers.h"
#include "logger.h"

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
                  holder(a_subject ? static_cast<RE::IAnimationGraphManagerHolder*>(a_subject) : nullptr) {}

            bool Run() {
                if (!subject || !holder) return Fail("animation graph subject is unavailable");
                std::vector<PreparedOutput> prepared;
                prepared.reserve(group.roots.size());
                for (const auto index : group.roots) {
                    if (index >= group.definitions.size()) {
                        return Fail("root definition index is out of range");
                    }
                    const auto& definition = group.definitions[index];
                    if (!definition.output_type) {
                        return Fail("root definition has no output type", definition.name);
                    }
                    float value;
                    if (!Calculate(index, value) || !Prepare(definition, value, prepared)) {
                        return false;
                    }
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

            const std::optional<EvaluationFailure>& Failure() const noexcept { return failure; }

        private:
            bool Fail(std::string a_reason, std::string_view a_definition = {}) {
                if (!failure)
                    failure = EvaluationFailure{std::string(a_definition.empty() ? currentDefinition : a_definition),
                                                std::move(a_reason)};
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
                } restore{currentDefinition, previousDefinition};
                bool gatePassed = definition.conditions.empty();
                if (!gatePassed) {
                    for (const auto* perk : definition.conditions) {
                        if (perk->perkConditions.IsTrue(subject, target)) {
                            gatePassed = true;
                            break;
                        }
                    }
                }
                if (!gatePassed) {
                    if (!definition.fallback) {
                        a_result = 0.0f;
                        return true;
                    }
                    if (!EvaluateSource(*definition.fallback, a_result)) return false;
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

            bool EvaluateSource(const Source& a_source, float& a_result) {
                return std::visit(
                    [this, &a_result](const auto& a_value) -> bool {
                        using T = std::decay_t<decltype(a_value)>;
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
                           [this, &a_result](const auto& a_value) -> bool {
                               using T = std::decay_t<decltype(a_value)>;
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

            bool Apply(const PostOperation& a_operation, float& a_value) {
                if (a_operation.type == PostOperationType::kAsin) {
                    if (a_value < -1.0f || a_value > 1.0f) return Fail("asin input is outside [-1, 1]");
                    a_value = std::asin(a_value);
                    return std::isfinite(a_value) || Fail("asin produced a non-finite value");
                }
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
                    case PostOperationType::kAsin:
                        return Fail("invalid asin post operation");
                }
                return std::isfinite(a_value) || Fail("post operation produced a non-finite value");
            }

            bool Prepare(const Definition& a_definition, const float a_value, std::vector<PreparedOutput>& a_outputs) {
                if (!std::isfinite(a_value) || !a_definition.output_type) {
                    return Fail(!std::isfinite(a_value) ? "output value is not finite" : "output type is unavailable",
                                a_definition.name);
                }
                PreparedOutput output{a_definition.name, *a_definition.output_type, false};
                switch (*a_definition.output_type) {
                    case GraphType::kBool:
                        output.value = a_value != 0.0f;
                        break;
                    case GraphType::kInt: {
                        constexpr float minimum = -2147483648.0f;
                        constexpr float maximumExclusive = 2147483648.0f;
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
                          RE::TESObjectREFR* a_target) noexcept {
        try {
            Evaluation evaluation(a_group, a_subject, a_target);
            if (evaluation.Run()) return true;
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
