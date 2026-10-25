#include <fincore/factor/research_detailed.hpp>

#include "research_detailed_internal.hpp"

#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/performance.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <set>
#include <string>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::string csv_list(const std::vector<std::string>& values) {
    std::string result;
    for (const auto& value : values) {
        if (!result.empty()) {
            result.push_back(',');
        }
        result += value;
    }
    return result;
}

template <typename T>
std::string csv_list(const std::vector<T>& values) {
    std::string result;
    for (const auto value : values) {
        if (!result.empty()) {
            result.push_back(',');
        }
        result += std::to_string(value);
    }
    return result;
}

std::string python_repr(const std::string_view value) {
    std::string result{"'"};
    for (const char character : value) {
        if (character == '\\' || character == '\'') {
            result.push_back('\\');
        }
        if (character == '\n') {
            result += "\\n";
        } else if (character == '\r') {
            result += "\\r";
        } else if (character == '\t') {
            result += "\\t";
        } else {
            result.push_back(character);
        }
    }
    result.push_back('\'');
    return result;
}

double pearson(const std::vector<double>& left,
               const std::vector<double>& right) {
    if (left.size() != right.size() || left.size() < 2U) {
        return nan;
    }
    double left_mean = 0.0;
    double right_mean = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        left_mean += left[index];
        right_mean += right[index];
    }
    left_mean /= static_cast<double>(left.size());
    right_mean /= static_cast<double>(right.size());
    double covariance = 0.0;
    double left_variance = 0.0;
    double right_variance = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const double x = left[index] - left_mean;
        const double y = right[index] - right_mean;
        covariance += x * y;
        left_variance += x * x;
        right_variance += y * y;
    }
    const double inverse_degrees_of_freedom =
        1.0 / static_cast<double>(left.size() - 1U);
    covariance *= inverse_degrees_of_freedom;
    left_variance *= inverse_degrees_of_freedom;
    right_variance *= inverse_degrees_of_freedom;
    if (!(left_variance > 0.0) || !(right_variance > 0.0)) {
        return nan;
    }
    // Match the pinned pandas/NumPy corrcoef path: np.cov first scales each
    // covariance by 1/(n-1), then corrcoef divides successively by each
    // standard deviation and clips the result. A single sqrt(var_x*var_y)
    // division is algebraically equivalent but can erase one-ulp differences
    // that affect the detailed ICIR summary for near-perfect daily ICs.
    const double left_deviation = std::sqrt(left_variance);
    const double right_deviation = std::sqrt(right_variance);
    double value = covariance / left_deviation;
    value /= right_deviation;
    if (!std::isfinite(value)) {
        return nan;
    }
    return std::clamp(value, -1.0, 1.0);
}

struct summary_values {
    double mean = nan;
    double deviation = nan;
    double ratio = nan;
    std::size_t count = 0U;
};

summary_values summarize(const std::vector<double>& values) {
    summary_values result;
    result.mean = 0.0;
    for (const double value : values) {
        if (std::isfinite(value)) {
            result.mean += value;
            ++result.count;
        }
    }
    if (result.count == 0U) {
        result.mean = nan;
        return result;
    }
    result.mean /= static_cast<double>(result.count);
    if (result.count < 2U) {
        return result;
    }
    double squared = 0.0;
    for (const double value : values) {
        if (std::isfinite(value)) {
            const double delta = value - result.mean;
            squared += delta * delta;
        }
    }
    result.deviation = std::sqrt(
        squared / static_cast<double>(result.count - 1U));
    if (result.deviation > 0.0 && std::isfinite(result.deviation)) {
        result.ratio = result.mean / result.deviation;
    }
    return result;
}

void append_diagnostic(research_detailed_result& result,
                       const std::string& factor,
                       const std::string& stage,
                       const std::string& severity,
                       const std::string& code,
                       const std::string& message) {
    result.diagnostics.push_back(
        {factor, stage, severity, code, message});
}

