#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace fincore {

/// Stable native operation descriptor.
///
/// The pinned Python domains declare an ``OperationSpec`` with an operation
/// id, the same capability id, a domain, determinism/RNG policy and
/// provenance. Native consumers need the same stable identity without Python
/// callables, adapters or placeholder implementations. This descriptor
/// therefore carries identity/metadata only; the owning native kernel remains
/// the implementation.
struct operation_descriptor {
    std::string_view operation_id;
    std::string_view capability_id;
    std::string_view domain;
    std::string_view source_module;
    std::string_view provenance_owner;
    bool deterministic{true};
    std::string_view rng_policy{"none"};

    constexpr operation_descriptor() = default;

    constexpr explicit operation_descriptor(
        const std::string_view id, const bool is_deterministic = true,
        const std::string_view policy = "none") noexcept
        : operation_id(id),
          capability_id(id),
          domain(domain_of(id)),
          source_module(module_of(id)),
          provenance_owner(domain_of(id)),
          deterministic(is_deterministic),
          rng_policy(policy) {}

  private:
    static constexpr std::string_view domain_of(const std::string_view id) {
        const auto dot = id.find('.');
        return dot == std::string_view::npos ? id : id.substr(0U, dot);
    }

    static constexpr std::string_view module_of(const std::string_view id) {
        const auto first = id.find('.');
        if (first == std::string_view::npos) {
            return {};
        }
        const auto second = id.find('.', first + 1U);
        return second == std::string_view::npos ? id
                                                : id.substr(0U, second);
    }
};

/// Domain-owned descriptor accessors. Each returns a stable, source-ordered
/// view of the domain's declared operations.
[[nodiscard]] std::span<const operation_descriptor> attribution_operations();
[[nodiscard]] std::span<const operation_descriptor> optimization_operations();
[[nodiscard]] std::span<const operation_descriptor> performance_operations();
[[nodiscard]] std::span<const operation_descriptor> portfolio_operations();
[[nodiscard]] std::span<const operation_descriptor> report_operations();
[[nodiscard]] std::span<const operation_descriptor> risk_operations();
[[nodiscard]] std::span<const operation_descriptor> simulation_operations();
[[nodiscard]] std::span<const operation_descriptor> data_operations();
[[nodiscard]] std::span<const operation_descriptor> factor_analysis_operations();
[[nodiscard]] std::span<const operation_descriptor> metrics_operations();

/// All declared operations in domain order. The returned view references
/// static storage and is stable for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> all_operations();

}  // namespace fincore
