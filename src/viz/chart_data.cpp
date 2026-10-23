#include <fincore/viz/chart_data.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

void require_sorted_index(const dataframe& frame,
                          const std::string_view context) {
    const auto& index = frame.get_index();
    for (std::size_t position = 1U; position < index.size(); ++position) {
        if (index[position] < index[position - 1U]) {
            throw input_error{std::string(context) +
                              " index must be sorted in ascending order"};
        }
    }
}

std::pair<int, unsigned> year_month(const timestamp value) {
    using nanosecond_time_point =
        std::chrono::time_point<std::chrono::system_clock,
                                std::chrono::nanoseconds>;
    const auto point = std::chrono::floor<std::chrono::days>(
        nanosecond_time_point{std::chrono::nanoseconds{value}});
    const std::chrono::year_month_day calendar{point};
    return {static_cast<int>(calendar.year()),
            static_cast<unsigned>(calendar.month())};
}

}  // namespace

heat_grid build_monthly_heatmap(const dataframe& returns,
                                const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    require_sorted_index(returns, "monthly heatmap");
    const auto values = require_double_column(returns, column);
    const auto& index = returns.get_index();

    struct month_accumulator {
        double product = 1.0;
        bool valid = true;
    };
    std::map<std::pair<int, unsigned>, month_accumulator> months;
    for (std::size_t position = 0U; position < values.size(); ++position) {
        const auto [year, month] = year_month(index[position]);
        auto& accumulator = months[{year, month}];
        const double value = values[position];
        if (!std::isfinite(value) || !std::isfinite(accumulator.product)) {
            accumulator.valid = false;
            continue;
        }
        accumulator.product *= 1.0 + value;
        if (!std::isfinite(accumulator.product)) {
            accumulator.valid = false;
        }
    }

    heat_grid grid;
    grid.columns.assign(std::begin(month_labels), std::end(month_labels));
    std::size_t year_count = 0U;
    int previous_year = 0;
    bool first_year = true;
    for (const auto& [key, accumulator] : months) {
        static_cast<void>(accumulator);
        if (first_year || key.first != previous_year) {
            ++year_count;
            previous_year = key.first;
            first_year = false;
        }
    }
    grid.years.reserve(year_count);
    grid.values.assign(year_count * 12U, nan);
    grid.present.assign(year_count * 12U, false);
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    bool any_value = false;
    std::size_t row = 0U;
    int current_year = 0;
    bool first_row = true;
    for (const auto& [key, accumulator] : months) {
        if (first_row || key.first != current_year) {
            grid.years.push_back(key.first);
            current_year = key.first;
            ++row;
            first_row = false;
        }
        const std::size_t cell =
            (row - 1U) * 12U + static_cast<std::size_t>(key.second - 1U);
        if (!accumulator.valid) {
            continue;
        }
        const double monthly =
            (accumulator.product - 1.0) * 100.0;
        if (!std::isfinite(monthly)) {
            continue;
        }
        grid.values[cell] = monthly;
        grid.present[cell] = true;
        minimum = std::min(minimum, monthly);
        maximum = std::max(maximum, monthly);
        any_value = true;
    }

    if (!any_value) {
        grid.color_min = -10.0;
        grid.color_max = 10.0;
    } else if (std::abs(maximum - minimum) < 1e-12) {
        grid.color_min = minimum - 1.0;
        grid.color_max = maximum + 1.0;
    } else {
        grid.color_min = minimum;
        grid.color_max = maximum;
    }
    grid.color_centered =
        grid.color_min < 0.0 && 0.0 < grid.color_max;
    return grid;
}

series_chart build_series_chart(
    const dataframe& frame, const std::vector<series_chart_spec>& specs,
    std::string title, std::string y_label, const bool zero_line,
    const bool fill_to_zero) {
    if (specs.empty()) {
        throw input_error{"series chart requires at least one series"};
    }
    series_chart chart;
    chart.title = std::move(title);
    chart.y_label = std::move(y_label);
    chart.zero_line = zero_line;
    chart.fill_to_zero = fill_to_zero;
    chart.series.reserve(specs.size());
    for (const auto& spec : specs) {
        if (spec.label.empty()) {
            throw input_error{"series labels must not be empty"};
        }
        const auto values = require_double_column(frame, spec.column);
        chart_series series;
        series.label = spec.label;
        series.index.assign(frame.get_index().begin(),
                            frame.get_index().end());
        series.values.assign(values.begin(), values.end());
        series.benchmark = spec.benchmark;
        series.dashed = spec.benchmark;
        chart.series.push_back(std::move(series));
    }
    return chart;
}

