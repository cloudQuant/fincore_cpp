#include <fincore/attribution/brinson.hpp>

#include <fincore/error.hpp>
#include <fincore/errors.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct selected_columns {
    std::vector<std::span<const double>> portfolio_returns;
    std::vector<std::span<const double>> benchmark_returns;
    std::vector<std::span<const double>> portfolio_weights;
    std::vector<std::span<const double>> benchmark_weights;
};

struct output_column {
    std::string name;
    std::vector<double> values;
};

std::vector<std::string> checked_sector_names(
    const std::span<const std::string_view> sector_column_names) {
    if (sector_column_names.empty()) {
        throw input_error{"sector column names must not be empty"};
    }

    std::vector<std::string> names;
    names.reserve(sector_column_names.size());
    for (const std::string_view name : sector_column_names) {
        if (name.empty()) {
            throw input_error{"sector column names must not contain an empty name"};
        }
        const std::string owned_name{name};
        if (!std::unordered_set<std::string>{names.begin(), names.end()}
                 .insert(owned_name)
                 .second) {
            throw input_error{"sector column names must be distinct"};
        }
        names.push_back(owned_name);
    }
    return names;
}

void require_unique_index(const dataframe& frame) {
    std::unordered_set<timestamp> values;
    values.reserve(frame.get_index().size());
    for (const timestamp value : frame.get_index()) {
        if (!values.insert(value).second) {
            throw input_error{"Brinson indexes must not contain duplicates"};
        }
    }
}

void require_matching_index(const dataframe& expected, const dataframe& actual) {
    if (expected.get_index() != actual.get_index()) {
        throw input_error{"Brinson indexes must match exactly"};
    }
}

selected_columns validate_inputs(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    const std::vector<std::string>& sector_names) {
    require_unique_index(portfolio_returns);
    require_unique_index(benchmark_returns);
    require_unique_index(portfolio_weights);
    require_unique_index(benchmark_weights);
    require_matching_index(portfolio_returns, benchmark_returns);
    require_matching_index(portfolio_returns, portfolio_weights);
    require_matching_index(portfolio_returns, benchmark_weights);

    selected_columns columns;
    columns.portfolio_returns.reserve(sector_names.size());
    columns.benchmark_returns.reserve(sector_names.size());
    columns.portfolio_weights.reserve(sector_names.size());
    columns.benchmark_weights.reserve(sector_names.size());
    for (const std::string& name : sector_names) {
        columns.portfolio_returns.push_back(
            require_double_column(portfolio_returns, name));
        columns.benchmark_returns.push_back(
            require_double_column(benchmark_returns, name));
        columns.portfolio_weights.push_back(
            require_double_column(portfolio_weights, name));
        columns.benchmark_weights.push_back(
            require_double_column(benchmark_weights, name));
    }
    return columns;
}

