#include "Variables/Compiler.h"
#include <rapidjson/error/en.h>
#include <unordered_set>
#include "CLibUtilsQTR/FormReader.hpp"


namespace Variables {
    namespace {
        using DefinitionMap = std::unordered_map<std::string, std::size_t>;

        std::string MakeDefinitionContext(const std::string_view a_context, const std::string_view a_definition) {
            std::string result(a_context);
            if (!a_definition.empty()) {
                result += " [";
                result += a_definition;
                result += ']';
            }
            return result;
        }

        bool Fail(std::string& a_error, const std::string_view a_context, const std::string_view a_message) {
            a_error = std::string(a_context) + ": " + std::string(a_message);
            return false;
        }

        bool ReadFiniteFloat(const rapidjson::Value& a_value, float& a_result, std::string& a_error,
                             const std::string_view a_context) {
            if (!a_value.IsNumber()) {
                return Fail(a_error, a_context, "expected a number");
            }
            const auto value = a_value.GetDouble();
            a_result = static_cast<float>(value);
            if (!std::isfinite(value) || !std::isfinite(a_result)) {
                return Fail(a_error, a_context, "number is not representable as a finite float");
            }
            return true;
        }

        bool ReadGraphType(const rapidjson::Value& a_value, GraphType& a_result, std::string& a_error,
                           const std::string_view a_context) {
            if (!a_value.IsInt()) {
                return Fail(a_error, a_context, "expected integer type 0, 1, or 2");
            }
            switch (a_value.GetInt()) {
                case 0:
                    a_result = GraphType::kBool;
                    return true;
                case 1:
                    a_result = GraphType::kInt;
                    return true;
                case 2:
                    a_result = GraphType::kFloat;
                    return true;
                default:
                    return Fail(a_error, a_context, "unknown graph type; expected 0, 1, or 2");
            }
        }

        bool ResolveEarlier(const std::string_view a_name, const DefinitionMap& a_definitions, std::size_t& a_result,
                            std::string& a_error, const std::string_view a_context) {
            if (a_name.empty()) {
                return Fail(a_error, a_context, "variable reference cannot be empty");
            }
            const auto found = a_definitions.find(std::string(a_name));
            if (found == a_definitions.end()) {
                return Fail(a_error, a_context, "variable reference must name an earlier definition");
            }
            a_result = found->second;
            return true;
        }

        bool CompileOperand(const rapidjson::Value& a_value, const DefinitionMap& a_definitions, Operand& a_result,
                            std::string& a_error, const std::string_view a_context) {
            if (a_value.IsNumber()) {
                float literal;
                if (!ReadFiniteFloat(a_value, literal, a_error, a_context)) {
                    return false;
                }
                a_result = literal;
                return true;
            }
            if (a_value.IsString()) {
                std::size_t reference;
                if (!ResolveEarlier(a_value.GetString(), a_definitions, reference, a_error, a_context)) {
                    return false;
                }
                a_result = reference;
                return true;
            }
            return Fail(a_error, a_context, "operand must be a finite number or earlier variable name");
        }

