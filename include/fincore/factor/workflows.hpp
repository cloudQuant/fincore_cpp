#pragma once

#include <fincore/factor/analysis.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr const char* factor_analyze_operation_id =
    "factor_analysis.analysis.analyze_factor";

/// One named, renderer-independent factor workflow.
///
/// ``build`` is the canonical analysis builder; it never re-runs a workflow
/// specific kernel. ``renderer`` stays empty because native workflows return
/// prepared models and chart data rather than Matplotlib figures.
struct factor_workflow {
    std::string workflow_id;
    std::string model_operation_id;
    std::vector<std::string> sections;
    std::optional<std::string> renderer;
    factor_analysis_model (*build)(const prepared_factor_data&,
                                   const factor_analysis_config&) =
        &analyze_factor;
};

const std::vector<factor_workflow>& factor_workflows();

/// Look up a workflow by identifier; returns nullptr when absent.
const factor_workflow* find_factor_workflow(std::string_view workflow_id);

/// Stable operation descriptors for the factor runtime catalog.
struct factor_operation_descriptor {
    std::string operation_id;
    std::string capability_id;
    std::string domain;
    std::map<std::string, std::string> provenance;
};

const std::vector<factor_operation_descriptor>& factor_operation_descriptors();

/// SHA-256 over the canonical descriptor listing (identifier order).
std::string factor_operations_digest();

}  // namespace fincore