dataframe make_result_frame(const std::vector<timestamp>& index,
                            std::vector<output_column> columns) {
    dataframe result;
    result.load_index(std::vector<timestamp>{index});
    for (auto& column : columns) {
        result.load_column<double>(column.name.c_str(), std::move(column.values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

}  // namespace

dataframe brinson_results(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    const std::span<const std::string_view> sector_column_names) {
    const auto sector_names = checked_sector_names(sector_column_names);
    const auto columns = validate_inputs(
        portfolio_returns, benchmark_returns, portfolio_weights,
        benchmark_weights, sector_names);
    const auto& index = portfolio_returns.get_index();

    std::vector<double> allocation(index.size(), 0.0);
    std::vector<double> selection(index.size(), 0.0);
    std::vector<double> interaction(index.size(), 0.0);
    std::vector<double> total(index.size(), 0.0);
    std::vector<double> portfolio(index.size(), 0.0);
    std::vector<double> benchmark(index.size(), 0.0);
    for (std::size_t row = 0U; row < index.size(); ++row) {
        for (std::size_t sector = 0U; sector < sector_names.size(); ++sector) {
            const double portfolio_return = columns.portfolio_returns[sector][row];
            const double benchmark_return = columns.benchmark_returns[sector][row];
            const double portfolio_weight = columns.portfolio_weights[sector][row];
            const double benchmark_weight = columns.benchmark_weights[sector][row];
            allocation[row] +=
                (portfolio_weight - benchmark_weight) * benchmark_return;
            selection[row] +=
                benchmark_weight * (portfolio_return - benchmark_return);
            interaction[row] += (portfolio_weight - benchmark_weight) *
                                (portfolio_return - benchmark_return);
            portfolio[row] += portfolio_weight * portfolio_return;
            benchmark[row] += benchmark_weight * benchmark_return;
        }
        total[row] = allocation[row] + selection[row] + interaction[row];
    }

    return make_result_frame(
        index, {{std::string{brinson_allocation_column}, std::move(allocation)},
                {std::string{brinson_selection_column}, std::move(selection)},
                {std::string{brinson_interaction_column}, std::move(interaction)},
                {std::string{brinson_total_column}, std::move(total)},
                {std::string{brinson_portfolio_return_column}, std::move(portfolio)},
                {std::string{brinson_benchmark_return_column}, std::move(benchmark)}});
}

brinson_period_result brinson_attribution(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    const std::span<const std::string_view> sector_column_names) {
    const dataframe result = brinson_results(
        portfolio_returns, benchmark_returns, portfolio_weights,
        benchmark_weights, sector_column_names);
    if (result.get_index().size() != 1U) {
        throw input_error{"Brinson single-period attribution requires exactly one row"};
    }
    return {result.get_column<double>(brinson_allocation_column.data())[0],
            result.get_column<double>(brinson_selection_column.data())[0],
            result.get_column<double>(brinson_interaction_column.data())[0],
            result.get_column<double>(brinson_total_column.data())[0],
            result.get_column<double>(brinson_portfolio_return_column.data())[0],
            result.get_column<double>(brinson_benchmark_return_column.data())[0]};
}

brinson_cumulative_result brinson_cumulative(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    const std::span<const std::string_view> sector_column_names) {
    const dataframe result = brinson_results(
        portfolio_returns, benchmark_returns, portfolio_weights,
        benchmark_weights, sector_column_names);
    const auto allocation = result.get_column<double>(brinson_allocation_column.data());
    const auto selection = result.get_column<double>(brinson_selection_column.data());
    const auto interaction = result.get_column<double>(brinson_interaction_column.data());
    const auto total = result.get_column<double>(brinson_total_column.data());
    const auto portfolio =
        result.get_column<double>(brinson_portfolio_return_column.data());
    const auto benchmark =
        result.get_column<double>(brinson_benchmark_return_column.data());

    brinson_cumulative_result cumulative{};
    double portfolio_wealth = 1.0;
    double benchmark_wealth = 1.0;
    for (std::size_t row = 0U; row < result.get_index().size(); ++row) {
        cumulative.allocation += allocation[row];
        cumulative.selection += selection[row];
        cumulative.interaction += interaction[row];
        cumulative.total += total[row];
        portfolio_wealth *= 1.0 + portfolio[row];
        benchmark_wealth *= 1.0 + benchmark[row];
    }
    cumulative.portfolio_cumulative = portfolio_wealth - 1.0;
    cumulative.benchmark_cumulative = benchmark_wealth - 1.0;
    return cumulative;
}

}  // namespace fincore

namespace fincore {
namespace {

std::vector<std::string> double_column_names(const dataframe& frame) {
    std::vector<std::string> names;
    for (const auto& info : frame.get_columns_info<double>()) {
        names.emplace_back(std::get<0>(info).c_str());
    }
    return names;
}

dataframe apply_sector_mapping(
    const dataframe& frame,
    const std::map<std::string, std::vector<std::string>>& mapping,
    const bool sum) {
    dataframe result;
    const auto& index = frame.get_index();
    result.load_index(std::vector<timestamp>{index.begin(), index.end()});
    for (const auto& [sector, assets] : mapping) {
        if (sector.empty()) {
            throw fincore_error{
                validation_error("sector mapping names must not be empty")};
        }
        if (assets.empty()) {
            throw fincore_error{validation_error(
                "sector mapping assets must not be empty", sector)};
        }
        std::vector<double> values(index.size(), 0.0);
        for (const std::string& asset : assets) {
            const auto column = require_double_column(frame, asset);
            if (column.size() != index.size()) {
                throw fincore_error{validation_error(
                    "sector mapping asset column length must match the index",
                    asset)};
            }
            for (std::size_t row = 0U; row < index.size(); ++row) {
                values[row] += column[row];
            }
        }
        if (!sum) {
            const double count = static_cast<double>(assets.size());
            for (double& value : values) {
                value /= count;
            }
        }
        result.load_column<double>(sector.c_str(), std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

dataframe equal_weights_frame(const std::vector<timestamp>& index,
                              const std::vector<std::string>& columns) {
    if (columns.empty()) {
        throw fincore_error{validation_error(
            "Brinson attribution requires at least one return column")};
    }
    const double weight = 1.0 / static_cast<double>(columns.size());
    dataframe result;
    result.load_index(std::vector<timestamp>{index});
    for (const std::string& column : columns) {
        result.load_column<double>(column.c_str(),
                                   std::vector<double>(index.size(), weight),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

}  // namespace

BrinsonAttribution::BrinsonAttribution(
    std::map<std::string, std::vector<std::string>> sector_mapping)
    : sector_mapping_(std::move(sector_mapping)) {}

const std::map<std::string, std::vector<std::string>>&
BrinsonAttribution::sector_mapping() const noexcept {
    return sector_mapping_;
}

dataframe BrinsonAttribution::calculate(
    const dataframe& returns, std::optional<dataframe> benchmark_returns,
    std::optional<dataframe> weights, const std::string_view method) const {
    if (method != "brinson" && method != "brinson_hood") {
        throw fincore_error{validation_error(
            "Unknown attribution method. Use: 'brinson' or 'brinson_hood'.",
            "method", std::string{method})};
    }

    dataframe portfolio = returns;
    std::optional<dataframe> benchmark = std::move(benchmark_returns);
    std::optional<dataframe> portfolio_weights = std::move(weights);
    if (!sector_mapping_.empty()) {
        portfolio = apply_sector_mapping(returns, sector_mapping_, false);
        if (benchmark.has_value()) {
            benchmark =
                apply_sector_mapping(*benchmark, sector_mapping_, false);
        }
        if (portfolio_weights.has_value()) {
            portfolio_weights =
                apply_sector_mapping(*portfolio_weights, sector_mapping_, true);
        }
    }

    if (!benchmark.has_value()) {
        benchmark = portfolio;
    }
    const auto columns = double_column_names(portfolio);
    if (!portfolio_weights.has_value()) {
        portfolio_weights = equal_weights_frame(portfolio.get_index(), columns);
    }
    const dataframe benchmark_weights =
        equal_weights_frame(portfolio.get_index(), columns);

    std::vector<std::string_view> sector_names;
    sector_names.reserve(columns.size());
    for (const std::string& column : columns) {
        sector_names.emplace_back(column);
    }
    return brinson_results(portfolio, *benchmark, *portfolio_weights,
                           benchmark_weights, sector_names);
}

}  // namespace fincore