std::vector<double> preprocess_cross_section(
    const std::vector<std::size_t>& positions, factor_series& panel,
    const std::string& method) {
    std::vector<double> values;
    values.reserve(positions.size());
    for (const std::size_t position : positions) {
        values.push_back(panel.values[position]);
    }
    if (method == "none") {
        return values;
    }
    if (method == "cross_section_rank") {
        std::vector<std::size_t> order;
        order.reserve(values.size());
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (!std::isnan(values[index])) {
                order.push_back(index);
            }
        }
        std::stable_sort(order.begin(), order.end(),
                         [&values](const std::size_t left,
                                   const std::size_t right) {
                             return values[left] < values[right];
                         });
        for (std::size_t first = 0; first < order.size();) {
            std::size_t last = first + 1U;
            while (last < order.size() &&
                   values[order[last]] == values[order[first]]) {
                ++last;
            }
            const double average_rank =
                (static_cast<double>(first + 1U) +
                 static_cast<double>(last)) /
                2.0;
            const double percentile =
                average_rank / static_cast<double>(order.size());
            for (std::size_t rank = first; rank < last; ++rank) {
                panel.values[positions[order[rank]]] = percentile;
            }
            first = last;
        }
        return values;
    }

    // pandas skips NaN but includes +/-Inf. Any infinite member makes the
    // row-wise mean/std projection undefined; preserve that outcome instead
    // of silently coercing infinity to a missing value before preprocessing.
    if (std::any_of(values.begin(), values.end(),
                    [](const double value) { return std::isinf(value); })) {
        for (const std::size_t position : positions) {
            panel.values[position] = nan;
        }
        return values;
    }
    std::vector<double> finite;
    finite.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            finite.push_back(value);
        }
    }
    if (finite.size() < 2U) {
        for (const std::size_t position : positions) {
            panel.values[position] = nan;
        }
        return values;
    }
    const double mean =
        std::accumulate(finite.begin(), finite.end(), 0.0) /
        static_cast<double>(finite.size());
    double squared = 0.0;
    for (const double value : finite) {
        const double delta = value - mean;
        squared += delta * delta;
    }
    const double deviation = std::sqrt(
        squared / static_cast<double>(finite.size() - 1U));
    if (!(deviation > 0.0) || !std::isfinite(deviation)) {
        for (const std::size_t position : positions) {
            panel.values[position] = nan;
        }
        return values;
    }
    for (std::size_t index = 0; index < positions.size(); ++index) {
        panel.values[positions[index]] =
            std::isnan(values[index]) ? nan : (values[index] - mean) / deviation;
    }
    return values;
}

std::size_t count_finite(const std::vector<double>& values) {
    return static_cast<std::size_t>(std::count_if(
        values.begin(), values.end(),
        [](const double value) { return std::isfinite(value); }));
}

double numpy_percentile(const std::vector<double>& sorted,
                        const double percentile) {
    if (sorted.empty()) {
        throw factor_data_error{"cannot quantize an empty group"};
    }
    // pandas qcut builds probabilities with np.linspace, multiplies them by
    // 100, then calls Series.quantile -> np.percentile. Preserve those two
    // floating-point conversions and NumPy's interpolation branch: the
    // latter intentionally interpolates from the upper endpoint for gamma
    // >= 0.5 to reduce cancellation.
    const double virtual_index =
        static_cast<double>(sorted.size() - 1U) * percentile;
    const auto lower = static_cast<std::size_t>(std::floor(virtual_index));
    const auto upper = static_cast<std::size_t>(std::ceil(virtual_index));
    if (lower == upper) {
        return sorted[lower];
    }
    const double gamma = virtual_index - static_cast<double>(lower);
    const double left = sorted[lower];
    const double right = sorted[upper];
    if (gamma >= 0.5) {
        return right - (right - left) * (1.0 - gamma);
    }
    return left + (right - left) * gamma;
}

std::vector<double> pandas_qcut_edges(const std::vector<double>& values,
                                      const int quantiles) {
    std::vector<double> sorted;
    sorted.reserve(values.size());
    for (const double value : values) {
        if (std::isfinite(value)) {
            sorted.push_back(value);
        }
    }
    std::sort(sorted.begin(), sorted.end());
    if (sorted.empty()) {
        throw factor_data_error{"cannot quantize an empty group"};
    }

    std::vector<double> edges;
    edges.reserve(static_cast<std::size_t>(quantiles) + 1U);
    const double step = 1.0 / static_cast<double>(quantiles);
    for (int index = 0; index <= quantiles; ++index) {
        const double probability =
            index == quantiles
                ? 1.0
                : static_cast<double>(index) * step;
        const double pandas_percentile = probability * 100.0;
        const double numpy_probability = pandas_percentile / 100.0;
        edges.push_back(numpy_percentile(sorted, numpy_probability));
    }
    for (std::size_t index = 1U; index < edges.size(); ++index) {
        if (!(edges[index] > edges[index - 1U])) {
            throw factor_data_error{"Bin edges must be unique"};
        }
    }
    return edges;
}

int pandas_qcut_label(const double value, const std::vector<double>& edges) {
    // qcut uses right-closed intervals and searchsorted(side="left").
    // Values are drawn from the finite sample used to construct the endpoints.
    const auto upper = std::lower_bound(edges.begin() + 1, edges.end(), value);
    if (upper == edges.end()) {
        return static_cast<int>(edges.size()) - 1;
    }
    return static_cast<int>(upper - edges.begin());
}

