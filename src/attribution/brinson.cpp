#include <fincore/attribution/brinson.hpp>

#include <fincore/error.hpp>

#include <cstddef>
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
