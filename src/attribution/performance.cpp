#include <fincore/attribution/performance.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <typeindex>
#include <vector>

namespace fincore {
namespace {

std::vector<std::string> double_columns(const dataframe& frame) {
    std::vector<std::string> names;
    for (const auto& info : frame.get_columns_info<double>()) {
        if (std::get<2>(info) == typeid(double)) {
            names.emplace_back(std::get<0>(info).c_str());
        }
    }
    return names;
}

dataframe filtered_frame(const dataframe& frame,
                         const std::set<timestamp>& dropped_dates,
                         const std::set<std::string>& dropped_columns = {}) {
    dataframe result;
    std::vector<timestamp> index;
    std::vector<std::size_t> positions;
    for (std::size_t row = 0; row < frame.get_index().size(); ++row) {
        if (dropped_dates.contains(frame.get_index()[row])) {
            continue;
        }
        index.push_back(frame.get_index()[row]);
        positions.push_back(row);
    }
    result.load_index(std::move(index));
    for (const auto& name : double_columns(frame)) {
        if (dropped_columns.contains(name)) {
            continue;
        }
        const auto& values = frame.get_column<double>(name.c_str());
        std::vector<double> selected;
        selected.reserve(positions.size());
        for (const std::size_t position : positions) {
            selected.push_back(values[position]);
        }
        result.load_column<double>(name.c_str(), std::move(selected),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

void require_unique_index(const dataframe& frame, const char* const name) {
    if (std::adjacent_find(frame.get_index().begin(), frame.get_index().end()) !=
        frame.get_index().end()) {
        throw input_error{std::string{name} + " index must be unique"};
    }
}

}  // namespace

attribution_alignment align_and_warn(
    const dataframe& returns, const dataframe& positions,
    const dataframe& factor_returns,
    const factor_loading_table& factor_loadings) {
    require_unique_index(returns, "returns");
    require_unique_index(positions, "positions");
    require_unique_index(factor_returns, "factor_returns");
    if (positions.get_index().empty()) {
        throw input_error{"positions must contain at least one row"};
    }
    if (factor_loadings.values.size() !=
        factor_loadings.dates.size() * factor_loadings.assets.size() *
            factor_loadings.factors.size()) {
        throw input_error{
            "factor_loadings values must match dates x assets x factors"};
    }
    const auto position_columns = double_columns(positions);
    std::size_t stock_count = 0U;
    for (const auto& name : position_columns) {
        if (name != "cash") {
            ++stock_count;
        }
    }
    std::set<std::string> loading_assets(factor_loadings.assets.begin(),
                                         factor_loadings.assets.end());
    std::vector<std::string> missing_assets;
    for (const auto& name : position_columns) {
        if (name != "cash" && !loading_assets.contains(name)) {
            missing_assets.push_back(name);
        }
    }
    if (stock_count == 0U || missing_assets.size() == stock_count) {
        throw input_error{
            "no factor loadings were available for the algorithm's positions"};
    }

    attribution_alignment result;
    const std::set<std::string> dropped(missing_assets.begin(),
                                        missing_assets.end());
    result.positions = filtered_frame(positions, {}, dropped);
    std::set<timestamp> loading_dates(factor_loadings.dates.begin(),
                                      factor_loadings.dates.end());
    std::set<timestamp> missing_dates;
    for (const timestamp date : positions.get_index()) {
        if (!loading_dates.contains(date)) {
            missing_dates.insert(date);
        }
    }
    result.returns = filtered_frame(returns, missing_dates);
    result.positions = filtered_frame(result.positions, missing_dates);
    result.factor_returns = filtered_frame(factor_returns, missing_dates);
    result.factor_loadings = factor_loadings;

    if (!missing_assets.empty()) {
        std::ostringstream message;
        message << "could not determine risk exposures for "
                << missing_assets.size() << " of " << stock_count
                << " assets (ratio "
                << std::fixed << std::setprecision(3)
                << static_cast<double>(missing_assets.size()) /
                       static_cast<double>(stock_count)
                << "); ignoring them for attribution: ";
        for (std::size_t index = 0; index < missing_assets.size(); ++index) {
            if (index > 0U) {
                message << ", ";
            }
            message << missing_assets[index];
        }
        result.warnings.push_back(message.str());
    }
    if (!missing_dates.empty()) {
        std::ostringstream message;
        message << "could not find factor loadings for "
                << missing_dates.size() << " dates; truncating attribution: ";
        bool first = true;
        for (const timestamp date : missing_dates) {
            if (!first) {
                message << ", ";
            }
            first = false;
            message << detail::format_timestamp(date);
        }
        result.warnings.push_back(message.str());
    }
    return result;
}

}  // namespace fincore
