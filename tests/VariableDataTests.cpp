#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "PCH.h"
#include "Variables/AnimationMapping.h"
#include "Variables/Compiler.h"
#include "Variables/Providers.h"

static_assert(std::is_same_v<decltype(Variables::GlobalRead::formID), std::uint32_t>);
static_assert(std::is_same_v<typename decltype(Variables::Definition::conditions)::value_type, std::uint32_t>);
static_assert(std::is_same_v<decltype(Variables::Providers::FormArgument::formID), std::uint32_t>);

namespace {
    struct Case {
        const char* name;
        const char* json;
        bool accepted;
        const char* errorFragment;
    };

    struct TemporaryDirectory {
        std::filesystem::path path;

        TemporaryDirectory() {
            const auto base = std::filesystem::temp_directory_path();
            std::mt19937_64 random(std::random_device{}());
            for (unsigned int attempt = 0; attempt < 100; ++attempt) {
                path = base / ("daf-variable-data-tests-" +
                               std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) +
                               '-' + std::to_string(random()));
                if (std::filesystem::create_directory(path)) return;
            }
            throw std::runtime_error("failed to create a unique temporary directory");
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    };

    bool Write(const std::filesystem::path& a_path, const char* a_text) {
        std::ofstream stream(a_path, std::ios::binary);
        stream << a_text;
        return stream.good();
    }

    rapidjson::Document Parse(const char* a_text) {
        rapidjson::Document document;
        document.Parse(a_text);
        return document;
    }

    void Check(const char* a_name, const bool a_passed, int& a_failures) {
        std::cout << (a_passed ? "PASS " : "FAIL ") << a_name << '\n';
        a_failures += a_passed ? 0 : 1;
    }

