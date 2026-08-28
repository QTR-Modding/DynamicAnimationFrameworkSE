#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Variables {
    namespace Providers {
        struct ProviderCall;
    }

    enum class GraphType : std::uint8_t {
        kBool = 0,
        kInt = 1,
        kFloat = 2,
    };

    struct VariableReference {
        std::size_t index;
    };

    struct GraphRead {
        std::string name;
        GraphType type;
    };

    struct GlobalRead {
        std::uint32_t formID;
    };

    struct ProviderRead {
        std::shared_ptr<const Providers::ProviderCall> call;
    };

    using Source = std::variant<float, VariableReference, GraphRead, GlobalRead, ProviderRead>;
    using Operand = std::variant<float, VariableReference>;

    enum class PostOperationType : std::uint8_t {
        kAdd,
        kSubtract,
        kMultiply,
        kDivide,
        kPow,
        kClamp,
        kAsin,
    };

    struct PostOperation {
        PostOperationType type;
        Operand first{0.0f};
        std::optional<Operand> second;
    };

    struct Definition {
        std::string name;
        Source value;
        std::optional<Source> fallback;
        std::vector<std::uint32_t> conditions;
        std::vector<PostOperation> post;
        std::optional<GraphType> output_type;
    };

    struct CompiledGroup {
        std::string context;
        std::vector<Definition> definitions;
        std::vector<std::size_t> roots;
    };

    using CompiledGroupPtr = std::shared_ptr<const CompiledGroup>;
}
