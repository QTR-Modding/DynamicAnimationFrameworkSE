#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <variant>

#include "RE/Skyrim.h"

namespace Variables::Providers {
    using ProviderLiteral = std::variant<double, std::string>;

    struct ProviderCall;

    std::shared_ptr<const ProviderCall> CompileCall(std::uint32_t a_providerID,
                                                    std::span<const ProviderLiteral> a_arguments, std::string& a_error);

    bool Evaluate(const ProviderCall& a_call, RE::TESObjectREFR* a_subject, RE::TESObjectREFR* a_target,
                  float& a_result, std::string& a_error) noexcept;
}