        bool CompileSource(const rapidjson::Value& a_value, const DefinitionMap& a_definitions, ValueSource& a_result,
                           std::string& a_error, const std::string_view a_context) {
            if (a_value.IsBool()) {
                a_result = a_value.GetBool() ? 1.0f : 0.0f;
                return true;
            }
            if (a_value.IsNumber()) {
                float literal;
                if (!ReadFiniteFloat(a_value, literal, a_error, a_context)) {
                    return false;
                }
                a_result = literal;
                return true;
            }
            if (a_value.IsString()) {
                const std::string value = a_value.GetString();
                if (const auto definition = a_definitions.find(value); definition != a_definitions.end()) {
                    a_result = definition->second;
                    return true;
                }
                const auto form = FormReader::GetFormFromString(value);
                auto global = form ? form->As<RE::TESGlobal>() : nullptr;
                if (!global) {
                    return Fail(a_error, a_context,
                                "string source did not resolve to an earlier definition or TESGlobal");
                }
                a_result = global;
                return true;
            }
            if (!a_value.IsArray() || a_value.Empty()) {
                return Fail(
                    a_error, a_context,
                    "value must be a boolean, number, variable/global string, graph-read array, or provider array");
            }

            if (a_value[0].IsString()) {
                if (a_value.Size() != 2 || std::string_view(a_value[0].GetString()).empty()) {
                    return Fail(a_error, a_context, "graph read must be [nonemptyName, readType]");
                }
                GraphType type;
                if (!ReadGraphType(a_value[1], type, a_error, std::string(a_context) + ".readType")) {
                    return false;
                }
                a_result = GraphRead{.name = a_value[0].GetString(), .type = type};
                return true;
            }

            if (!a_value[0].IsUint()) {
                return Fail(a_error, a_context, "provider ID must be a non-negative 32-bit integer");
            }
            std::vector<Providers::ProviderLiteral> arguments;
            arguments.reserve(a_value.Size() - 1);
            for (rapidjson::SizeType i = 1; i < a_value.Size(); ++i) {
                const auto& argument = a_value[i];
                if (argument.IsNumber()) {
                    const auto number = argument.GetDouble();
                    if (!std::isfinite(number)) {
                        return Fail(a_error, a_context, "provider numeric argument must be finite");
                    }
                    arguments.emplace_back(number);
                } else if (argument.IsString()) {
                    auto form = FormReader::GetFormFromString(argument.GetString());
                    if (!form) {
                        return Fail(a_error, std::string(a_context) + '[' + std::to_string(i) + ']',
                                    "Form identifier did not resolve");
                    }
                    arguments.emplace_back(form);
                } else {
                    return Fail(a_error, a_context,
                                "provider arguments must be finite numbers or Form identifier strings");
                }
            }
            std::string providerError;
            auto call = Providers::CompileCall(a_value[0].GetUint(),
                                               std::span<const Providers::ProviderLiteral>(arguments), providerError);
            if (!call) {
                return Fail(a_error, a_context, "provider call rejected: " + providerError);
            }
            a_result = std::move(call);
            return true;
        }

        bool CompileConditions(const rapidjson::Value& a_value, const DefinitionMap& a_definitions,
                               std::vector<Condition>& a_result, std::string& a_error,
                               const std::string_view a_context) {
            if (!a_value.IsArray()) {
                return Fail(a_error, a_context, "conditions must be an array");
            }
            for (const auto& entry : a_value.GetArray()) {
                if (!entry.IsString()) {
                    return Fail(a_error, a_context,
                                "each condition must be an earlier variable name or BGSPerk Form identifier");
                }
                const std::string value = entry.GetString();
                if (const auto definition = a_definitions.find(value); definition != a_definitions.end()) {
                    a_result.emplace_back(definition->second);
                    continue;
                }
                const auto form = FormReader::GetFormFromString(value);
                auto perk = form ? form->As<RE::BGSPerk>() : nullptr;
                if (!perk) {
                    return Fail(a_error, a_context,
                                "condition did not resolve to an earlier variable or BGSPerk");
                }
                a_result.emplace_back(perk);
            }
            return true;
        }