series_chart build_returns_chart(const dataframe& frame,
                                 const std::string_view column,
                                 const std::string_view label) {
    return build_series_chart(
        frame, {{std::string(column), std::string(label), false}},
        "Cumulative Returns", "Cumulative Return", true);
}

series_chart build_rolling_sharpe_chart(
    const dataframe& portfolio, const std::string_view column,
    const std::size_t window, const dataframe* benchmark,
    const std::string_view benchmark_column,
    const std::string_view label,
    const std::string_view benchmark_label) {
    series_chart chart = build_series_chart(
        portfolio,
        {{std::string(column), std::string(label), false}},
        "Rolling Sharpe Ratio (" + std::to_string(window) +
            "-day window)",
        "Sharpe Ratio", true);
    chart.window = window;
    if (benchmark != nullptr) {
        if (benchmark_label.empty()) {
            throw input_error{"benchmark label must not be empty"};
        }
        const auto values =
            require_double_column(*benchmark, benchmark_column);
        chart_series series;
        series.label = std::string(benchmark_label);
        series.index.assign(benchmark->get_index().begin(),
                            benchmark->get_index().end());
        series.values.assign(values.begin(), values.end());
        series.benchmark = true;
        series.dashed = true;
        chart.series.push_back(std::move(series));
    }
    return chart;
}

series_chart build_drawdown_chart(const dataframe& frame,
                                  const std::string_view column,
                                  const std::string_view label) {
    series_chart chart = build_series_chart(
        frame, {{std::string(column), std::string(label), false}},
        "Drawdown", "Drawdown", false, true);
    return chart;
}

bar_chart build_bar_chart(std::vector<std::string> group_labels,
                          std::vector<std::string> series_labels,
                          std::vector<double> values) {
    if (group_labels.empty() || series_labels.empty()) {
        throw input_error{
            "bar chart requires at least one group and one series"};
    }
    for (const auto& label : group_labels) {
        if (label.empty()) {
            throw input_error{"bar group labels must not be empty"};
        }
    }
    for (const auto& label : series_labels) {
        if (label.empty()) {
            throw input_error{"bar series labels must not be empty"};
        }
    }
    if (values.size() != group_labels.size() * series_labels.size()) {
        throw input_error{
            "bar values must have groups x series entries"};
    }
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw input_error{"bar values must be finite"};
        }
    }
    return {std::move(group_labels), std::move(series_labels),
            std::move(values)};
}

histogram_bins build_histogram(const std::span<const double> values,
                               const std::size_t bins) {
    if (bins == 0U) {
        throw input_error{"histogram bins must be at least 1"};
    }
    std::vector<double> finite;
    finite.reserve(values.size());
    for (const double value : values) {
        if (std::isfinite(value)) {
            finite.push_back(value);
        }
    }
    if (finite.empty()) {
        throw input_error{
            "histogram requires at least one finite observation"};
    }
    const auto [minimum, maximum] =
        std::minmax_element(finite.begin(), finite.end());
    double low = *minimum;
    double high = *maximum;
    if (!(high > low)) {
        low -= 0.5;
        high += 0.5;
    }
    const double width =
        (high - low) / static_cast<double>(bins);
    histogram_bins result;
    result.edges.resize(bins + 1U);
    result.counts.assign(bins, 0.0);
    result.density.assign(bins, 0.0);
    result.n_observations = finite.size();
    for (std::size_t index = 0U; index <= bins; ++index) {
        result.edges[index] =
            index == bins ? high
                          : low + width * static_cast<double>(index);
    }
    for (const double value : finite) {
        std::size_t index = 0U;
        if (value >= high) {
            index = bins - 1U;
        } else {
            double position = (value - low) / width;
            if (!(position > 0.0)) {
                position = 0.0;
            }
            if (position >= static_cast<double>(bins)) {
                position = static_cast<double>(bins - 1U);
            }
            index = static_cast<std::size_t>(position);
        }
        result.counts[index] += 1.0;
    }
    for (std::size_t index = 0U; index < bins; ++index) {
        result.density[index] =
            result.counts[index] /
            (static_cast<double>(finite.size()) * width);
    }
    return result;
}

