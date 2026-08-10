#include <fincore/dataframe.hpp>

#include <algorithm>
#include <ranges>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>

namespace fincore {
namespace {

std::string checked_column_name(std::string_view column_name) {
    if (column_name.empty()) {
        throw input_error{"column name must not be empty"};
    }
    return std::string{column_name};
}

}  // namespace

dataframe make_series(std::vector<timestamp> index,
                      std::string_view column_name,
                      std::vector<double> values) {
    const std::string name = checked_column_name(column_name);
    if (index.size() != values.size()) {
        throw input_error{"index and value vectors must have equal lengths"};
    }

    dataframe result;
    result.load_index(std::move(index));
    result.load_column<double>(name.c_str(), std::move(values),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

std::span<const double> require_double_column(const dataframe& frame,
                                              std::string_view column_name) {
    const std::string name = checked_column_name(column_name);
    if (!frame.has_column(name.c_str())) {
        throw input_error{"required double column is absent"};
    }

    const auto columns = frame.get_columns_info<double>();
    const auto column = std::ranges::find_if(
        columns, [&name](const auto& info) {
            return std::get<0>(info) == name.c_str();
        });
    if (column == columns.end() || std::get<2>(*column) != typeid(double)) {
        throw input_error{"required column is not a double column"};
    }

    const auto& values = frame.get_column<double>(name.c_str());
    if (values.size() != frame.get_index().size()) {
        throw input_error{
            "required double column must match the frame index length"};
    }
    return values;
}

std::pair<dataframe, dataframe> align_on_common_index(
    const dataframe& left,
    std::string_view left_column,
    const dataframe& right,
    std::string_view right_column) {
    const auto left_values = require_double_column(left, left_column);
    const auto right_values = require_double_column(right, right_column);
    const auto& left_index = left.get_index();
    const auto& right_index = right.get_index();

    std::unordered_map<timestamp, double> right_values_by_timestamp;
    right_values_by_timestamp.reserve(right_index.size());
    for (std::size_t position = 0; position < right_index.size(); ++position) {
        right_values_by_timestamp.emplace(right_index[position], right_values[position]);
    }

    std::vector<timestamp> common_index;
    std::vector<double> common_left_values;
    std::vector<double> common_right_values;
    common_index.reserve(left_index.size());
    common_left_values.reserve(left_index.size());
    common_right_values.reserve(left_index.size());

    for (std::size_t position = 0; position < left_index.size(); ++position) {
        const auto right_value = right_values_by_timestamp.find(left_index[position]);
        if (right_value == right_values_by_timestamp.end()) {
            continue;
        }
        common_index.push_back(left_index[position]);
        common_left_values.push_back(left_values[position]);
        common_right_values.push_back(right_value->second);
    }

    auto right_common_index = common_index;
    return {make_series(std::move(common_index), left_column,
                        std::move(common_left_values)),
            make_series(std::move(right_common_index), right_column,
                        std::move(common_right_values))};
}

}  // namespace fincore