        bool CompilePost(const rapidjson::Value& a_value, const DefinitionMap& a_definitions,
                         std::vector<PostOp>& a_result, std::string& a_error, const std::string_view a_context) {
            if (!a_value.IsObject()) {
                return Fail(a_error, a_context, "post must be an object");
            }
            std::unordered_set<std::string> seen;
            for (auto it = a_value.MemberBegin(); it != a_value.MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                const auto operationContext = std::string(a_context) + '.' + name;
                if (!seen.insert(name).second) {
                    return Fail(a_error, operationContext, "duplicate post operation");
                }

                PostOp operation{};
                if (name == "abs" || name == "floor" || name == "ceil" || name == "log" || name == "exp" ||
                    name == "sin" || name == "cos" || name == "tan" || name == "asin" || name == "acos" ||
                    name == "atan") {
                    if (!it->value.IsBool() || !it->value.GetBool()) {
                        return Fail(a_error, operationContext, name + " must be true");
                    }
                    if (name == "abs") {
                        operation.type = PostOperationType::kAbs;
                    } else if (name == "floor") {
                        operation.type = PostOperationType::kFloor;
                    } else if (name == "ceil") {
                        operation.type = PostOperationType::kCeil;
                    } else if (name == "log") {
                        operation.type = PostOperationType::kLog;
                    } else if (name == "exp") {
                        operation.type = PostOperationType::kExp;
                    } else if (name == "sin") {
                        operation.type = PostOperationType::kSin;
                    } else if (name == "cos") {
                        operation.type = PostOperationType::kCos;
                    } else if (name == "tan") {
                        operation.type = PostOperationType::kTan;
                    } else if (name == "asin") {
                        operation.type = PostOperationType::kAsin;
                    } else if (name == "acos") {
                        operation.type = PostOperationType::kAcos;
                    } else {
                        operation.type = PostOperationType::kAtan;
                    }
                } else if (name == "clamp") {
                    if (!it->value.IsArray() || it->value.Size() != 2) {
                        return Fail(a_error, operationContext, "clamp must be [minimum, maximum]");
                    }
                    operation.type = PostOperationType::kClamp;
                    if (!CompileOperand(it->value[0], a_definitions, operation.first, a_error,
                                        operationContext + "[0]")) {
                        return false;
                    }
                    Operand second;
                    if (!CompileOperand(it->value[1], a_definitions, second, a_error, operationContext + "[1]")) {
                        return false;
                    }
                    operation.second = std::move(second);
                } else {
                    if (name == "add") {
                        operation.type = PostOperationType::kAdd;
                    } else if (name == "subtract") {
                        operation.type = PostOperationType::kSubtract;
                    } else if (name == "multiply") {
                        operation.type = PostOperationType::kMultiply;
                    } else if (name == "divide") {
                        operation.type = PostOperationType::kDivide;
                    } else if (name == "pow") {
                        operation.type = PostOperationType::kPow;
                    } else if (name == "round") {
                        operation.type = PostOperationType::kRound;
                    } else if (name == "min") {
                        operation.type = PostOperationType::kMin;
                    } else if (name == "max") {
                        operation.type = PostOperationType::kMax;
                    } else if (name == "atan2") {
                        operation.type = PostOperationType::kAtan2;
                    } else if (name == "lt") {
                        operation.type = PostOperationType::kLessThan;
                    } else if (name == "le") {
                        operation.type = PostOperationType::kLessThanOrEqual;
                    } else if (name == "gt") {
                        operation.type = PostOperationType::kGreaterThan;
                    } else if (name == "ge") {
                        operation.type = PostOperationType::kGreaterThanOrEqual;
                    } else if (name == "eq") {
                        operation.type = PostOperationType::kEqual;
                    } else if (name == "ne") {
                        operation.type = PostOperationType::kNotEqual;
                    } else {
                        return Fail(a_error, operationContext, "unknown post operation");
                    }
                    if (!CompileOperand(it->value, a_definitions, operation.first, a_error, operationContext)) {
                        return false;
                    }
                }
                a_result.push_back(std::move(operation));
            }
            return true;
        }

