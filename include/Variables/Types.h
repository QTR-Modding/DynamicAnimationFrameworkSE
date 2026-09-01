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

    using ValueSource =
    std::variant<float, std::size_t, GraphRead, RE::TESGlobal*, std::shared_ptr<const Providers::ProviderCall>>;
    using Operand = std::variant<float, std::size_t>;
    using Condition = std::variant<std::size_t, RE::BGSPerk*>;

    enum class PostOperationType : std::uint8_t {
        kAdd,
        kSubtract,
        kMultiply,
        kDivide,
        kPow,
        kClamp,
        kAbs,
        kFloor,
        kCeil,
        kRound,
        kMin,
        kMax,
        kLog,
        kExp,
        kSin,
        kCos,
        kTan,
        kAsin,
        kAcos,
        kAtan,
        kAtan2,
        kLessThan,
        kLessThanOrEqual,
        kGreaterThan,
        kGreaterThanOrEqual,
        kEqual,
        kNotEqual,
    };

    struct PostOp {
        PostOperationType type;
        Operand first{0.0f};
        std::optional<Operand> second;
    };

    struct Definition {
        std::string name;
        ValueSource value;
        std::optional<ValueSource> else_val;
        std::vector<Condition> conditions;
        std::vector<PostOp> post;
        std::optional<GraphType> output_type;
    };

    struct CompiledGroup {
        std::string context;
        std::vector<Definition> definitions;
        std::vector<std::size_t> graph_variable_indices;
    };

    using CompiledGroupPtr = std::shared_ptr<const CompiledGroup>;
}
