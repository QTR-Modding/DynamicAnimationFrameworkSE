#include "Variables/Compiler.h"

#include <rapidjson/error/en.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "CLibUtilsQTR/FormReader.hpp"
#include "Variables/Providers.h"

namespace Variables {
    namespace {
        using DefinitionMap = std::unordered_map<std::string, std::size_t>;

        std::string Prefix(std::string_view a_context, std::string_view a_definition) {
            std::string result(a_context);
            if (!a_definition.empty()) {
                result += " [";
                result += a_definition;
                result += ']';
            }
            return result;
        }

        bool Fail(std::string& a_error, std::string_view a_context, std::string_view a_message) {
            a_error = std::string(a_context) + ": " + std::string(a_message);
            return false;
        }

        bool ReadFiniteFloat(const rapidjson::Value& a_value, float& a_result, std::string& a_error,
                             std::string_view a_context) {
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

        bool IsPluginQualifiedForm(std::string_view a_value) {
            if (!a_value.starts_with("0x") && !a_value.starts_with("0X")) {
                return false;
            }
            const auto separator = a_value.find('~');
            if (separator <= 2 || separator + 1 >= a_value.size() || a_value.find('~', separator + 1) != a_value.npos) {
                return false;
            }
            for (std::size_t i = 2; i < separator; ++i) {
                const auto character = static_cast<unsigned char>(a_value[i]);
                if (!std::isxdigit(character)) {
                    return false;
                }
            }
            return true;
        }

        bool ResolvePluginFormID(std::string_view a_value, std::uint32_t& a_result, std::string& a_error,
                                 std::string_view a_context) {
            const auto separator = a_value.find('~');
            const std::string localID(a_value.substr(0, separator));
            const std::string pluginName(a_value.substr(separator + 1));
            a_result = FormReader::GetForm(pluginName.c_str(), FormReader::GetFormIDFromString(localID));
            return a_result != 0 || Fail(a_error, a_context, "Form identifier did not resolve to a loaded plugin");
        }

        bool ReadGraphType(const rapidjson::Value& a_value, GraphType& a_result, std::string& a_error,
                           std::string_view a_context) {
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

        bool ResolveEarlier(std::string_view a_name, const DefinitionMap& a_definitions, VariableReference& a_result,
                            std::string& a_error, std::string_view a_context) {
            if (a_name.empty()) {
                return Fail(a_error, a_context, "variable reference cannot be empty");
            }
            const auto found = a_definitions.find(std::string(a_name));
            if (found == a_definitions.end()) {
                return Fail(a_error, a_context, "variable reference must name an earlier definition");
            }
            a_result.index = found->second;
            return true;
        }

        bool CompileOperand(const rapidjson::Value& a_value, const DefinitionMap& a_definitions, Operand& a_result,
                            std::string& a_error, std::string_view a_context) {
            if (a_value.IsNumber()) {
                float literal;
                if (!ReadFiniteFloat(a_value, literal, a_error, a_context)) {
                    return false;
                }
                a_result = literal;
                return true;
            }
            if (a_value.IsString()) {
                VariableReference reference;
                if (!ResolveEarlier(a_value.GetString(), a_definitions, reference, a_error, a_context)) {
                    return false;
                }
                a_result = reference;
                return true;
            }
            return Fail(a_error, a_context, "operand must be a finite number or earlier variable name");
        }

        bool CompileSource(const rapidjson::Value& a_value, const DefinitionMap& a_definitions, Source& a_result,
                           std::string& a_error, std::string_view a_context) {
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
                if (IsPluginQualifiedForm(value)) {
                    std::uint32_t formID;
                    if (!ResolvePluginFormID(value, formID, a_error, a_context)) {
                        return false;
                    }
                    if (!RE::TESForm::LookupByID<RE::TESGlobal>(formID)) {
                        return Fail(a_error, a_context, "FormID source did not resolve to TESGlobal");
                    }
                    a_result = GlobalRead{formID};
                    return true;
                }
                VariableReference reference;
                if (!ResolveEarlier(value, a_definitions, reference, a_error, a_context)) {
                    return false;
                }
                a_result = reference;
                return true;
            }
            if (!a_value.IsArray() || a_value.Empty()) {
                return Fail(a_error, a_context, "source must be a literal, string, graph read, or provider array");
            }

            if (a_value[0].IsString()) {
                if (a_value.Size() != 2 || std::string_view(a_value[0].GetString()).empty()) {
                    return Fail(a_error, a_context, "graph read must be [nonemptyName, readType]");
                }
                GraphType type;
                if (!ReadGraphType(a_value[1], type, a_error, std::string(a_context) + ".readType")) {
                    return false;
                }
                a_result = GraphRead{a_value[0].GetString(), type};
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
                } else if (argument.IsString() && IsPluginQualifiedForm(argument.GetString())) {
                    std::uint32_t formID;
                    if (!ResolvePluginFormID(argument.GetString(), formID, a_error,
                                             std::string(a_context) + '[' + std::to_string(i) + ']')) {
                        return false;
                    }
                    arguments.emplace_back(Providers::FormArgument{formID});
                } else {
                    return Fail(a_error, a_context, "provider arguments must be finite numbers or FormID strings");
                }
            }
            std::string providerError;
            auto call = Providers::CompileCall(a_value[0].GetUint(),
                                               std::span<const Providers::ProviderLiteral>(arguments), providerError);
            if (!call) {
                return Fail(a_error, a_context, "provider call rejected: " + providerError);
            }
            a_result = ProviderRead{std::move(call)};
            return true;
        }

