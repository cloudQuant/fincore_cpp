#pragma once

#include <fincore/runtime/runtime.hpp>

#include <vector>

namespace fincore {

/// Native-compatible runtime operations for the portfolio, risk and
/// simulation source domains (Iteration 8 / P105).
///
/// The returned declarations preserve the authoritative operation IDs from
/// the reconciliation catalog. Every callable invokes an existing native
/// kernel; structured results that have no dedicated runtime_value alternative
/// are serialized into a dataframe with the schema documented in the
/// operation's `provenance["input_schema"]` entry.
[[nodiscard]] std::vector<operation_spec>
portfolio_risk_simulation_operations();

}  // namespace fincore