factor_loss_report make_detailed_loss_report(
    const std::size_t input_count, const std::size_t finite_factor_count,
    const std::size_t forward_returns_count,
    const std::size_t binning_count) {
    if (input_count == 0U) {
        throw factor_data_error{"factor must contain at least one observation"};
    }
    factor_loss_report report;
    report.input_count = input_count;
    report.finite_factor_count = finite_factor_count;
    report.forward_returns_count = forward_returns_count;
    report.binning_count = binning_count;
    const double denominator = static_cast<double>(input_count);
    report.factor_input_loss =
        static_cast<double>(input_count - finite_factor_count) / denominator;
    report.forward_returns_loss =
        static_cast<double>(finite_factor_count - forward_returns_count) /
        denominator;
    report.binning_loss =
        static_cast<double>(forward_returns_count - binning_count) /
        denominator;
    report.total_loss = report.factor_input_loss +
                        report.forward_returns_loss + report.binning_loss;
    return report;
}

struct horizon_date_run {
    std::size_t begin = 0U;
    std::size_t end = 0U;
};

struct detailed_horizon_workspace {
    // Each entry points into the already-owned panel and horizon label arrays.
    // The rows and their quantiles stay in (date, asset) order.
    std::vector<std::size_t> panel_rows;
    std::vector<int> quantiles;
    std::vector<horizon_date_run> date_runs;
    std::vector<int> available_quantiles;
    factor_loss_report loss_report;
};

detailed_horizon_workspace prepare_detailed_horizon(
    const factor_series& panel, const std::vector<double>& labels,
    const research_spec& spec) {
    if (labels.size() != panel.dates.size()) {
        throw factor_data_error{
            "detailed forward labels must match panel row count"};
    }

    detailed_horizon_workspace prepared;
    prepared.panel_rows.reserve(panel.dates.size());
    prepared.quantiles.reserve(panel.dates.size());
    std::set<int> available_quantiles;
    std::vector<std::size_t> valid_rows;
    std::vector<double> date_values;
    std::size_t finite_factor_count = 0U;
    std::size_t forward_returns_count = 0U;
    std::size_t binning_count = 0U;

    for (std::size_t date_begin = 0U; date_begin < panel.dates.size();) {
        std::size_t date_end = date_begin + 1U;
        while (date_end < panel.dates.size() &&
               panel.dates[date_end] == panel.dates[date_begin]) {
            ++date_end;
        }

        valid_rows.clear();
        date_values.clear();
        for (std::size_t row = date_begin; row < date_end; ++row) {
            if (!std::isfinite(panel.values[row])) {
                continue;
            }
            ++finite_factor_count;
            if (!std::isfinite(labels[row])) {
                continue;
            }
            ++forward_returns_count;
            valid_rows.push_back(row);
            date_values.push_back(panel.values[row]);
        }

        if (!valid_rows.empty()) {
            std::vector<double> edges;
            try {
                edges = pandas_qcut_edges(date_values, spec.quantiles);
            } catch (const factor_data_error&) {
                if (spec.max_loss == 0.0) {
                    throw;
                }
                // Python's qcut wrapper catches a per-date binning exception
                // when no_raise=True and leaves that cross-section unassigned.
                date_begin = date_end;
                continue;
            }

            const std::size_t run_begin = prepared.panel_rows.size();
            for (const std::size_t row : valid_rows) {
                const int quantile =
                    pandas_qcut_label(panel.values[row], edges);
                prepared.panel_rows.push_back(row);
                prepared.quantiles.push_back(quantile);
                available_quantiles.insert(quantile);
                ++binning_count;
            }
            prepared.date_runs.push_back(
                {run_begin, prepared.panel_rows.size()});
        }

        date_begin = date_end;
    }

    prepared.loss_report = make_detailed_loss_report(
        panel.dates.size(), finite_factor_count, forward_returns_count,
        binning_count);
    if (prepared.loss_report.total_loss > spec.max_loss) {
        std::ostringstream message;
        message << std::fixed << std::setprecision(1)
                << "max_loss (" << spec.max_loss * 100.0
                << "%) exceeded " << prepared.loss_report.total_loss * 100.0
                << "%, consider increasing it.";
        throw factor_loss_exceeded{message.str(), prepared.loss_report};
    }
    prepared.available_quantiles.assign(available_quantiles.begin(),
                                          available_quantiles.end());
    return prepared;
}

std::vector<double> average_ranks_including_infinities(
    const std::vector<double>& values) {
    std::vector<std::size_t> order;
    order.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (!std::isnan(values[index])) {
            order.push_back(index);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&values](const auto left,
                                                          const auto right) {
        return values[left] < values[right];
    });
    std::vector<double> result(values.size(), nan);
    for (std::size_t first = 0U; first < order.size();) {
        std::size_t last = first + 1U;
        while (last < order.size() &&
               values[order[last]] == values[order[first]]) {
            ++last;
        }
        const double rank =
            (static_cast<double>(first + 1U) + static_cast<double>(last)) /
            2.0;
        for (std::size_t position = first; position < last; ++position) {
            result[order[position]] = rank;
        }
        first = last;
    }
    return result;
}

