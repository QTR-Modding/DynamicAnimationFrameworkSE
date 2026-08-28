#pragma once

#include "Variables/Providers.h"

namespace Variables {
    enum class GraphType : std::uint8_t {
        kBool = 0,
        kInt = 1,
        kFloat = 2,
    };

    struct GraphRead {
        std::string name;
        GraphType type;
    };

    using Source =
        std::variant<float, std::size_t, GraphRead, RE::TESGlobal*, std::shared_ptr<const Providers::ProviderCall>>;
    using Operand = std::variant<float, std::size_t>;

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
        std::vector<RE::BGSPerk*> conditions;
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