        bool CompileConditions(const rapidjson::Value& a_value, std::vector<std::uint32_t>& a_result,
                               std::string& a_error, std::string_view a_context) {
            if (!a_value.IsArray()) {
                return Fail(a_error, a_context, "conditions must be an array");
            }
            for (const auto& entry : a_value.GetArray()) {
                if (!entry.IsString() || !IsPluginQualifiedForm(entry.GetString())) {
                    return Fail(a_error, a_context, "each condition must be a plugin-qualified BGSPerk FormID");
                }
                std::uint32_t formID;
                if (!ResolvePluginFormID(entry.GetString(), formID, a_error, a_context)) {
                    return false;
                }
                if (!RE::TESForm::LookupByID<RE::BGSPerk>(formID)) {
                    return Fail(a_error, a_context, "condition FormID did not resolve to BGSPerk");
                }
                a_result.push_back(formID);
            }
            return true;
        }

        bool CompilePost(const rapidjson::Value& a_value, const DefinitionMap& a_definitions,
                         std::vector<PostOperation>& a_result, std::string& a_error, std::string_view a_context) {
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

                PostOperation operation{};
                if (name == "asin") {
                    if (!it->value.IsBool() || !it->value.GetBool()) {
                        return Fail(a_error, operationContext, "asin must be true");
                    }
                    operation.type = PostOperationType::kAsin;
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

        bool CompileDefinition(std::string_view a_name, const rapidjson::Value& a_value,
                               const DefinitionMap& a_definitions, Definition& a_result, std::string& a_error,
                               std::string_view a_context) {
            a_result.name = a_name;
            const auto definitionContext = Prefix(a_context, a_name);
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
                Source compiledFallback;
                if (!CompileSource(*fallback, a_definitions, compiledFallback, a_error, definitionContext + ".else")) {
                    return false;
                }
                a_result.fallback = std::move(compiledFallback);
            }
            if (conditions &&
                !CompileConditions(*conditions, a_result.conditions, a_error, definitionContext + ".conditions")) {
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
            return true;
        }
    }

    CompiledGroupPtr CompileGroup(const rapidjson::Value& a_root, const std::string_view a_context,
                                  std::string& a_error) noexcept {
        try {
            a_error.clear();
            if (!a_root.IsObject()) {
                Fail(a_error, a_context, "variable-group root must be an object");
                return nullptr;
            }
            auto group = std::make_shared<CompiledGroup>();
            group->context = a_context;
            group->definitions.reserve(a_root.MemberCount());
            DefinitionMap definitions;
            for (auto it = a_root.MemberBegin(); it != a_root.MemberEnd(); ++it) {
                const std::string name = it->name.GetString();
                if (name.empty()) {
                    Fail(a_error, a_context, "definition name cannot be empty");
                    return nullptr;
                }
                if (definitions.contains(name)) {
                    Fail(a_error, Prefix(a_context, name), "duplicate definition name");
                    return nullptr;
                }
                Definition definition;
                if (!CompileDefinition(name, it->value, definitions, definition, a_error, a_context)) {
                    return nullptr;
                }
                const auto index = group->definitions.size();
                if (definition.output_type) {
                    group->roots.push_back(index);
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
        const auto animationFolder = a_animationFile.parent_path();
        const auto animDataFolder = animationFolder.parent_path();
        const auto dafFolder = animDataFolder.parent_path();
        if (animationFolder.empty() || animDataFolder.filename() != "animData" || dafFolder.filename() != "DAF") {
            Fail(a_error, a_animationFile.string(), "animation file is not under DAF/animData/<folder>");
            return {};
        }
        return dafFolder / "varData" / animationFolder.filename() / (std::string(a_groupName) + ".json");
    }
}