void average_ranks_including_infinities_into(
    const std::span<const double> values, const std::span<double> result) {
    if (values.size() != result.size()) {
        throw factor_data_error{
            "detailed rank input and output must have equal lengths"};
    }
    std::fill(result.begin(), result.end(), nan);
    std::vector<std::size_t> order;
    order.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (!std::isnan(values[index])) {
            order.push_back(index);
        }
    }
    std::stable_sort(order.begin(), order.end(), [&values](const auto left,
                                                          const auto right) {
        return values[left] < values[right];
    });
    for (std::size_t first = 0U; first < order.size();) {
        std::size_t last = first + 1U;
        while (last < order.size() &&
               values[order[last]] == values[order[first]]) {
            ++last;
        }
        const double rank =
            (static_cast<double>(first + 1U) + static_cast<double>(last)) /
            2.0;
        for (std::size_t position = first; position < last; ++position) {
            result[order[position]] = rank;
        }
        first = last;
    }
}

int turnover_lag(const std::string& label) {
    if (label.empty() || label.back() != 'D') {
        return 1;
    }
    try {
        std::size_t consumed = 0U;
        const int value = std::stoi(label.substr(0U, label.size() - 1U),
                                    &consumed);
        return consumed == label.size() - 1U && value > 0 ? value : 1;
    } catch (...) {
        return 1;
    }
}

void add_manifest(research_detailed_result& result,
                  const std::string& factor_name,
                  const research_spec& spec,
                  const int direction,
                  const std::vector<std::string>& periods) {
    std::ostringstream max_loss;
    max_loss << std::setprecision(std::numeric_limits<double>::max_digits10)
             << spec.max_loss;
    result.manifest = {
        {"api_version", research_detailed_api_version},
        {"schema_version", std::to_string(research_detailed_result_schema_version)},
        {"factor", factor_name},
        {"label", spec.label},
        {"preprocess", spec.preprocess},
        {"start_ns", std::to_string(spec.start)},
        {"end_ns", std::to_string(spec.end)},
        {"targets", csv_list(spec.targets)},
        {"periods", csv_list(periods)},
        {"quantiles", std::to_string(spec.quantiles)},
        {"min_assets", std::to_string(spec.min_assets)},
        {"min_quantile_assets", std::to_string(spec.min_quantile_assets)},
        {"max_loss", max_loss.str()},
        {"stability_lags", csv_list(spec.stability_lags)},
        {"direction", std::to_string(direction)},
        {"snapshot_id", spec.snapshot_id},
        {"price_basis", spec.price_basis},
        {"calendar_id", spec.calendar_id},
        {"frequency", spec.frequency},
        {"groups_count", std::to_string(spec.groups.size())},
    };
}

void add_table_counts(research_detailed_result& result) {
    result.manifest["rows_signals"] =
        std::to_string(result.signals.size());
    result.manifest["rows_labels"] = std::to_string(result.labels.size());
    result.manifest["rows_ic"] = std::to_string(result.ic.size());
    result.manifest["rows_ic_summary"] =
        std::to_string(result.ic_summary.size());
    result.manifest["rows_quantile_returns"] =
        std::to_string(result.quantile_returns.size());
    result.manifest["rows_turnover"] =
        std::to_string(result.turnover.size());
    result.manifest["rows_spreads"] = std::to_string(result.spreads.size());
    result.manifest["rows_stability"] =
        std::to_string(result.stability.size());
    result.manifest["rows_correlation"] =
        std::to_string(result.correlation.size());
    result.manifest["rows_comparisons"] =
        std::to_string(result.comparisons.size());
    result.manifest["rows_coverage"] =
        std::to_string(result.coverage.size());
    result.manifest["rows_diagnostics"] =
        std::to_string(result.diagnostics.size());
}

}  // namespace

