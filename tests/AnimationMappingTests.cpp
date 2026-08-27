#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "PCH.h"
#include "Variables/AnimationMapping.h"

namespace {
    struct TemporaryDirectory {
        std::filesystem::path path;
        TemporaryDirectory() {
            const auto base = std::filesystem::temp_directory_path();
            std::mt19937_64 random(std::random_device{}());
            for (unsigned int attempt = 0; attempt < 100; ++attempt) {
                path = base / ("daf-animation-mapping-tests-" +
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
}

int main() {
    TemporaryDirectory temporaryDirectory;
    const auto daf = temporaryDirectory.path / "DAF";
    const auto animationFolder = daf / "animData" / "Example";
    const auto variableFolder = daf / "varData" / "Example";
    std::filesystem::create_directories(animationFolder);
    std::filesystem::create_directories(variableFolder);
    const auto animationFile = animationFolder / "animations.json";
    Write(variableFolder / "valid.json", R"json({"helper":1,"output":{"value":"helper","type":2}})json");
    Write(variableFolder / "invalid.json", R"json({"output":{"type":2}})json");

    Variables::AnimationMappingCompiler compiler;
    std::vector<Variables::CompiledGroupPtr> groups;
    std::string error;
    int failures = 0;
    const auto check = [&](const char* a_name, const bool a_passed) {
        std::cout << (a_passed ? "PASS " : "FAIL ") << a_name << '\n';
        failures += a_passed ? 0 : 1;
    };

    auto document = Parse(R"json({"animations":["a","b"]})json");
    check("omitted", compiler.Compile(document, animationFile, 2, groups, error) && groups.size() == 2 && !groups[0] &&
                         !groups[1]);
    document = Parse(R"json({"variables":[null,"valid"]})json");
    check("null-index-preservation",
          compiler.Compile(document, animationFile, 2, groups, error) && groups.size() == 2 && !groups[0] && groups[1]);
    const auto cached = groups[1];
    document = Parse(R"json({"variables":["valid"]})json");
    check("exact-length", !compiler.Compile(document, animationFile, 2, groups, error));
    document = Parse(R"json({"variables":["valid","valid","valid"]})json");
    check("overlong", !compiler.Compile(document, animationFile, 2, groups, error));
    document = Parse(R"json({"variables":{}})json");
    check("non-array", !compiler.Compile(document, animationFile, 1, groups, error));
    document = Parse(R"json({"variables":[null],"variables":[null]})json");
    check("duplicate-field", !compiler.Compile(document, animationFile, 1, groups, error));
    document = Parse(R"json({"variables":[1]})json");
    check("invalid-entry", !compiler.Compile(document, animationFile, 1, groups, error));
    document = Parse(R"json({"variables":["../valid"]})json");
    check("invalid-name", !compiler.Compile(document, animationFile, 1, groups, error));
    document = Parse(R"json({"variables":["missing"]})json");
    check("missing-group", !compiler.Compile(document, animationFile, 1, groups, error));
    document = Parse(R"json({"variables":["invalid"]})json");
    check("invalid-group", !compiler.Compile(document, animationFile, 1, groups, error));
    document = Parse(R"json({"variables":["valid"]})json");
    check("wrong-animation-namespace",
          !compiler.Compile(document, daf / "other" / "Example" / "animations.json", 1, groups, error));
    document = Parse(R"json({"variables":[null]})json");
    check("success-clears-failure-state", compiler.Compile(document, animationFile, 1, groups, error) &&
                                              error.empty() && groups.size() == 1 && !groups[0]);
    document = Parse(R"json({"variables":["valid","valid"]})json");
    check("valid-cache-reuse", compiler.Compile(document, animationFile, 2, groups, error) && groups.size() == 2 &&
                                   groups[0] == cached && groups[1] == cached);
    Variables::AnimationMappingCompiler independentCompiler;
    document = Parse(R"json({"variables":["valid"]})json");
    check("cache-instance-isolation", independentCompiler.Compile(document, animationFile, 1, groups, error) &&
                                          groups.size() == 1 && groups[0] && groups[0] != cached);

    std::cout << "TOTAL failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