correlation_matrix build_correlation_matrix(const dataframe& frame) {
    const auto columns = frame.get_columns_info<double>();
    std::vector<std::vector<double>> series;
    correlation_matrix result;
    for (const auto& info : columns) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }
        const std::string name{std::get<0>(info).c_str()};
        if (name.empty()) {
            throw input_error{
                "correlation column names must not be empty"};
        }
        result.labels.push_back(name);
        const auto& values = frame.get_column<double>(name.c_str());
        series.emplace_back(values.begin(), values.end());
    }
    if (result.labels.empty()) {
        throw input_error{
            "correlation requires at least one double column"};
    }
    const std::size_t count = result.labels.size();
    result.values.assign(count * count, nan);
    result.present.assign(count * count, false);
    for (std::size_t row = 0; row < count; ++row) {
        for (std::size_t column = 0; column < count; ++column) {
            std::vector<double> left;
            std::vector<double> right;
            for (std::size_t observation = 0;
                 observation < series[row].size(); ++observation) {
                const double first = series[row][observation];
                const double second = series[column][observation];
                if (std::isfinite(first) && std::isfinite(second)) {
                    left.push_back(first);
                    right.push_back(second);
                }
            }
            if (left.size() < 2U) {
                continue;
            }
            const double mean_left =
                std::accumulate(left.begin(), left.end(), 0.0) /
                static_cast<double>(left.size());
            const double mean_right =
                std::accumulate(right.begin(), right.end(), 0.0) /
                static_cast<double>(right.size());
            double covariance = 0.0;
            double variance_left = 0.0;
            double variance_right = 0.0;
            for (std::size_t index = 0; index < left.size(); ++index) {
                const double first = left[index] - mean_left;
                const double second = right[index] - mean_right;
                covariance += first * second;
                variance_left += first * first;
                variance_right += second * second;
            }
            if (!(variance_left > 0.0) || !(variance_right > 0.0)) {
                continue;
            }
            const double correlation =
                covariance /
                std::sqrt(variance_left * variance_right);
            if (std::isfinite(correlation)) {
                const std::size_t cell = row * count + column;
                result.values[cell] = correlation;
                result.present[cell] = true;
            }
        }
    }
    return result;
}

frontier_chart_data build_frontier_chart(
    const efficient_frontier_result& frontier) {
    const std::size_t points = frontier.frontier_returns.size();
    if (frontier.frontier_volatilities.size() != points ||
        frontier.frontier_sharpe.size() != points) {
        throw input_error{
            "frontier chart requires matching return/volatility/Sharpe "
            "lengths"};
    }
    auto require_finite = [](const std::vector<double>& values) {
        for (const double value : values) {
            if (!std::isfinite(value)) {
                throw input_error{
                    "frontier chart values must be finite"};
            }
        }
    };
    require_finite(frontier.frontier_returns);
    require_finite(frontier.frontier_volatilities);
    require_finite(frontier.frontier_sharpe);
    if (!std::isfinite(frontier.min_variance.expected_return) ||
        !std::isfinite(frontier.min_variance.volatility) ||
        !std::isfinite(frontier.max_sharpe.expected_return) ||
        !std::isfinite(frontier.max_sharpe.volatility)) {
        throw input_error{"frontier chart markers must be finite"};
    }
    frontier_chart_data chart;
    chart.volatilities = frontier.frontier_volatilities;
    chart.returns = frontier.frontier_returns;
    chart.sharpe = frontier.frontier_sharpe;
    chart.min_variance_volatility = frontier.min_variance.volatility;
    chart.min_variance_return = frontier.min_variance.expected_return;
    chart.max_sharpe_volatility = frontier.max_sharpe.volatility;
    chart.max_sharpe_return = frontier.max_sharpe.expected_return;
    return chart;
}

}  // namespace fincore