research_detailed_result evaluate_factor_research_detailed(
    const std::string_view factor_name, const factor_series& factors,
    const dataframe& prices, const research_spec& spec) {
    if (factor_name.empty()) {
        throw factor_data_error{"factor name must not be empty"};
    }
    if (factors.dates.size() != factors.assets.size() ||
        factors.dates.size() != factors.values.size()) {
        throw factor_data_error{
            "factor dates, assets and values must have equal lengths"};
    }
    if (spec.label == "precomputed") {
        throw factor_data_error{
            "precomputed labels require the precomputed evaluator"};
    }
    if (spec.preprocess != "none" &&
        spec.preprocess != "cross_section_rank" &&
        spec.preprocess != "cross_section_zscore") {
        throw factor_data_error{
            "preprocess must be cross_section_rank, cross_section_zscore or "
            "none"};
    }
    if (spec.min_assets < 1 || spec.min_quantile_assets < 1) {
        throw factor_data_error{
            "min_assets and min_quantile_assets must be positive integers"};
    }
    if (spec.quantiles < 2) {
        throw factor_data_error{"quantiles must be at least two"};
    }
    std::set<int> unique_stability_lags;
    for (const int lag : spec.stability_lags) {
        if (lag <= 0) {
            throw factor_data_error{
                "stability_lags must contain positive integers"};
        }
        if (!unique_stability_lags.insert(lag).second) {
            throw factor_data_error{
                "stability_lags must not contain duplicates"};
        }
    }
    if (!spec.combinations.empty() || !spec.comparisons.empty()) {
        throw factor_data_error{
            "single-factor detailed evaluation does not support combinations "
            "or comparisons"};
    }
    for (const auto& [name, direction] : spec.direction_overrides) {
        if (name != factor_name) {
            throw factor_data_error{
                "direction_overrides names an unknown factor: " + name};
        }
        if (direction != -1 && direction != 1) {
            throw factor_data_error{
                "factor directions must be +1 or -1"};
        }
    }
    for (const auto& [asset, group] : spec.groups) {
        static_cast<void>(group);
        if (asset.empty()) {
            throw factor_data_error{"groups must have non-empty asset names"};
        }
    }

    const int direction = spec.direction_overrides.contains(
                              std::string{factor_name})
                              ? spec.direction_overrides.at(
                                    std::string{factor_name})
                              : 1;
    research_spec validation_spec = spec;
    validation_spec.direction_overrides.clear();
    validation_spec.groups.clear();
    validation_spec.combinations.clear();
    validation_spec.comparisons.clear();
    validation_spec.preprocess = "none";
    factor_series selected = research_detailed_detail::select_panel(factors, validation_spec);

    struct selected_row {
        timestamp time;
        std::string asset;
        double adjusted;
    };
    std::vector<selected_row> ordered;
    ordered.reserve(selected.dates.size());
    // select_panel returns a stable source-order subsequence and has already
    // validated duplicate keys. Walk the source to recover original values,
    // since its validation copy canonicalizes non-finite values to NaN.
    std::size_t source_row = 0U;
    for (std::size_t row = 0; row < selected.dates.size(); ++row) {
        while (source_row < factors.dates.size() &&
               (factors.dates[source_row] != selected.dates[row] ||
                factors.assets[source_row] != selected.assets[row])) {
            ++source_row;
        }
        if (source_row == factors.dates.size()) {
            throw factor_data_error{
                "selected research panel could not be mapped to input rows"};
        }
        ordered.push_back({selected.dates[row], selected.assets[row],
                           factors.values[source_row] *
                               static_cast<double>(direction)});
        ++source_row;
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const selected_row& left, const selected_row& right) {
                  return std::tie(left.time, left.asset) <
                         std::tie(right.time, right.asset);
              });
    factor_series panel;
    panel.dates.reserve(ordered.size());
    panel.assets.reserve(ordered.size());
    panel.values.reserve(ordered.size());
    std::map<std::string, std::string> groups;
    for (const auto& row : ordered) {
        panel.dates.push_back(row.time);
        panel.assets.push_back(row.asset);
        panel.values.push_back(row.adjusted);
        if (!spec.groups.empty() && !spec.groups.contains(row.asset)) {
            throw factor_data_error{
                "groups do not cover every evaluated asset"};
        }
        groups.emplace(row.asset,
                       spec.groups.empty() ? "default"
                                           : spec.groups.at(row.asset));
    }

    std::set<std::string> available_price_columns;
    for (const auto& info : prices.get_columns_info<double>()) {
        if (std::get<2>(info) == typeid(double)) {
            available_price_columns.emplace(std::get<0>(info).c_str());
        }
    }
    std::vector<std::string> missing_price_columns;
    for (const auto& asset : panel.assets) {
        if (!available_price_columns.contains(asset) &&
            std::find(missing_price_columns.begin(),
                      missing_price_columns.end(), asset) ==
                missing_price_columns.end()) {
            missing_price_columns.push_back(asset);
        }
    }
    if (!missing_price_columns.empty()) {
        throw factor_data_error{
            "prices are missing target assets: " +
            csv_list(missing_price_columns)};
    }

    std::map<timestamp, std::vector<std::size_t>> rows_by_date;
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        rows_by_date[panel.dates[row]].push_back(row);
    }
    std::size_t collapsed_cross_sections = 0U;
    for (const auto& [date, positions] : rows_by_date) {
        static_cast<void>(date);
        const bool had_finite = std::any_of(
            positions.begin(), positions.end(), [&panel](const std::size_t row) {
                return std::isfinite(panel.values[row]);
            });
        static_cast<void>(preprocess_cross_section(positions, panel,
                                                   spec.preprocess));
        const bool has_finite = std::any_of(
            positions.begin(), positions.end(), [&panel](const std::size_t row) {
                return std::isfinite(panel.values[row]);
            });
        if (had_finite && !has_finite) {
            ++collapsed_cross_sections;
        }
    }

    research_detailed_result result;
    if (collapsed_cross_sections != 0U) {
        append_diagnostic(
            result, std::string{factor_name}, "preprocess", "warning",
            "preprocess_collapsed_cross_section",
            std::to_string(collapsed_cross_sections) +
                " cross-sections for factor " + python_repr(factor_name) +
                " became fully undefined under preprocess=" +
                python_repr(spec.preprocess));
    }

    auto label_input = research_detailed_detail::build_detailed_label_input(
        panel, prices, spec);
    result.signals.reserve(panel.dates.size());
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        const double adjusted = ordered[row].adjusted;
        result.signals.push_back(
            {panel.dates[row], panel.assets[row], std::string{factor_name},
             direction, adjusted, panel.values[row], spec.preprocess,
             groups.at(panel.assets[row])});
    }
    std::vector<unsigned char> has_any_label(panel.dates.size(), 0U);
    result.labels.reserve(panel.dates.size() * label_input.horizons.size());
    for (const auto& horizon : label_input.horizons) {
        if (horizon.label_values.size() != panel.dates.size()) {
            throw factor_data_error{
                "each detailed label horizon must match the panel row count"};
        }
        for (std::size_t row = 0; row < panel.dates.size(); ++row) {
            const double value = horizon.label_values[row];
            if (std::isfinite(value)) {
                has_any_label[row] = 1U;
            }
            result.labels.push_back({panel.dates[row], panel.assets[row],
                                     horizon.period, value});
        }
    }
    const std::size_t label_cells = static_cast<std::size_t>(std::count(
        has_any_label.begin(), has_any_label.end(), 1U));
    std::size_t evaluable_cells = 0U;
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        if (has_any_label[row] != 0U && std::isfinite(panel.values[row])) {
            ++evaluable_cells;
        }
    }

    std::vector<std::size_t> n_dates;
    n_dates.reserve(label_input.horizons.size());
    for (std::size_t horizon = 0; horizon < label_input.horizons.size();
         ++horizon) {
        const auto& input = label_input.horizons[horizon];
        const auto finite_factor = count_finite(panel.values);
        const auto finite_label = count_finite(input.label_values);
        const std::string factor{factor_name};
        if (finite_factor == 0U) {
            append_diagnostic(
                result, factor, "prepare", "error", "no_finite_factor",
                "factor " + python_repr(factor_name) +
                    " has no finite values in the study window");
            continue;
        }
        if (finite_label == 0U) {
            append_diagnostic(
                result, factor, "prepare", "error", "no_labels",
                "forward returns " + python_repr(input.period) +
                    " are unavailable for every " + python_repr(factor_name) +
                    " cell");
            continue;
        }

        detailed_horizon_workspace prepared;
        try {
            prepared = prepare_detailed_horizon(
                panel, input.label_values, spec);
        } catch (const factor_loss_exceeded& error) {
            append_diagnostic(
                result, factor, "prepare", "error", "max_loss_exceeded",
                std::string{error.what()} + " (period " + input.period +
                    ", factor " + python_repr(factor_name) + ")");
            continue;
        } catch (const factor_data_error& error) {
            append_diagnostic(
                result, factor, "prepare", "warning", "preparation_failed",
                "period " + input.period + ", factor " +
                    python_repr(factor_name) + ": " + error.what());
            continue;
        }
        if (prepared.panel_rows.empty()) {
            append_diagnostic(
                result, factor, "prepare", "warning", "no_evaluable_cells",
                "period " + input.period + ", factor " +
                    python_repr(factor_name) +
                    " has no rows after cleaning and binning");
            continue;
        }

        std::size_t skipped_dates = 0U;
        std::vector<double> factors_on_date;
        std::vector<double> returns_on_date;
        std::size_t largest_date_run = 0U;
        for (const auto& run : prepared.date_runs) {
            largest_date_run =
                std::max(largest_date_run, run.end - run.begin);
        }
        factors_on_date.reserve(largest_date_run);
        returns_on_date.reserve(largest_date_run);
        for (const auto& run : prepared.date_runs) {
            factors_on_date.clear();
            returns_on_date.clear();
            for (std::size_t index = run.begin; index < run.end; ++index) {
                const std::size_t row = prepared.panel_rows[index];
                factors_on_date.push_back(panel.values[row]);
                returns_on_date.push_back(input.label_values[row]);
            }
            if (factors_on_date.size() <
                static_cast<std::size_t>(spec.min_assets)) {
                ++skipped_dates;
                continue;
            }
            const auto factor_ranks =
                average_ranks_including_infinities(factors_on_date);
            const auto return_ranks =
                average_ranks_including_infinities(returns_on_date);
            const double ic = pearson(factors_on_date, returns_on_date);
            const double rank_ic = pearson(factor_ranks, return_ranks);
            const std::size_t first_panel_row =
                prepared.panel_rows[run.begin];
            result.ic.push_back(
                {panel.dates[first_panel_row], factor, input.period, ic,
                 rank_ic, factors_on_date.size()});
        }
        if (skipped_dates != 0U) {
            append_diagnostic(
                result, factor, "ic", "warning", "insufficient_assets",
                "period " + input.period + ", factor " +
                    python_repr(factor_name) + ": " +
                    std::to_string(skipped_dates) +
                    " dates had fewer than min_assets=" +
                    std::to_string(spec.min_assets) + " evaluable assets");
        }
        const std::size_t quantile_count =
            prepared.available_quantiles.size();
        const std::size_t date_count = prepared.date_runs.size();
        std::vector<std::size_t> quantile_slots(
            static_cast<std::size_t>(spec.quantiles) + 1U,
            std::numeric_limits<std::size_t>::max());
        for (std::size_t quantile = 0; quantile < quantile_count;
             ++quantile) {
            const int value = prepared.available_quantiles[quantile];
            quantile_slots[static_cast<std::size_t>(value)] = quantile;
        }
        std::vector<double> daily_sums(date_count * quantile_count, 0.0);
        std::vector<std::size_t> daily_counts(date_count * quantile_count, 0U);
        for (std::size_t date = 0; date < date_count; ++date) {
            const auto& run = prepared.date_runs[date];
            for (std::size_t index = run.begin; index < run.end; ++index) {
                const std::size_t row = prepared.panel_rows[index];
                const double value = input.label_values[row];
                if (!std::isfinite(value)) {
                    continue;
                }
                const std::size_t slot = quantile_slots[
                    static_cast<std::size_t>(prepared.quantiles[index])];
                const std::size_t cell = date * quantile_count + slot;
                // Match the generic mean helper's per-bucket insertion and
                // std::accumulate order: panel rows are date/asset ordered.
                daily_sums[cell] += value;
                ++daily_counts[cell];
            }
        }
        const auto daily_mean = [&daily_sums, &daily_counts,
                                 quantile_count](const std::size_t date,
                                                 const std::size_t quantile) {
            const std::size_t cell = date * quantile_count + quantile;
            if (daily_counts[cell] == 0U) {
                return nan;
            }
            return daily_sums[cell] /
                   static_cast<double>(daily_counts[cell]);
        };
        std::size_t filtered_quantiles = 0U;
        for (std::size_t quantile = 0; quantile < quantile_count;
             ++quantile) {
            const int quantile_value =
                prepared.available_quantiles[quantile];
            for (std::size_t date = 0; date < date_count; ++date) {
                const std::size_t cell = date * quantile_count + quantile;
                const std::size_t observations = daily_counts[cell];
                if (observations == 0U) {
                    continue;
                }
                const double mean = daily_mean(date, quantile);
                if (!std::isfinite(mean)) {
                    continue;
                }
                if (observations <
                    static_cast<std::size_t>(spec.min_quantile_assets)) {
                    ++filtered_quantiles;
                    continue;
                }
                const std::size_t first_panel_row =
                    prepared.panel_rows[prepared.date_runs[date].begin];
                result.quantile_returns.push_back(
                    {panel.dates[first_panel_row], factor, input.period,
                     quantile_value, mean, observations});
            }
        }
        if (filtered_quantiles != 0U) {
            append_diagnostic(
                result, factor, "quantile", "warning", "min_quantile_assets",
                "period " + input.period + ", factor " +
                    python_repr(factor_name) + ": dropped " +
                    std::to_string(filtered_quantiles) +
                    " quantile observations below min_quantile_assets=" +
                    std::to_string(spec.min_quantile_assets));
        }

        if (quantile_count >= 2U) {
            const int bottom = prepared.available_quantiles.front();
            const int top = prepared.available_quantiles.back();
            const std::size_t bottom_slot = 0U;
            const std::size_t top_slot = quantile_count - 1U;
            for (std::size_t date = 0; date < date_count; ++date) {
                if (daily_counts[date * quantile_count + bottom_slot] == 0U ||
                    daily_counts[date * quantile_count + top_slot] == 0U) {
                    continue;
                }
                const double high = daily_mean(date, top_slot);
                const double low = daily_mean(date, bottom_slot);
                if (!std::isfinite(high) || !std::isfinite(low)) {
                    continue;
                }
                const std::size_t first_panel_row =
                    prepared.panel_rows[prepared.date_runs[date].begin];
                result.spreads.push_back(
                    {panel.dates[first_panel_row], factor, input.period,
                     high - low, top, bottom});
            }
        } else {
            append_diagnostic(
                result, factor, "quantile", "warning",
                "quantile_spread_unavailable",
                "factor " + python_repr(factor_name) +
                    " has fewer than two populated quantiles");
        }

        const std::size_t lag =
            static_cast<std::size_t>(turnover_lag(input.period));
        for (std::size_t quantile_slot = 0U;
             quantile_slot < quantile_count; ++quantile_slot) {
            const int quantile =
                prepared.available_quantiles[quantile_slot];
            std::vector<std::size_t> quantile_dates;
            quantile_dates.reserve(date_count);
            for (std::size_t date = 0U; date < date_count; ++date) {
                if (daily_counts[date * quantile_count + quantile_slot] !=
                    0U) {
                    quantile_dates.push_back(date);
                }
            }
            for (std::size_t position = lag;
                 position < quantile_dates.size(); ++position) {
                const std::size_t current_date = quantile_dates[position];
                const std::size_t previous_date =
                    quantile_dates[position - lag];
                const auto& current_run = prepared.date_runs[current_date];
                const auto& previous_run =
                    prepared.date_runs[previous_date];
                std::size_t current_index = current_run.begin;
                std::size_t previous_index = previous_run.begin;
                std::size_t common = 0U;
                while (current_index < current_run.end &&
                       previous_index < previous_run.end) {
                    if (prepared.quantiles[current_index] != quantile) {
                        ++current_index;
                        continue;
                    }
                    if (prepared.quantiles[previous_index] != quantile) {
                        ++previous_index;
                        continue;
                    }
                    const auto& current_asset = panel.assets[
                        prepared.panel_rows[current_index]];
                    const auto& previous_asset = panel.assets[
                        prepared.panel_rows[previous_index]];
                    if (current_asset < previous_asset) {
                        ++current_index;
                    } else if (previous_asset < current_asset) {
                        ++previous_index;
                    } else {
                        ++common;
                        ++current_index;
                        ++previous_index;
                    }
                }
                const std::size_t current_count =
                    daily_counts[current_date * quantile_count +
                                 quantile_slot];
                const double turnover =
                    static_cast<double>(current_count - common) /
                    static_cast<double>(current_count);
                if (std::isfinite(turnover)) {
                    const std::size_t first_panel_row =
                        prepared.panel_rows[current_run.begin];
                    result.turnover.push_back(
                        {panel.dates[first_panel_row], factor, input.period,
                         quantile, turnover});
                }
            }
        }
    }

    for (const auto& horizon : label_input.horizons) {
        std::vector<double> pearson_values;
        std::vector<double> rank_values;
        for (const auto& row : result.ic) {
            if (row.period != horizon.period) {
                continue;
            }
            pearson_values.push_back(row.ic);
            rank_values.push_back(row.rank_ic);
        }
        const auto ic_summary = summarize(pearson_values);
        const auto rank_summary = summarize(rank_values);
        n_dates.push_back(ic_summary.count);
        result.ic_summary.push_back(
            {std::string{factor_name}, horizon.period,
             ic_summary.mean, ic_summary.deviation, ic_summary.ratio,
             rank_summary.mean, rank_summary.deviation, rank_summary.ratio,
             ic_summary.count});
    }

    std::vector<double> stability_ranks(panel.dates.size(), nan);
    std::vector<std::size_t> stability_date_offsets{0U};
    for (std::size_t date_begin = 0U; date_begin < panel.dates.size();) {
        std::size_t date_end = date_begin + 1U;
        while (date_end < panel.dates.size() &&
               panel.dates[date_end] == panel.dates[date_begin]) {
            ++date_end;
        }
        const std::size_t count = date_end - date_begin;
        average_ranks_including_infinities_into(
            std::span<const double>{panel.values.data() + date_begin, count},
            std::span<double>{stability_ranks.data() + date_begin, count});
        stability_date_offsets.push_back(date_end);
        date_begin = date_end;
    }
    const auto stability =
        research_detailed_detail::factor_rank_autocorrelation_ranked_panel(
            panel, std::span<const double>{stability_ranks},
            std::span<const std::size_t>{stability_date_offsets},
            spec.stability_lags);
    for (std::size_t lag = 0; lag < stability.size(); ++lag) {
        const auto summary = summarize(stability[lag].values);
        result.stability.push_back(
            {std::string{factor_name}, spec.stability_lags[lag],
             summary.mean, summary.deviation, summary.count});
        if (summary.count == 0U) {
            append_diagnostic(
                result, std::string{factor_name}, "stability", "warning",
                "stability_lag_unavailable",
                "factor " + python_repr(factor_name) +
                    " has no rank autocorrelation at lag " +
                    std::to_string(spec.stability_lags[lag]));
        }
    }

    const bool any_evaluated = std::any_of(
        n_dates.begin(), n_dates.end(),
        [](const std::size_t value) { return value != 0U; });
    const bool all_evaluated = !n_dates.empty() && std::all_of(
        n_dates.begin(), n_dates.end(),
        [](const std::size_t value) { return value != 0U; });
    result.status = any_evaluated ? "OK" : "UNEVALUABLE";
    const std::string coverage_status =
        !any_evaluated ? "UNEVALUABLE"
                       : (all_evaluated ? "OK" : "PARTIAL");
    result.coverage.push_back(
        {std::string{factor_name}, "factor", direction, panel.dates.size(),
         count_finite(panel.values), label_cells, evaluable_cells,
         coverage_status});
    add_manifest(result, std::string{factor_name}, spec, direction,
                 label_input.periods);
    add_table_counts(result);
    return result;
}

}  // namespace fincore
