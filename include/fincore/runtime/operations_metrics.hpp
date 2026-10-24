#pragma once

#include <fincore/runtime/runtime.hpp>

#include <vector>

namespace fincore {

/// Native runtime declarations for the `metrics.*` and `performance.*`
/// operation domains that are not already part of builtin_catalog().
///
/// Every declaration maps to an existing native kernel. The declarations are
/// deterministic and consume no random state (`rng_policy == "none"`).
std::vector<operation_spec> metrics_performance_operations();

}  // namespace fincore