        bool CompileInterpolation(const rapidjson::Value& a_value, const DefinitionMap& a_definitions,
                                  Interpolation& a_result, std::string& a_error,
                                  const std::string_view a_context) {
            if (!a_value.IsObject()) {
                return CompileSource(a_value, a_definitions, a_result.target, a_error, a_context);
            }

            const rapidjson::Value* target = nullptr;
            std::unordered_set<std::string> seen;
            for (auto it = a_value.MemberBegin(); it != a_value.MemberEnd(); ++it) {
                const std::string field = it->name.GetString();
                if (!seen.insert(field).second) {
                    return Fail(a_error, std::string(a_context) + '.' + field, "duplicate interpolation field");
                }
                if (field == "target") {
                    target = &it->value;
                } else if (field == "mode") {
                    if (!it->value.IsString()) {
                        return Fail(a_error, std::string(a_context) + ".mode", "mode must be 'linear' or 'step'");
                    }
                    const std::string_view mode = it->value.GetString();
                    if (mode == "linear") {
                        a_result.mode = InterpolationMode::kLinear;
                    } else if (mode == "step") {
                        a_result.mode = InterpolationMode::kStep;
                    } else {
                        return Fail(a_error, std::string(a_context) + ".mode", "unknown interpolation mode");
                    }
                } else {
                    return Fail(a_error, std::string(a_context) + '.' + field, "unknown interpolation field");
                }
            }
            if (!target) {
                return Fail(a_error, a_context, "interpolation object requires 'target'");
            }
            return CompileSource(*target, a_definitions, a_result.target, a_error,
                                 std::string(a_context) + ".target");
        }

        bool CompileDefinition(const std::string_view a_name, const rapidjson::Value& a_value,
                               const DefinitionMap& a_definitions, Definition& a_result, std::string& a_error,
                               const std::string_view a_context) {
            a_result.name = a_name;
            const auto definitionContext = MakeDefinitionContext(a_context, a_name);
            if (a_value.IsBool() || a_value.IsNumber()) {
                return CompileSource(a_value, a_definitions, a_result.value, a_error, definitionContext + ".value");
            }
            if (!a_value.IsObject()) {
                return Fail(a_error, definitionContext, "definition must be a literal or object");
            }

            const rapidjson::Value* value = nullptr;
            const rapidjson::Value* fallback = nullptr;
            const rapidjson::Value* conditions = nullptr;
            const rapidjson::Value* post = nullptr;
            const rapidjson::Value* type = nullptr;
            const rapidjson::Value* interpolate = nullptr;
            std::unordered_set<std::string> seen;
            for (auto it = a_value.MemberBegin(); it != a_value.MemberEnd(); ++it) {
                const std::string field = it->name.GetString();
                if (!seen.insert(field).second) {
                    return Fail(a_error, definitionContext + '.' + field, "duplicate definition field");
                }
                if (field == "value") {
                    value = &it->value;
                } else if (field == "else") {
                    fallback = &it->value;
                } else if (field == "conditions") {
                    conditions = &it->value;
                } else if (field == "post") {
                    post = &it->value;
                } else if (field == "type") {
                    type = &it->value;
                } else if (field == "interpolate") {
                    interpolate = &it->value;
                } else if (field == "set") {
                    return Fail(a_error, definitionContext, "field 'set' was removed; use 'type' to mark an output");
                } else {
                    return Fail(a_error, definitionContext + '.' + field, "unknown definition field");
                }
            }
            if (!value) {
                return Fail(a_error, definitionContext, "expanded definition requires 'value'");
            }
            if (!CompileSource(*value, a_definitions, a_result.value, a_error, definitionContext + ".value")) {
                return false;
            }
            if (fallback) {
                ValueSource compiledFallback;
                if (!CompileSource(*fallback, a_definitions, compiledFallback, a_error, definitionContext + ".else")) {
                    return false;
                }
                a_result.else_val = std::move(compiledFallback);
            }
            if (conditions &&
                !CompileConditions(*conditions, a_definitions, a_result.conditions, a_error,
                                   definitionContext + ".conditions")) {
                return false;
            }
            if (post && !CompilePost(*post, a_definitions, a_result.post, a_error, definitionContext + ".post")) {
                return false;
            }
            if (type) {
                GraphType graphType;
                if (!ReadGraphType(*type, graphType, a_error, definitionContext + ".type")) {
                    return false;
                }
                a_result.output_type = graphType;
            }
            if (interpolate) {
                if (!a_result.output_type) {
                    return Fail(a_error, definitionContext + ".interpolate",
                                "interpolation requires a graph output type");
                }
                Interpolation compiled;
                if (!CompileInterpolation(*interpolate, a_definitions, compiled, a_error,
                                          definitionContext + ".interpolate")) {
                    return false;
                }
                if (compiled.mode == InterpolationMode::kLinear &&
                    *a_result.output_type != GraphType::kFloat) {
                    return Fail(a_error, definitionContext + ".interpolate",
                                "linear interpolation requires graph type 2");
                }
                a_result.interpolation = std::move(compiled);
            }
            return true;
        }
    }

