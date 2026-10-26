#include <fincore/portfolio/positions.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <functional>
#include <map>
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

// These summaries read cash as part of the book, so the column is required and
// must be numeric, where the performance-attribution asset list treats it as
// optional and excludes it from factor exposure by name.
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

// A row's denominator skips missing positions, so a row that is entirely missing
// sums to zero and its ratios come out infinite; this maps those back to the
// unavailable marker, because an infinite percentage allocation would read as a
// real quantity rather than as a row with nothing to divide by.
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

namespace {

std::vector<numeric_column> asset_columns(const dataframe& positions,
                                          const std::string& cash_name) {
    std::vector<numeric_column> result;
    for (const auto& column : numeric_columns(positions)) {
        if (column.name != cash_name) {
            result.push_back(column);
        }
    }
    return result;
}

}  // namespace

top_positions get_top_long_short_abs(const dataframe& positions,
                                     const std::size_t top,
                                     const std::string_view cash_column_name) {
    if (cash_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto columns = asset_columns(positions, cash_name);
    top_positions result;
    std::vector<ranked_position> longs;
    std::vector<ranked_position> shorts;
    std::vector<ranked_position> absolutes;
    for (const auto& column : columns) {
        double maximum = nan;
        double minimum = nan;
        double absolute_maximum = nan;
        for (const double value : column.values) {
            if (std::isnan(value)) {
                continue;
            }
            maximum = std::isnan(maximum) ? value : std::max(maximum, value);
            minimum = std::isnan(minimum) ? value : std::min(minimum, value);
            const double absolute = std::fabs(value);
            absolute_maximum = std::isnan(absolute_maximum)
                                   ? absolute
                                   : std::max(absolute_maximum, absolute);
        }
        if (!std::isnan(maximum) && maximum > 0.0) {
            longs.push_back({column.name, maximum});
        }
        if (!std::isnan(minimum) && minimum < 0.0) {
            shorts.push_back({column.name, minimum});
        }
        if (!std::isnan(absolute_maximum)) {
            absolutes.push_back({column.name, absolute_maximum});
        }
    }
    std::sort(longs.begin(), longs.end(),
              [](const ranked_position& left, const ranked_position& right) {
                  return left.value > right.value;
              });
    std::sort(shorts.begin(), shorts.end(),
              [](const ranked_position& left, const ranked_position& right) {
                  return left.value < right.value;
              });
    std::sort(absolutes.begin(), absolutes.end(),
              [](const ranked_position& left, const ranked_position& right) {
                  return left.value > right.value;
              });
    if (longs.size() > top) {
        longs.resize(top);
    }
    if (shorts.size() > top) {
        shorts.resize(top);
    }
    if (absolutes.size() > top) {
        absolutes.resize(top);
    }
    result.long_positions = std::move(longs);
    result.short_positions = std::move(shorts);
    result.absolute_positions = std::move(absolutes);
    return result;
}

std::pair<std::vector<double>, std::vector<double>> get_long_short_notional(
    const dataframe& positions, const std::string_view cash_column_name) {
    if (cash_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto columns = asset_columns(positions, cash_name);
    const std::size_t row_count = positions.get_index().size();
    std::vector<double> longs(row_count, 0.0);
    std::vector<double> shorts(row_count, 0.0);
    for (std::size_t row = 0; row < row_count; ++row) {
        for (const auto& column : columns) {
            const double value = column.values[row];
            if (std::isnan(value)) {
                continue;
            }
            if (value > 0.0) {
                longs[row] += value;
            } else if (value < 0.0) {
                shorts[row] -= value;
            }
        }
    }
    return {std::move(longs), std::move(shorts)};
}

std::vector<position_record> stack_positions(
    const dataframe& positions, const bool pos_in_dollars,
    const std::string_view cash_column_name) {
    static_cast<void>(pos_in_dollars);
    if (cash_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto columns = asset_columns(positions, cash_name);
    const auto& index = positions.get_index();
    std::vector<position_record> result;
    result.reserve(index.size() * columns.size());
    for (std::size_t row = 0; row < index.size(); ++row) {
        for (const auto& column : columns) {
            result.push_back({index[row], column.name, column.values[row]});
        }
    }
    return result;
}

namespace {

void require_same_wide_index(const dataframe& left, const dataframe& right) {
    if (left.get_index() != right.get_index()) {
        throw input_error{"exposure panels must share the same index"};
    }
}

std::vector<std::string> asset_column_names(const dataframe& positions,
                                            const std::string& cash_name) {
    std::vector<std::string> names;
    for (const auto& column : numeric_columns(positions)) {
        if (column.name != cash_name) {
            names.push_back(column.name);
        }
    }
    return names;
}

std::vector<double> positive_row_sums(
    const dataframe& positions, const std::vector<std::string>& names) {
    const std::size_t rows = positions.get_index().size();
    std::vector<double> result(rows, 0.0);
    for (const auto& name : names) {
        const auto& values = positions.get_column<double>(name.c_str());
        for (std::size_t row = 0; row < rows; ++row) {
            if (!std::isnan(values[row]) && values[row] > 0.0) {
                result[row] += values[row];
            }
        }
    }
    return result;
}

std::vector<double> negative_abs_row_sums(
    const dataframe& positions, const std::vector<std::string>& names) {
    const std::size_t rows = positions.get_index().size();
    std::vector<double> result(rows, 0.0);
    for (const auto& name : names) {
        const auto& values = positions.get_column<double>(name.c_str());
        for (std::size_t row = 0; row < rows; ++row) {
            if (!std::isnan(values[row]) && values[row] < 0.0) {
                result[row] -= values[row];
            }
        }
    }
    return result;
}

std::vector<double> abs_row_sums(const dataframe& positions,
                                 const std::vector<std::string>& names) {
    const std::size_t rows = positions.get_index().size();
    std::vector<double> result(rows, 0.0);
    for (const auto& name : names) {
        const auto& values = positions.get_column<double>(name.c_str());
        for (std::size_t row = 0; row < rows; ++row) {
            if (!std::isnan(values[row])) {
                result[row] += std::fabs(values[row]);
            }
        }
    }
    return result;
}

std::vector<double> divide_or_zero(std::vector<double> numerator,
                                   std::vector<double> denominator) {
    for (std::size_t row = 0; row < numerator.size(); ++row) {
        if (denominator[row] == 0.0) {
            numerator[row] = 0.0;
        } else {
            const double value = numerator[row] / denominator[row];
            numerator[row] = std::isfinite(value) ? value : 0.0;
        }
    }
    return numerator;
}

double linear_quantile(std::vector<double> values, const double q) {
    values.erase(std::remove_if(values.begin(), values.end(),
                                [](const double value) {
                                    return std::isnan(value);
                                }),
                 values.end());
    if (values.empty()) {
        return std::nan("");
    }
    std::sort(values.begin(), values.end());
    const double virtual_index =
        static_cast<double>(values.size() - 1) * q;
    const auto lower = static_cast<std::size_t>(std::floor(virtual_index));
    const auto upper = static_cast<std::size_t>(std::ceil(virtual_index));
    if (lower == upper) {
        return values[lower];
    }
    const double difference = values[upper] - values[lower];
    const double fraction = virtual_index - static_cast<double>(lower);
    return fraction < 0.5 ? values[lower] + difference * fraction
                          : values[upper] - difference * (1.0 - fraction);
}

}  // namespace

dataframe get_sector_exposures(
    const dataframe& positions,
    const std::span<const sector_assignment> assignments,
    const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto& cash = positions.get_column<double>(cash_name.c_str());
    const auto assets = asset_column_names(positions, cash_name);
    std::map<std::string, std::vector<std::string>> by_sector;
    for (const auto& assignment : assignments) {
        by_sector[assignment.sector].push_back(assignment.symbol);
    }
    std::vector<output_column> output;
    const std::size_t rows = positions.get_index().size();
    for (const auto& [sector, symbols] : by_sector) {
        output.push_back({sector, std::vector<double>(rows, 0.0)});
        for (const auto& symbol : symbols) {
            if (std::find(assets.begin(), assets.end(), symbol) ==
                assets.end()) {
                continue;
            }
            const auto& values = positions.get_column<double>(symbol.c_str());
            for (std::size_t row = 0; row < rows; ++row) {
                if (!std::isnan(values[row])) {
                    output.back().values[row] += values[row];
                }
            }
        }
    }
    output.push_back({cash_name, std::vector<double>(cash.begin(),
                                                     cash.end())});
    return make_frame(positions.get_index(), std::move(output));
}

dataframe compute_style_factor_exposures(
    const dataframe& positions, const dataframe& risk_factors,
    const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    require_same_wide_index(positions, risk_factors);
    const auto assets = asset_column_names(positions, cash_name);
    const std::size_t rows = positions.get_index().size();
    std::vector<double> exposure(rows, 0.0);
    for (std::size_t row = 0; row < rows; ++row) {
        double numerator = 0.0;
        double gross = 0.0;
        for (const auto& asset : assets) {
            const double position =
                positions.get_column<double>(asset.c_str())[row];
            if (std::isnan(position)) {
                continue;
            }
            gross += std::fabs(position);
            const auto& factor_values =
                risk_factors.get_column<double>(asset.c_str());
            if (!std::isnan(factor_values[row])) {
                numerator += position * factor_values[row];
            }
        }
        if (gross != 0.0) {
            const double value = numerator / gross;
            exposure[row] = std::isfinite(value) ? value : 0.0;
        }
    }
    return make_frame(positions.get_index(), {{"exposure", exposure}});
}

namespace {

exposure_bundle compute_panel_exposures(
    const dataframe& positions, const dataframe& panel,
    const std::span<const sector_definition> definitions,
    const std::string& cash_name,
    const std::function<bool(const double, const sector_definition&)>& in_group) {
    require_same_wide_index(positions, panel);
    const auto assets = asset_column_names(positions, cash_name);
    const std::size_t rows = positions.get_index().size();
    auto total_long = positive_row_sums(positions, assets);
    auto total_short = negative_abs_row_sums(positions, assets);
    auto total_gross = abs_row_sums(positions, assets);
    for (auto& value : total_long) {
        if (value == 0.0) {
            value = std::nan("");
        }
    }
    for (auto& value : total_short) {
        if (value == 0.0) {
            value = std::nan("");
        }
    }
    for (auto& value : total_gross) {
        if (value == 0.0) {
            value = std::nan("");
        }
    }

    std::vector<output_column> long_columns;
    std::vector<output_column> short_columns;
    std::vector<output_column> gross_columns;
    for (const auto& definition : definitions) {
        std::vector<double> long_values(rows, 0.0);
        std::vector<double> short_values(rows, 0.0);
        std::vector<double> gross_values(rows, 0.0);
        for (const auto& asset : assets) {
            const auto& position_values =
                positions.get_column<double>(asset.c_str());
            const auto& panel_values =
                panel.get_column<double>(asset.c_str());
            for (std::size_t row = 0; row < rows; ++row) {
                const double position = position_values[row];
                const double panel_value = panel_values[row];
                if (std::isnan(position) || std::isnan(panel_value) ||
                    !in_group(panel_value, definition)) {
                    continue;
                }
                if (position > 0.0) {
                    long_values[row] += position;
                } else if (position < 0.0) {
                    short_values[row] += position;
                }
                gross_values[row] += std::fabs(position);
            }
        }
        long_columns.push_back(
            {definition.name, divide_or_zero(std::move(long_values),
                                             total_long)});
        short_columns.push_back(
            {definition.name, divide_or_zero(std::move(short_values),
                                             total_short)});
        gross_columns.push_back(
            {definition.name, divide_or_zero(std::move(gross_values),
                                             total_gross)});
    }
    std::vector<output_column> net_columns;
    for (std::size_t column = 0; column < definitions.size(); ++column) {
        std::vector<double> net(rows, 0.0);
        for (std::size_t row = 0; row < rows; ++row) {
            net[row] = long_columns[column].values[row] -
                       short_columns[column].values[row];
        }
        net_columns.push_back({definitions[column].name, std::move(net)});
    }
    return {make_frame(positions.get_index(), std::move(long_columns)),
            make_frame(positions.get_index(), std::move(short_columns)),
            make_frame(positions.get_index(), std::move(gross_columns)),
            make_frame(positions.get_index(), std::move(net_columns))};
}

}  // namespace

exposure_bundle compute_sector_exposures(
    const dataframe& positions, const dataframe& sectors,
    const std::span<const sector_definition> definitions,
    const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    return compute_panel_exposures(
        positions, sectors, definitions, cash_name,
        [](const double value, const sector_definition& definition) {
            return value == definition.id;
        });
}

exposure_bundle compute_cap_exposures(
    const dataframe& positions, const dataframe& caps,
    const std::string_view cash_column_name) {
    static const std::vector<sector_definition> buckets{
        {1.0, "Micro"}, {2.0, "Small"}, {3.0, "Mid"}, {4.0, "Large"},
        {5.0, "Mega"}};
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    return compute_panel_exposures(
        positions, caps, buckets, cash_name,
        [](const double value, const sector_definition& definition) {
            if (definition.name == "Micro") {
                return value >= 50'000'000.0 && value <= 300'000'000.0;
            }
            if (definition.name == "Small") {
                return value >= 300'000'000.0 && value <= 2'000'000'000.0;
            }
            if (definition.name == "Mid") {
                return value >= 2'000'000'000.0 && value <= 10'000'000'000.0;
            }
            if (definition.name == "Large") {
                return value >= 10'000'000'000.0 &&
                       value <= 200'000'000'000.0;
            }
            return value >= 200'000'000'000.0;
        });
}

exposure_bundle compute_volume_exposures(
    const dataframe& shares_held, const dataframe& volumes,
    const double percentile) {
    require_same_wide_index(shares_held, volumes);
    if (!(percentile >= 0.0 && percentile <= 1.0)) {
        throw input_error{"percentile must be in [0, 1]"};
    }
    const auto shares_columns = shares_held.get_columns_info<double>();
    std::vector<std::string> columns;
    for (const auto& info : shares_columns) {
        columns.emplace_back(std::get<0>(info).c_str());
    }
    const auto& index = shares_held.get_index();
    std::vector<std::string> volume_columns;
    for (const auto& info : volumes.get_columns_info<double>()) {
        volume_columns.emplace_back(std::get<0>(info).c_str());
    }
    std::vector<std::string> common;
    for (const auto& column : columns) {
        if (std::find(volume_columns.begin(), volume_columns.end(), column) !=
            volume_columns.end()) {
            common.push_back(column);
        }
    }
    for (const auto& column : columns) {
        const auto& share_values =
            shares_held.get_column<double>(column.c_str());
        const bool has_volume =
            std::find(volume_columns.begin(), volume_columns.end(), column) !=
            volume_columns.end();
        for (std::size_t row = 0; row < share_values.size(); ++row) {
            if (std::isnan(share_values[row]) || share_values[row] == 0.0) {
                continue;
            }
            if (!has_volume) {
                throw input_error{"active nonzero shares require matching finite positive volume observations"};
            }
            const double volume =
                volumes.get_column<double>(column.c_str())[row];
            if (std::isnan(volume) || !(volume > 0.0)) {
                throw input_error{"active nonzero shares require matching finite positive volume observations"};
            }
        }
    }

    std::vector<double> long_values(index.size(), 0.0);
    std::vector<double> short_values(index.size(), 0.0);
    std::vector<double> gross_values(index.size(), 0.0);
    for (std::size_t row = 0; row < index.size(); ++row) {
        std::vector<double> long_fraction;
        std::vector<double> short_fraction;
        std::vector<double> gross_fraction;
        for (const auto& column : common) {
            const double share =
                shares_held.get_column<double>(column.c_str())[row];
            const double volume =
                volumes.get_column<double>(column.c_str())[row];
            if (std::isnan(share) || share == 0.0 || std::isnan(volume) ||
                volume == 0.0) {
                continue;
            }
            const double fraction = share / volume;
            if (share > 0.0) {
                long_fraction.push_back(fraction);
            } else if (share < 0.0) {
                short_fraction.push_back(-fraction);
            }
            gross_fraction.push_back(std::fabs(fraction));
        }
        long_values[row] = 100.0 * linear_quantile(long_fraction, percentile);
        short_values[row] = 100.0 * linear_quantile(short_fraction, percentile);
        gross_values[row] = 100.0 * linear_quantile(gross_fraction, percentile);
        if (!std::isfinite(long_values[row])) {
            long_values[row] = 0.0;
        }
        if (!std::isfinite(short_values[row])) {
            short_values[row] = 0.0;
        }
        if (!std::isfinite(gross_values[row])) {
            gross_values[row] = 0.0;
        }
    }
    return {make_frame(index, {{"exposure", std::move(long_values)}}),
            make_frame(index, {{"exposure", std::move(short_values)}}),
            make_frame(index, {{"exposure", std::move(gross_values)}})};
}

}  // namespace fincore
