#include <fincore/factor/workflows.hpp>

#include <fincore/detail/digest.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

factor_analysis_model build_factor_model(
    const prepared_factor_data& data, const factor_analysis_config& config) {
    return analyze_factor(data, config);
}

std::vector<factor_workflow> make_workflows() {
    return {
        {"summary",
         factor_analyze_operation_id,
         {"returns", "information", "turnover"},
         std::nullopt,
         &build_factor_model},
        {"returns",
         factor_analyze_operation_id,
         {"returns"},
         std::nullopt,
         &build_factor_model},
        {"information",
         factor_analyze_operation_id,
         {"information"},
         std::nullopt,
         &build_factor_model},
        {"turnover",
         factor_analyze_operation_id,
         {"turnover"},
         std::nullopt,
         &build_factor_model},
        {"full",
         factor_analyze_operation_id,
         {"returns", "information", "turnover"},
         std::nullopt,
         &build_factor_model},
        {"event_returns",
         factor_analyze_operation_id,
         {"event_returns"},
         std::nullopt,
         &build_factor_model},
        {"event_study",
         factor_analyze_operation_id,
         {"event_distribution", "event_returns", "returns"},
         std::nullopt,
         &build_factor_model},
    };
}

std::vector<std::string> operation_ids() {
    return {
        "factor_analysis.analysis.analyze_factor",
        "factor_analysis.calendar.add_custom_calendar_timedelta",
        "factor_analysis.calendar.backshift_returns_series",
        "factor_analysis.calendar.diff_custom_calendar_timedeltas",
        "factor_analysis.calendar.get_forward_returns_columns",
        "factor_analysis.calendar.infer_trading_calendar",
        "factor_analysis.calendar.timedelta_strings_to_integers",
        "factor_analysis.calendar.timedelta_to_string",
        "factor_analysis.costs.apply_factor_costs",
        "factor_analysis.costs.estimate_factor_capacity",
        "factor_analysis.data.compute_forward_returns",
        "factor_analysis.data.prepare_factor_data",
        "factor_analysis.data.prepare_factor_data_by_horizon",
        "factor_analysis.data.prepare_factor_data_from_forward_returns",
        "factor_analysis.data.prepare_pit_factor_data",
        "factor_analysis.data.quantize_factor",
        "factor_analysis.inference.benjamini_hochberg",
        "factor_analysis.inference.factor_model_inference",
        "factor_analysis.inference.fama_macbeth",
        "factor_analysis.inference.ic_confidence_interval",
        "factor_analysis.inference.ic_mean",
        "factor_analysis.inference.ic_t_stat",
        "factor_analysis.inference.information_coefficient_inference",
        "factor_analysis.performance.average_cumulative_return_by_quantile",
        "factor_analysis.performance.common_start_returns",
        "factor_analysis.performance.compute_mean_returns_spread",
        "factor_analysis.performance.cumulative_returns",
        "factor_analysis.performance.factor_alpha_beta",
        "factor_analysis.performance.factor_information_coefficient",
        "factor_analysis.performance.factor_rank_autocorrelation",
        "factor_analysis.performance.factor_returns",
        "factor_analysis.performance.factor_weights",
        "factor_analysis.performance.mean_information_coefficient",
        "factor_analysis.performance.mean_return_by_quantile",
        "factor_analysis.performance.quantile_turnover",
        "factor_analysis.pit.materialize_pit_factor",
        "factor_analysis.pit.validate_pit_alignment",
        "factor_analysis.portfolio.build_factor_portfolio_inputs",
        "factor_analysis.portfolio.factor_cumulative_returns",
        "factor_analysis.portfolio.factor_positions",
        "factor_analysis.portfolio.positions",
    };
}

std::string kernel_module(const std::string& operation_id) {
    const std::size_t last_dot = operation_id.rfind('.');
    return "fincore." +
           (last_dot == std::string::npos
                ? operation_id
                : operation_id.substr(0U, last_dot));
}

std::vector<factor_operation_descriptor> make_operation_descriptors() {
    std::vector<factor_operation_descriptor> descriptors;
    const std::vector<std::string> ids = operation_ids();
    descriptors.reserve(ids.size());
    for (const std::string& id : ids) {
        factor_operation_descriptor descriptor;
        descriptor.operation_id = id;
        descriptor.capability_id = id;
        descriptor.domain = "factor_analysis";
        descriptor.provenance = {{"owner", "factor_analysis"},
                                 {"kernel_module", kernel_module(id)}};
        descriptors.push_back(std::move(descriptor));
    }
    return descriptors;
}

}  // namespace

const std::vector<factor_workflow>& factor_workflows() {
    static const std::vector<factor_workflow> workflows = make_workflows();
    return workflows;
}

const factor_workflow* find_factor_workflow(
    const std::string_view workflow_id) {
    for (const factor_workflow& workflow : factor_workflows()) {
        if (workflow.workflow_id == workflow_id) {
            return &workflow;
        }
    }
    return nullptr;
}

const std::vector<factor_operation_descriptor>&
factor_operation_descriptors() {
    static const std::vector<factor_operation_descriptor> descriptors =
        make_operation_descriptors();
    return descriptors;
}

std::string factor_operations_digest() {
    std::string canonical = "fincore-factor-operations-v1\n";
    for (const factor_operation_descriptor& descriptor :
         factor_operation_descriptors()) {
        canonical += descriptor.operation_id;
        canonical.push_back('\x1f');
        canonical += descriptor.capability_id;
        canonical.push_back('\x1f');
        canonical += descriptor.domain;
        for (const auto& [key, value] : descriptor.provenance) {
            canonical.push_back('\x1f');
            canonical += key;
            canonical.push_back('=');
            canonical += value;
        }
        canonical.push_back('\n');
    }
    return detail::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(canonical.data()),
        canonical.size()));
}

double legacy_forward_returns_loss(const factor_loss_report& report) {
    if (report.input_count == 0U) {
        return 0.0;
    }
    return static_cast<double>(report.input_count -
                               report.forward_returns_count) /
           static_cast<double>(report.input_count);
}

}  // namespace fincore