    CompiledGroupPtr CompileGroup(const rapidjson::Value& a_groupData, const std::string_view a_context,
                                  std::string& a_error) noexcept {
        try {
            a_error.clear();
            if (!a_groupData.IsObject()) {
                Fail(a_error, a_context, "variable-group root must be an object");
                return nullptr;
            }
            auto group = std::make_shared<CompiledGroup>();
            group->context = a_context;
            group->definitions.reserve(a_groupData.MemberCount());
            DefinitionMap definitions;
            for (auto it = a_groupData.MemberBegin(); it != a_groupData.MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                if (name.empty()) {
                    Fail(a_error, a_context, "definition name cannot be empty");
                    return nullptr;
                }
                if (definitions.contains(name)) {
                    Fail(a_error, MakeDefinitionContext(a_context, name), "duplicate definition name");
                    return nullptr;
                }
                Definition definition;
                if (!CompileDefinition(name, it->value, definitions, definition, a_error, a_context)) {
                    return nullptr;
                }
                const auto index = group->definitions.size();
                if (definition.output_type) {
                    group->graph_variable_indices.push_back(index);
                }
                group->definitions.push_back(std::move(definition));
                definitions.emplace(name, index);
            }
            return group;
        } catch (const std::exception& exception) {
            a_error = std::string(a_context) + ": exception while compiling: " + exception.what();
        } catch (...) {
            a_error = std::string(a_context) + ": unknown exception while compiling";
        }
        return nullptr;
    }

    CompiledGroupPtr CompileFile(const std::filesystem::path& a_path, std::string& a_error) noexcept {
        try {
            std::ifstream stream(a_path, std::ios::binary);
            if (!stream) {
                a_error = a_path.string() + ": failed to open variable-group file";
                return nullptr;
            }
            std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
            rapidjson::Document document;
            document.Parse(contents.c_str(), contents.size());
            if (document.HasParseError()) {
                std::ostringstream message;
                message << a_path.string() << ": JSON parse error at offset " << document.GetErrorOffset() << ": "
                    << rapidjson::GetParseError_En(document.GetParseError());
                a_error = message.str();
                return nullptr;
            }
            return CompileGroup(document, a_path.string(), a_error);
        } catch (const std::exception& exception) {
            a_error = a_path.string() + ": exception while reading: " + exception.what();
        } catch (...) {
            a_error = a_path.string() + ": unknown exception while reading";
        }
        return nullptr;
    }

    bool IsSafeGroupName(const std::string_view a_name, std::string& a_error) noexcept {
        a_error.clear();
        if (a_name.empty()) {
            return Fail(a_error, "variable group", "name cannot be empty");
        }
        if (a_name == "." || a_name == ".." || a_name.find('/') != a_name.npos || a_name.find('\\') != a_name.npos ||
            a_name.find(':') != a_name.npos) {
            return Fail(a_error, "variable group", "name must be a bare filename without path syntax");
        }
        std::string lower(a_name);
        std::ranges::transform(lower, lower.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        if (lower.ends_with(".json")) {
            return Fail(a_error, "variable group", "name must not include the .json extension");
        }
        return true;
    }

    std::filesystem::path ResolveGroupPath(const std::filesystem::path& a_animationFile,
                                           const std::string_view a_groupName, std::string& a_error) noexcept {
        if (!IsSafeGroupName(a_groupName, a_error)) {
            return {};
        }

        constexpr std::string_view varDataFolder = R"(Data\SKSE\Plugins\DAF\varData)";

        return std::filesystem::path(varDataFolder) / a_animationFile.parent_path().filename() /
               (std::string(a_groupName) + ".json");
    }
}
