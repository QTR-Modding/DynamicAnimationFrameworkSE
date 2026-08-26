#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include "PCH.h"
#include "Variables/Compiler.h"

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
                path = base / ("daf-variable-compiler-tests-" +
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
}

int main() {
    const std::vector<Case> cases{
        {"valid-literal-helper-post-graph",
         R"json({"helper":1.5,"scaled":{"value":"helper","post":{"multiply":-1,"add":1},"type":2},"live":{"value":["bUseLeftHand",0],"type":0}})json",
         true, ""},
        {"duplicate-definition", R"json({"x":1,"x":2})json", false, "duplicate definition name"},
        {"forward-reference", R"json({"x":{"value":"later"},"later":1})json", false, "earlier definition"},
        {"removed-set", R"json({"x":{"value":1,"set":true}})json", false, "field 'set' was removed"},
        {"unknown-type", R"json({"x":{"value":1,"type":3}})json", false, "unknown graph type"},
        {"bad-graph-read", R"json({"x":{"value":["live",4],"type":2}})json", false, "unknown graph type"},
        {"unknown-post", R"json({"x":{"value":1,"post":{"rotate":2},"type":2}})json", false, "unknown post operation"},
        {"missing-value", R"json({"x":{"type":2}})json", false, "requires 'value'"},
        {"malformed-json", "{\"x\":", false, "JSON parse error"},
    };

    TemporaryDirectory temporaryDirectory;
    int failures = 0;
    for (const auto& test : cases) {
        const auto path = temporaryDirectory.path / (std::string(test.name) + ".json");
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
    const bool pathsPass = safeName && unsafePath && resolved == expected && rejected.empty();
    std::cout << (pathsPass ? "PASS" : "FAIL") << " group-path-validation\n";
    failures += pathsPass ? 0 : 1;

    std::cout << "TOTAL failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
