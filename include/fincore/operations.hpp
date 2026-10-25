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
    std::string_view operation_id;  ///< Stable operation identifier.
    std::string_view capability_id;  ///< Capability identifier; defaults to
                                     ///< the operation id.
    std::string_view domain;  ///< Domain portion of the operation id.
    std::string_view source_module;  ///< Module portion of the operation id.
    std::string_view provenance_owner;  ///< Provenance owner; defaults to the
                                        ///< domain.
    bool deterministic{true};  ///< Whether the operation is deterministic.
    std::string_view rng_policy{"none"};  ///< RNG policy label.

    /// Creates an empty descriptor.
    ///
    /// All views are empty, `deterministic` is true and `rng_policy` is
    /// `"none"`.
    constexpr operation_descriptor() = default;

    /// Creates a descriptor from an operation id.
    ///
    /// `capability_id` is set to `id`. `domain` and `provenance_owner` are the
    /// text before the first dot, or the whole id when it has no dot;
    /// `source_module` is empty when `id` has no dot, the whole id when it has
    /// exactly one dot, and otherwise the text before the second dot.
    ///
    /// @param id Stable operation identifier.
    /// @param is_deterministic Whether the operation is deterministic.
    /// @param policy RNG policy label.
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

/// Returns the attribution operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> attribution_operations();

/// Returns the optimization operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> optimization_operations();

/// Returns the performance operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> performance_operations();

/// Returns the portfolio operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> portfolio_operations();

/// Returns the report operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> report_operations();

/// Returns the risk operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> risk_operations();

/// Returns the simulation operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> simulation_operations();

/// Returns the data operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> data_operations();

/// Returns the factor analysis operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> factor_analysis_operations();

/// Returns the metrics operations in source order.
///
/// @return View of static descriptor storage; valid for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> metrics_operations();

/// Returns all declared operations in domain order.
///
/// @return View of static storage ordered by attribution, optimization,
///     performance, portfolio, report, risk, simulation, data, factor analysis
///     and metrics. The view is stable for the process lifetime.
[[nodiscard]] std::span<const operation_descriptor> all_operations();

}  // namespace fincore
