#pragma once

#include <fincore/factor/analysis.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Operation identifier of the shared factor analysis kernel.
inline constexpr const char* factor_analyze_operation_id =
    "factor_analysis.analysis.analyze_factor";

/// One named, renderer-independent factor workflow.
///
/// ``build`` is the canonical analysis builder; it never re-runs a workflow
/// specific kernel. ``renderer`` stays empty because native workflows return
/// prepared models and chart data rather than Matplotlib figures.
struct factor_workflow {
    std::string workflow_id; ///< Workflow identifier such as `"summary"`.
    std::string model_operation_id; ///< Analysis-kernel operation identifier.
    std::vector<std::string> sections; ///< Ordered analysis section names.
    std::optional<std::string> renderer; ///< Optional renderer ID; empty here.
    /// Builds the analysis model from prepared data and configuration.
    factor_analysis_model (*build)(const prepared_factor_data&,
                                   const factor_analysis_config&) =
        &analyze_factor;
};

/// Returns the built-in named factor workflows.
///
/// The list covers the summary, returns, information, turnover, full,
/// event_returns and event_study workflows.
///
/// @return Reference to the immutable workflow list.
const std::vector<factor_workflow>& factor_workflows();

/// Looks up a workflow by identifier and returns nullptr when absent.
///
/// @param workflow_id Workflow identifier to find.
/// @return Pointer to the matching workflow, or nullptr when absent.
const factor_workflow* find_factor_workflow(std::string_view workflow_id);

/// Stable operation descriptors for the factor runtime catalog.
struct factor_operation_descriptor {
    std::string operation_id; ///< Stable operation identifier.
    std::string capability_id; ///< Equals `operation_id` when generated.
    std::string domain; ///< Equals `"factor_analysis"` when generated.
    std::map<std::string, std::string> provenance; ///< Owner and kernel keys.
};

/// Returns stable operation descriptors for the factor runtime catalog.
///
/// @return Reference to the immutable descriptor list.
const std::vector<factor_operation_descriptor>& factor_operation_descriptors();

/// Computes SHA-256 over the canonical descriptor listing (identifier order).
///
/// @return Lowercase hexadecimal SHA-256 digest.
std::string factor_operations_digest();

}  // namespace fincore
