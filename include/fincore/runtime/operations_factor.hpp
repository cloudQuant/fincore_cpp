#pragma once

#include <fincore/runtime/runtime.hpp>

#include <vector>

namespace fincore {

/// Native-compatible runtime operations for the Iteration 8 P105 domains:
/// attribution, factor_analysis, optimization, report.factor and the native
/// data-snapshot model.
///
/// Every declaration maps an exact source operation id to an existing native
/// kernel. Composite native results are projected onto an owning
/// ``report_document``; tabular kernels return an owning ``dataframe``.
/// ``rng_policy`` is ``none`` and every operation is deterministic.
std::vector<operation_spec> attribution_factor_operations();

}  // namespace fincore