    int RunCompilerTests(const std::filesystem::path& a_root) {
        const std::vector<Case> cases{
            {"valid-literal-helper-post-graph",
             R"json({"helper":1.5,"scaled":{"value":"helper","post":{"multiply":-1,"add":1},"type":2},"live":{"value":["bUseLeftHand",0],"type":0}})json",
             true, ""},
            {"valid-complete-grammar",
             R"json({"base":2,"fallback":3,"allPost":{"value":"base","conditions":[],"else":"fallback","post":{"add":1,"subtract":2,"multiply":3,"divide":4,"pow":2,"clamp":[0,10],"asin":true},"type":2},"liveInt":{"value":["iLive",1],"type":1},"liveFloat":{"value":["fLive",2],"type":2}})json",
             true, ""},
            {"valid-helpers-only", R"json({"enabled":true,"copy":{"value":"enabled","post":{"multiply":-1}}})json",
             true, ""},
            {"duplicate-definition", R"json({"x":1,"x":2})json", false, "duplicate definition name"},
            {"empty-definition-name", R"json({"":1})json", false, "definition name cannot be empty"},
            {"non-object-root", R"json([])json", false, "root must be an object"},
            {"forward-reference", R"json({"x":{"value":"later"},"later":1})json", false, "earlier definition"},
            {"self-reference", R"json({"x":{"value":"x"}})json", false, "earlier definition"},
            {"else-forward-reference", R"json({"x":{"value":1,"else":"later"},"later":2})json", false,
             "earlier definition"},
            {"post-forward-reference", R"json({"x":{"value":1,"post":{"add":"later"}},"later":2})json", false,
             "earlier definition"},
            {"removed-set", R"json({"x":{"value":1,"set":true}})json", false, "field 'set' was removed"},
            {"duplicate-definition-field", R"json({"x":{"value":1,"value":2}})json", false,
             "duplicate definition field"},
            {"unknown-definition-field", R"json({"x":{"value":1,"extra":2}})json", false, "unknown definition field"},
            {"unknown-type", R"json({"x":{"value":1,"type":3}})json", false, "unknown graph type"},
            {"noninteger-type", R"json({"x":{"value":1,"type":1.5}})json", false, "expected integer type"},
            {"bad-graph-read", R"json({"x":{"value":["live",4],"type":2}})json", false, "unknown graph type"},
            {"empty-graph-name", R"json({"x":{"value":["",0],"type":0}})json", false,
             "graph read must be [nonemptyName, readType]"},
            {"bad-graph-shape", R"json({"x":{"value":["live",0,1],"type":0}})json", false,
             "graph read must be [nonemptyName, readType]"},
            {"empty-provider", R"json({"x":{"value":[],"type":2}})json", false, "source must be"},
            {"negative-provider-id", R"json({"x":{"value":[-1],"type":2}})json", false,
             "provider ID must be a non-negative 32-bit integer"},
            {"oversized-provider-id", R"json({"x":{"value":[4294967296],"type":2}})json", false,
             "provider ID must be a non-negative 32-bit integer"},
            {"invalid-provider-argument", R"json({"x":{"value":[1,true],"type":2}})json", false,
             "provider arguments must be finite numbers or FormID strings"},
            {"conditions-not-array", R"json({"x":{"value":1,"conditions":{},"type":2}})json", false,
             "conditions must be an array"},
            {"invalid-condition-entry", R"json({"x":{"value":1,"conditions":[1],"type":2}})json", false,
             "each condition must be a plugin-qualified BGSPerk FormID"},
            {"post-not-object", R"json({"x":{"value":1,"post":[],"type":2}})json", false, "post must be an object"},
            {"unknown-post", R"json({"x":{"value":1,"post":{"rotate":2},"type":2}})json", false,
             "unknown post operation"},
            {"duplicate-post", R"json({"x":{"value":1,"post":{"add":1,"add":2},"type":2}})json", false,
             "duplicate post operation"},
            {"invalid-asin", R"json({"x":{"value":1,"post":{"asin":false},"type":2}})json", false, "asin must be true"},
            {"invalid-clamp", R"json({"x":{"value":1,"post":{"clamp":[0]},"type":2}})json", false,
             "clamp must be [minimum, maximum]"},
            {"invalid-post-operand", R"json({"x":{"value":1,"post":{"add":true},"type":2}})json", false,
             "operand must be a finite number or earlier variable name"},
            {"missing-value", R"json({"x":{"type":2}})json", false, "requires 'value'"},
            {"malformed-json", "{\"x\":", false, "JSON parse error"},
        };

        const auto directory = a_root / "compiler";
        std::filesystem::create_directory(directory);
        int failures = 0;
        for (const auto& test : cases) {
            const auto path = directory / (std::string(test.name) + ".json");
            if (!Write(path, test.json)) {
                std::cerr << "WRITE FAIL " << test.name << '\n';
                ++failures;
                continue;
            }
            std::string error;
            const auto group = Variables::CompileFile(path, error);
            const bool accepted = static_cast<bool>(group);
            const bool errorMatches = test.accepted || error.find(test.errorFragment) != std::string::npos;
            if (accepted != test.accepted || !errorMatches) {
                std::cerr << "FAIL " << test.name << " accepted=" << accepted << " error=" << error << '\n';
                ++failures;
                continue;
            }
            std::cout << "PASS " << test.name;
            if (group && std::string_view(test.name) == "valid-literal-helper-post-graph") {
                const bool shape = group->definitions.size() == 3 && group->roots.size() == 2 &&
                                   group->definitions[1].post.size() == 2 &&
                                   group->definitions[1].post[0].type == Variables::PostOperationType::kMultiply &&
                                   group->definitions[1].post[1].type == Variables::PostOperationType::kAdd &&
                                   std::holds_alternative<Variables::GraphRead>(group->definitions[2].value);
                if (!shape) {
                    std::cout << " SHAPE-FAIL";
                    ++failures;
                }
            }
            if (group && std::string_view(test.name) == "valid-complete-grammar") {
                const auto& allPost = group->definitions[2];
                const bool shape = group->definitions.size() == 5 && group->roots.size() == 3 &&
                                   allPost.fallback.has_value() && allPost.conditions.empty() &&
                                   allPost.post.size() == 7 &&
                                   allPost.post[0].type == Variables::PostOperationType::kAdd &&
                                   allPost.post[1].type == Variables::PostOperationType::kSubtract &&
                                   allPost.post[2].type == Variables::PostOperationType::kMultiply &&
                                   allPost.post[3].type == Variables::PostOperationType::kDivide &&
                                   allPost.post[4].type == Variables::PostOperationType::kPow &&
                                   allPost.post[5].type == Variables::PostOperationType::kClamp &&
                                   allPost.post[6].type == Variables::PostOperationType::kAsin &&
                                   std::holds_alternative<Variables::GraphRead>(group->definitions[3].value) &&
                                   std::holds_alternative<Variables::GraphRead>(group->definitions[4].value);
                if (!shape) {
                    std::cout << " SHAPE-FAIL";
                    ++failures;
                }
            }
            if (group && std::string_view(test.name) == "valid-helpers-only" && !group->roots.empty()) {
                std::cout << " SHAPE-FAIL";
                ++failures;
            }
            std::cout << '\n';
        }

        std::string error;
        const bool safeName = Variables::IsSafeGroupName("takeLow", error);
        const bool unsafePath = !Variables::IsSafeGroupName("../takeLow", error);
        const auto resolved =
            Variables::ResolveGroupPath(R"(Data\SKSE\Plugins\DAF\animData\MyMod\interactions.json)", "takeLow", error);
        const auto rejected =
            Variables::ResolveGroupPath(R"(Data\SKSE\Plugins\DAF\other\MyMod\interactions.json)", "takeLow", error);
        const auto expected = std::filesystem::path(R"(Data\SKSE\Plugins\DAF\varData\MyMod\takeLow.json)");
        Check("group-path-validation", safeName && unsafePath && resolved == expected && rejected.empty(), failures);
        return failures;
    }

