#include <fincore/metrics/positions.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <typeindex>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

struct numeric_column {
    std::string name;
    std::span<const double> values;
};

struct output_column {
    std::string name;
    std::vector<double> values;
};

std::string checked_cash_column_name(const std::string_view cash_column_name) {
    if (cash_column_name.empty()) {
        throw input_error{"cash column name must not be empty"};
    }
    return std::string{cash_column_name};
}

std::vector<numeric_column> numeric_columns(const dataframe& positions) {
    const auto columns = positions.get_columns_info<double>();
    const auto& index = positions.get_index();

    std::vector<numeric_column> result;
    result.reserve(columns.size());
    for (const auto& info : columns) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }

        const std::string name{std::get<0>(info).c_str()};
        const auto& values = positions.get_column<double>(name.c_str());
        if (values.size() != index.size()) {
            throw input_error{
                "numeric position columns must match the frame index length"};
        }
        result.push_back({name, values});
    }
    if (result.empty()) {
        throw input_error{"positions must contain at least one double column"};
    }
    return result;
}

const numeric_column& required_cash_column(
    const std::vector<numeric_column>& columns,
    const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto cash = std::ranges::find_if(
        columns, [&cash_name](const numeric_column& column) {
            return column.name == cash_name;
        });
    if (cash == columns.end()) {
        throw input_error{"cash column must be a double position column"};
    }
    return *cash;
}

dataframe make_frame(const std::vector<timestamp>& index,
                     std::vector<output_column> columns) {
    dataframe result;
    auto output_index = index;
    result.load_index(std::move(output_index));
    for (auto& column : columns) {
        result.load_column<double>(column.name.c_str(), std::move(column.values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

double row_sum_skipping_nan(const std::vector<numeric_column>& columns,
                            const std::size_t row) {
    double total = 0.0;
    for (const auto& column : columns) {
        const double value = column.values[row];
        if (!std::isnan(value)) {
            total += value;
        }
    }
    return total;
}

double finite_or_nan(const double value) {
    return std::isinf(value) ? nan : value;
}

std::vector<output_column> percent_allocation_columns(
    const std::vector<numeric_column>& columns,
    const std::size_t row_count) {
    std::vector<output_column> result;
    result.reserve(columns.size());
    for (const auto& column : columns) {
        result.push_back({column.name, std::vector<double>(row_count, nan)});
    }

    for (std::size_t row = 0U; row < row_count; ++row) {
        const double total = row_sum_skipping_nan(columns, row);
        for (std::size_t column = 0U; column < columns.size(); ++column) {
            result[column].values[row] =
                finite_or_nan(columns[column].values[row] / total);
        }
    }
    return result;
}

double median(std::vector<double> values) {
    if (values.empty()) {
        return nan;
    }
    std::ranges::sort(values);
    const std::size_t midpoint = values.size() / 2U;
    if (values.size() % 2U != 0U) {
        return values[midpoint];
    }
    return (values[midpoint - 1U] + values[midpoint]) / 2.0;
}

}  // namespace

dataframe get_percent_alloc(const dataframe& positions) {
    const auto columns = numeric_columns(positions);
    auto allocations =
        percent_allocation_columns(columns, positions.get_index().size());
    return make_frame(positions.get_index(), std::move(allocations));
}

dataframe get_long_short_pos(const dataframe& positions,
                             const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto columns = numeric_columns(positions);
    const std::size_t row_count = positions.get_index().size();
    std::vector<double> long_values(row_count, 0.0);
    std::vector<double> short_values(row_count, 0.0);

    for (const auto& column : columns) {
        if (column.name == cash_name) {
            continue;
        }
        for (std::size_t row = 0U; row < row_count; ++row) {
            const double value = column.values[row];
            if (std::isnan(value)) {
                continue;
            }
            if (value > 0.0) {
                long_values[row] += value;
            } else if (value < 0.0) {
                short_values[row] -= value;
            }
        }
    }
    return make_frame(positions.get_index(),
                      {{"long", std::move(long_values)},
                       {"short", std::move(short_values)}});
}

dataframe get_max_median_position_concentration(
    const dataframe& positions,
    const std::string_view cash_column_name) {
    const auto columns = numeric_columns(positions);
    const auto& cash = required_cash_column(columns, cash_column_name);
    const std::size_t row_count = positions.get_index().size();
    auto allocations = percent_allocation_columns(columns, row_count);
    std::vector<double> max_long_values(row_count, nan);
    std::vector<double> median_long_values(row_count, nan);
    std::vector<double> median_short_values(row_count, nan);
    std::vector<double> max_short_values(row_count, nan);

    for (std::size_t row = 0U; row < row_count; ++row) {
        std::vector<double> long_allocations;
        std::vector<double> short_allocations;
        long_allocations.reserve(allocations.size());
        short_allocations.reserve(allocations.size());
        for (std::size_t column = 0U; column < allocations.size(); ++column) {
            if (allocations[column].name == cash.name) {
                continue;
            }
            const double allocation = allocations[column].values[row];
            if (allocation > 0.0) {
                long_allocations.push_back(allocation);
            } else if (allocation < 0.0) {
                short_allocations.push_back(allocation);
            }
        }
        if (!long_allocations.empty()) {
            max_long_values[row] = *std::ranges::max_element(long_allocations);
            median_long_values[row] = median(std::move(long_allocations));
        }
        if (!short_allocations.empty()) {
            median_short_values[row] = median(short_allocations);
            max_short_values[row] = *std::ranges::min_element(short_allocations);
        }
    }
    return make_frame(positions.get_index(),
                      {{"max_long", std::move(max_long_values)},
                       {"median_long", std::move(median_long_values)},
                       {"median_short", std::move(median_short_values)},
                       {"max_short", std::move(max_short_values)}});
}

dataframe gross_lev(const dataframe& positions,
                    const std::string_view cash_column_name) {
    const auto columns = numeric_columns(positions);
    const auto& cash = required_cash_column(columns, cash_column_name);
    const std::size_t row_count = positions.get_index().size();
    std::vector<double> values(row_count, nan);

    for (std::size_t row = 0U; row < row_count; ++row) {
        double gross_exposure = 0.0;
        for (const auto& column : columns) {
            if (column.name == cash.name) {
                continue;
            }
            const double value = column.values[row];
            if (!std::isnan(value)) {
                gross_exposure += std::abs(value);
            }
        }
        values[row] = finite_or_nan(
            gross_exposure / row_sum_skipping_nan(columns, row));
    }
    return make_frame(positions.get_index(), {{"gross_lev", std::move(values)}});
}

}  // namespace fincore