    int RunMappingTests(const std::filesystem::path& a_root) {
        const auto daf = a_root / "mapping" / "DAF";
        const auto animationFolder = daf / "animData" / "Example";
        const auto variableFolder = daf / "varData" / "Example";
        std::filesystem::create_directories(animationFolder);
        std::filesystem::create_directories(variableFolder);
        const auto animationFile = animationFolder / "animations.json";
        if (!Write(variableFolder / "valid.json", R"json({"helper":1,"output":{"value":"helper","type":2}})json") ||
            !Write(variableFolder / "invalid.json", R"json({"output":{"type":2}})json")) {
            throw std::runtime_error("failed to write mapping fixtures");
        }

        Variables::AnimationMappingCompiler compiler;
        std::vector<Variables::CompiledGroupPtr> groups;
        std::string error;
        int failures = 0;

        auto document = Parse(R"json({"animations":["a","b"]})json");
        Check("mapping-omitted",
              compiler.Compile(document, animationFile, 2, groups, error) && groups.size() == 2 && !groups[0] &&
                  !groups[1],
              failures);
        document = Parse(R"json({"variables":[null,"valid"]})json");
        Check("mapping-null-index-preservation",
              compiler.Compile(document, animationFile, 2, groups, error) && groups.size() == 2 && !groups[0] &&
                  groups[1],
              failures);
        const auto cached = groups[1];
        document = Parse(R"json({"variables":["valid"]})json");
        Check("mapping-exact-length", !compiler.Compile(document, animationFile, 2, groups, error), failures);
        document = Parse(R"json({"variables":["valid","valid","valid"]})json");
        Check("mapping-overlong", !compiler.Compile(document, animationFile, 2, groups, error), failures);
        document = Parse(R"json({"variables":{}})json");
        Check("mapping-non-array", !compiler.Compile(document, animationFile, 1, groups, error), failures);
        document = Parse(R"json({"variables":[null],"variables":[null]})json");
        Check("mapping-duplicate-field", !compiler.Compile(document, animationFile, 1, groups, error), failures);
        document = Parse(R"json({"variables":[1]})json");
        Check("mapping-invalid-entry", !compiler.Compile(document, animationFile, 1, groups, error), failures);
        document = Parse(R"json({"variables":["../valid"]})json");
        Check("mapping-invalid-name", !compiler.Compile(document, animationFile, 1, groups, error), failures);
        document = Parse(R"json({"variables":["missing"]})json");
        Check("mapping-missing-group", !compiler.Compile(document, animationFile, 1, groups, error), failures);
        document = Parse(R"json({"variables":["invalid"]})json");
        Check("mapping-invalid-group", !compiler.Compile(document, animationFile, 1, groups, error), failures);
        document = Parse(R"json({"variables":["valid"]})json");
        Check("mapping-wrong-animation-namespace",
              !compiler.Compile(document, daf / "other" / "Example" / "animations.json", 1, groups, error), failures);
        document = Parse(R"json({"variables":[null]})json");
        Check("mapping-success-clears-failure-state",
              compiler.Compile(document, animationFile, 1, groups, error) && error.empty() && groups.size() == 1 &&
                  !groups[0],
              failures);
        document = Parse(R"json({"variables":["valid","valid"]})json");
        Check("mapping-valid-cache-reuse",
              compiler.Compile(document, animationFile, 2, groups, error) && groups.size() == 2 &&
                  groups[0] == cached && groups[1] == cached,
              failures);
        Variables::AnimationMappingCompiler independentCompiler;
        document = Parse(R"json({"variables":["valid"]})json");
        Check("mapping-cache-instance-isolation",
              independentCompiler.Compile(document, animationFile, 1, groups, error) && groups.size() == 1 &&
                  groups[0] && groups[0] != cached,
              failures);
        return failures;
    }
}

int main() {
    const TemporaryDirectory temporaryDirectory;
    const int failures = RunCompilerTests(temporaryDirectory.path) + RunMappingTests(temporaryDirectory.path);
    std::cout << "TOTAL failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
