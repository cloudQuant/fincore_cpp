#include <fincore/runtime/operations_factor.hpp>

#include <fincore/attribution/brinson.hpp>
#include <fincore/attribution/fama_french.hpp>
#include <fincore/attribution/performance.hpp>
#include <fincore/attribution/style.hpp>
#include <fincore/data/snapshots.hpp>
#include <fincore/error.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/calendar.hpp>
#include <fincore/factor/costs.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/factor/pit.hpp>
#include <fincore/factor/portfolio.hpp>
#include <fincore/factor/report.hpp>
#include <fincore/factor/timedelta.hpp>
#include <fincore/metrics/perf_attrib.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/optimization/frontier.hpp>
#include <fincore/optimization/objectives.hpp>
#include <fincore/optimization/risk_parity.hpp>
#include <fincore/report/models.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace fincore {
namespace {

// ---------------------------------------------------------------------------
// Runtime input helpers. Values are read with the same contracts as the
// builtin catalog: typed lookups fail with input_error instead of coercing.
// ---------------------------------------------------------------------------

const runtime_value& require_value(const runtime_inputs& inputs,
                                   const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    return found->second;
}

const runtime_value* optional_value(const runtime_inputs& inputs,
                                    const std::string& name) {
    const auto found = inputs.find(name);
    return found == inputs.end() ? nullptr : &found->second;
}

double require_number(const runtime_inputs& inputs, const std::string& name) {
    const auto* value = std::get_if<double>(&require_value(inputs, name).data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a number: " + name};
    }
    return *value;
}

std::optional<double> optional_number(const runtime_inputs& inputs,
                                      const std::string& name) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* number = std::get_if<double>(&value->data);
    if (number == nullptr) {
        throw input_error{"runtime input must be a number: " + name};
    }
    return *number;
}

std::int64_t require_integer(const runtime_inputs& inputs,
                             const std::string& name) {
    const auto* value =
        std::get_if<std::int64_t>(&require_value(inputs, name).data);
    if (value == nullptr) {
        throw input_error{"runtime input must be an integer: " + name};
    }
    return *value;
}

std::optional<std::int64_t> optional_integer(const runtime_inputs& inputs,
                                             const std::string& name) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* integer = std::get_if<std::int64_t>(&value->data);
    if (integer == nullptr) {
        throw input_error{"runtime input must be an integer: " + name};
    }
    return *integer;
}

std::size_t optional_size(const runtime_inputs& inputs,
                          const std::string& name,
                          const std::size_t fallback) {
    const auto value = optional_integer(inputs, name);
    if (!value.has_value()) {
        return fallback;
    }
    if (*value < 0) {
        throw input_error{"runtime input must be non-negative: " + name};
    }
    return static_cast<std::size_t>(*value);
}

int require_int(const runtime_inputs& inputs, const std::string& name) {
    const auto value = require_integer(inputs, name);
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        throw input_error{"runtime input is out of int range: " + name};
    }
    return static_cast<int>(value);
}

int optional_int(const runtime_inputs& inputs, const std::string& name,
                 const int fallback) {
    const auto value = optional_integer(inputs, name);
    if (!value.has_value()) {
        return fallback;
    }
    if (*value < std::numeric_limits<int>::min() ||
        *value > std::numeric_limits<int>::max()) {
        throw input_error{"runtime input is out of int range: " + name};
    }
    return static_cast<int>(*value);
}

bool optional_bool(const runtime_inputs& inputs, const std::string& name,
                   const bool fallback) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return fallback;
    }
    const auto* flag = std::get_if<bool>(&value->data);
    if (flag == nullptr) {
        throw input_error{"runtime input must be a boolean: " + name};
    }
    return *flag;
}

std::string require_string(const runtime_inputs& inputs,
                           const std::string& name) {
    const auto* value =
        std::get_if<std::string>(&require_value(inputs, name).data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a string: " + name};
    }
    return *value;
}

std::string optional_string(const runtime_inputs& inputs,
                            const std::string& name,
                            const std::string& fallback) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return fallback;
    }
    const auto* text = std::get_if<std::string>(&value->data);
    if (text == nullptr) {
        throw input_error{"runtime input must be a string: " + name};
    }
    return *text;
}

const dataframe& require_frame(const runtime_inputs& inputs,
                               const std::string& name) {
    const auto* value = std::get_if<dataframe>(&require_value(inputs, name).data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a dataframe: " + name};
    }
    return *value;
}

const dataframe* optional_frame(const runtime_inputs& inputs,
                                const std::string& name) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return nullptr;
    }
    const auto* frame = std::get_if<dataframe>(&value->data);
    if (frame == nullptr) {
        throw input_error{"runtime input must be a dataframe: " + name};
    }
    return frame;
}

// ---------------------------------------------------------------------------
// DataFrame type helpers.
// ---------------------------------------------------------------------------

bool has_column_of_type(const dataframe& frame, const std::string& name,
                        const std::type_index type) {
    const auto columns = frame.get_columns_info<
        bool, char, signed char, unsigned char, short, unsigned short, int,
        unsigned int, long, unsigned long, long long, unsigned long long,
        float, double, std::string>();
    for (const auto& info : columns) {
        if (std::string{std::get<0>(info).c_str()} == name) {
            return std::get<2>(info) == type;
        }
    }
    return false;
}

const std::vector<std::string>&
require_string_column(const dataframe& frame, const std::string& name) {
    if (!frame.has_column(name.c_str())) {
        throw input_error{"required string column is absent: " + name};
    }
    if (!has_column_of_type(frame, name, std::type_index(typeid(std::string)))) {
        throw input_error{"required column must be a string column: " + name};
    }
    const auto& values = frame.get_column<std::string>(name.c_str());
    if (values.size() != frame.get_index().size()) {
        throw input_error{"string column must match the frame index length: " +
                          name};
    }
    return values;
}

std::vector<std::int64_t>
int64_values(const dataframe& frame, const std::string& name) {
    if (!frame.has_column(name.c_str()) ||
        !has_column_of_type(frame, name,
                            std::type_index(typeid(std::int64_t)))) {
        throw input_error{"required int64 column is absent: " + name};
    }
    const auto& values = frame.get_column<std::int64_t>(name.c_str());
    if (values.size() != frame.get_index().size()) {
        throw input_error{"int64 column must match the frame index length: " +
                          name};
    }
    return {values.begin(), values.end()};
}

std::vector<int> int_values(const dataframe& frame, const std::string& name) {
    if (!frame.has_column(name.c_str())) {
        throw input_error{"required integer column is absent: " + name};
    }
    if (has_column_of_type(frame, name, std::type_index(typeid(int)))) {
        const auto& values = frame.get_column<int>(name.c_str());
        if (values.size() != frame.get_index().size()) {
            throw input_error{
                "integer column must match the frame index length: " + name};
        }
        return {values.begin(), values.end()};
    }
    const auto wide = int64_values(frame, name);
    std::vector<int> values;
    values.reserve(wide.size());
    for (const auto value : wide) {
        if (value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max()) {
            throw input_error{"integer column is out of int range: " + name};
        }
        values.push_back(static_cast<int>(value));
    }
    return values;
}

std::vector<timestamp> frame_dates(const dataframe& frame) {
    if (frame.has_column("date")) {
        const auto columns = frame.get_columns_info<std::int64_t>();
        for (const auto& info : columns) {
            if (std::string{std::get<0>(info).c_str()} == "date") {
                const auto& values =
                    frame.get_column<std::int64_t>("date");
                if (values.size() != frame.get_index().size()) {
                    throw input_error{
                        "date column must match the frame index length"};
                }
                return {values.begin(), values.end()};
            }
        }
    }
    const auto& index = frame.get_index();
    return {index.begin(), index.end()};
}

std::string first_double_column(const dataframe& frame,
                                const std::string& excluded = {}) {
    for (const auto& info : frame.get_columns_info<double>()) {
        const std::string name{std::get<0>(info).c_str()};
        if (name != excluded) {
            return name;
        }
    }
    return {};
}

std::vector<double> double_values(const dataframe& frame,
                                  const std::string& name) {
    const auto& values = require_double_column(frame, name);
    return {values.begin(), values.end()};
}

std::vector<double> first_double_values(const dataframe& frame,
                                        const std::string& name) {
    const std::string column = first_double_column(frame);
    if (column.empty()) {
        throw input_error{"runtime dataframe input must have a double column: " +
                          name};
    }
    return double_values(frame, column);
}

dataframe make_frame(std::vector<timestamp> index) {
    dataframe frame;
    frame.load_index(std::move(index));
    return frame;
}

std::vector<timestamp> ordinal_index(const std::size_t size) {
    std::vector<timestamp> index;
    index.reserve(size);
    for (std::size_t position = 0U; position < size; ++position) {
        index.push_back(static_cast<timestamp>(position));
    }
    return index;
}

void add_double_column(dataframe& frame, const std::string& name,
                       std::vector<double> values) {
    frame.load_column<double>(name.c_str(), std::move(values),
                              hmdf::nan_policy::dont_pad_with_nans);
}

void add_int_column(dataframe& frame, const std::string& name,
                    std::vector<int> values) {
    frame.load_column<int>(name.c_str(), std::move(values),
                           hmdf::nan_policy::dont_pad_with_nans);
}

void add_int64_column(dataframe& frame, const std::string& name,
                      std::vector<std::int64_t> values) {
    frame.load_column<std::int64_t>(name.c_str(), std::move(values),
                                    hmdf::nan_policy::dont_pad_with_nans);
}

void add_bool_column(dataframe& frame, const std::string& name,
                     std::vector<bool> values) {
    frame.load_column<bool>(name.c_str(), std::move(values),
                            hmdf::nan_policy::dont_pad_with_nans);
}

void add_string_column(dataframe& frame, const std::string& name,
                       std::vector<std::string> values) {
    frame.load_column<std::string>(name.c_str(), std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
}

dataframe string_list_frame(const std::vector<std::string>& values,
                            const std::string& column = "value") {
    auto frame = make_frame(ordinal_index(values.size()));
    add_string_column(frame, column, values);
    return frame;
}

dataframe int_list_frame(const std::vector<int>& values,
                         const std::string& column = "value") {
    auto frame = make_frame(ordinal_index(values.size()));
    add_int_column(frame, column, values);
    return frame;
}

dataframe timestamp_list_frame(const std::vector<timestamp>& values,
                               const std::string& column = "value") {
    auto frame = make_frame(ordinal_index(values.size()));
    add_int64_column(frame, column, values);
    return frame;
}

// ---------------------------------------------------------------------------
// Typed list inputs. Native callers pass lists as single-column dataframes;
// scalar variants are accepted where the source accepts a conformable object.
// ---------------------------------------------------------------------------

std::vector<std::string>
parse_string_list(const runtime_inputs& inputs, const std::string& name,
                  const bool required) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        if (required) {
            throw input_error{"missing runtime input: " + name};
        }
        return {};
    }
    if (const auto* text = std::get_if<std::string>(&value->data)) {
        return {*text};
    }
    const auto* frame = std::get_if<dataframe>(&value->data);
    if (frame == nullptr) {
        throw input_error{
            "runtime input must be a string or a single-column string "
            "dataframe: " +
            name};
    }
    if (frame->has_column("value") &&
        has_column_of_type(*frame, "value",
                           std::type_index(typeid(std::string)))) {
        const auto& column = require_string_column(*frame, "value");
        return {column.begin(), column.end()};
    }
    const auto columns = frame->get_columns_info<std::string>();
    if (columns.empty()) {
        throw input_error{"string-list dataframe has no string column: " +
                          name};
    }
    const std::string column{std::get<0>(columns.front()).c_str()};
    const auto& values = require_string_column(*frame, column);
    return {values.begin(), values.end()};
}

std::vector<double>
parse_double_list(const runtime_inputs& inputs, const std::string& name,
                  const std::vector<double>& fallback = {}) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return fallback;
    }
    if (const auto* number = std::get_if<double>(&value->data)) {
        return {*number};
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value->data)) {
        return {static_cast<double>(*integer)};
    }
    const auto* frame = std::get_if<dataframe>(&value->data);
    if (frame == nullptr) {
        throw input_error{
            "runtime input must be a number or a single-column double "
            "dataframe: " +
            name};
    }
    return first_double_values(*frame, name);
}

std::vector<int> parse_int_list(const runtime_inputs& inputs,
                                const std::string& name,
                                const std::vector<int>& fallback = {}) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        return fallback;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value->data)) {
        if (*integer < std::numeric_limits<int>::min() ||
            *integer > std::numeric_limits<int>::max()) {
            throw input_error{"runtime input is out of int range: " + name};
        }
        return {static_cast<int>(*integer)};
    }
    const auto* frame = std::get_if<dataframe>(&value->data);
    if (frame == nullptr) {
        throw input_error{
            "runtime input must be an integer or a single-column integer "
            "dataframe: " +
            name};
    }
    if (frame->has_column("value") && frame->get_index().size() > 0U) {
        const auto columns = frame->get_columns_info<int, std::int64_t>();
        for (const auto& info : columns) {
            if (std::string{std::get<0>(info).c_str()} == "value") {
                return int_values(*frame, "value");
            }
        }
    }
    const auto columns = frame->get_columns_info<int, std::int64_t>();
    if (columns.empty()) {
        throw input_error{"integer-list dataframe has no integer column: " +
                          name};
    }
    const std::string column{std::get<0>(columns.front()).c_str()};
    return int_values(*frame, column);
}

std::vector<timestamp>
parse_timestamp_list(const runtime_inputs& inputs, const std::string& name,
                     const bool required) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        if (required) {
            throw input_error{"missing runtime input: " + name};
        }
        return {};
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value->data)) {
        return {*integer};
    }
    const auto* frame = std::get_if<dataframe>(&value->data);
    if (frame == nullptr) {
        throw input_error{
            "runtime input must be an integer timestamp or an int64 "
            "dataframe: " +
            name};
    }
    if (frame->has_column("date") &&
        has_column_of_type(*frame, "date",
                           std::type_index(typeid(std::int64_t)))) {
        return int64_values(*frame, "date");
    }
    if (frame->has_column("value") &&
        has_column_of_type(*frame, "value",
                           std::type_index(typeid(std::int64_t)))) {
        return int64_values(*frame, "value");
    }
    const auto columns = frame->get_columns_info<std::int64_t>();
    if (columns.empty()) {
        throw input_error{"timestamp-list dataframe has no int64 column: " +
                          name};
    }
    const std::string column{std::get<0>(columns.front()).c_str()};
    return int64_values(*frame, column);
}

std::int64_t parse_timedelta(const runtime_inputs& inputs,
                             const std::string& name) {
    const auto* value = optional_value(inputs, name);
    if (value == nullptr) {
        throw input_error{"missing runtime input: " + name};
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value->data)) {
        return *integer;
    }
    if (const auto* text = std::get_if<std::string>(&value->data)) {
        return parse_timedelta_nanoseconds(*text);
    }
    throw input_error{
        "runtime input must be an integer nanosecond duration or a duration "
        "string: " +
        name};
}

trading_calendar calendar_from_inputs(const runtime_inputs& inputs) {
    const auto* factor = optional_value(inputs, "factor_dates");
    const auto* prices = optional_value(inputs, "price_dates");
    if (factor != nullptr && prices != nullptr) {
        return infer_trading_calendar(
            parse_timestamp_list(inputs, "factor_dates", true),
            parse_timestamp_list(inputs, "price_dates", true));
    }
    if (factor != nullptr || prices != nullptr) {
        throw input_error{
            "factor_dates and price_dates must be supplied together"};
    }
    return business_day_calendar();
}

// ---------------------------------------------------------------------------
// Native record parsers.
// ---------------------------------------------------------------------------

factor_series parse_factor_series(const runtime_inputs& inputs,
                                  const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    factor_series series;
    series.dates = frame_dates(frame);
    const auto& assets = require_string_column(frame, "asset");
    const std::string value_column =
        frame.has_column("value") ? "value" : "factor";
    series.values = double_values(frame, value_column);
    if (assets.size() != series.dates.size() ||
        series.values.size() != series.dates.size()) {
        throw input_error{"factor series columns must match the row count: " +
                          name};
    }
    series.assets.assign(assets.begin(), assets.end());
    return series;
}

std::vector<factor_weight_row>
parse_weight_rows(const runtime_inputs& inputs, const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    const auto dates = frame_dates(frame);
    const auto& assets = require_string_column(frame, "asset");
    const std::string weight_column =
        frame.has_column("weight") ? "weight" : "factor";
    const auto weights = double_values(frame, weight_column);
    if (assets.size() != dates.size() || weights.size() != dates.size()) {
        throw input_error{"weight rows must match the date count: " + name};
    }
    std::vector<factor_weight_row> rows;
    rows.reserve(dates.size());
    for (std::size_t row = 0U; row < dates.size(); ++row) {
        rows.push_back({dates[row], assets[row], weights[row]});
    }
    return rows;
}

factor_grouping parse_grouping(const runtime_inputs& inputs) {
    factor_grouping grouping;
    const auto* groupby = optional_frame(inputs, "groupby");
    if (groupby != nullptr) {
        const auto& assets = require_string_column(*groupby, "asset");
        const auto& groups = require_string_column(*groupby, "group");
        if (assets.size() != groups.size()) {
            throw input_error{"groupby asset and group columns must align"};
        }
        for (std::size_t row = 0U; row < assets.size(); ++row) {
            grouping.group_by_asset.emplace(assets[row], groups[row]);
        }
    }
    const auto* labels = optional_frame(inputs, "groupby_labels");
    if (labels != nullptr) {
        const auto& groups = require_string_column(*labels, "group");
        const auto& values = require_string_column(*labels, "label");
        if (groups.size() != values.size()) {
            throw input_error{
                "groupby_labels group and label columns must align"};
        }
        for (std::size_t row = 0U; row < groups.size(); ++row) {
            grouping.group_labels.emplace(groups[row], values[row]);
        }
    }
    return grouping;
}

prepared_factor_data parse_prepared_frame(const dataframe& frame,
                                          const std::string& default_label) {
    prepared_factor_data data;
    const auto dates = frame_dates(frame);
    const auto& assets = require_string_column(frame, "asset");
    const std::string factor_column =
        frame.has_column("factor") ? "factor" : "value";
    const auto factors = double_values(frame, factor_column);
    if (dates.size() != frame.get_index().size() ||
        assets.size() != dates.size() || factors.size() != dates.size()) {
        throw input_error{
            "factor data columns must match the frame index length"};
    }

    std::vector<int> quantiles(dates.size(), 0);
    if (frame.has_column("quantile")) {
        quantiles = int_values(frame, "quantile");
    }
    std::vector<std::string> groups(dates.size());
    const bool has_groups = frame.has_column("group");
    if (has_groups) {
        const auto& column = require_string_column(frame, "group");
        groups.assign(column.begin(), column.end());
    }

    data.forward_labels.clear();
    std::vector<std::string> labels;
    for (const auto& info : frame.get_columns_info<double>()) {
        const std::string name{std::get<0>(info).c_str()};
        if (name != factor_column && name.rfind("loss_", 0U) != 0U) {
            labels.push_back(name);
        }
    }
    if (labels.empty() && !default_label.empty()) {
        labels.push_back(default_label);
    }
    data.forward_labels = labels;

    data.rows.reserve(dates.size());
    for (std::size_t row = 0U; row < dates.size(); ++row) {
        factor_row value;
        value.date = dates[row];
        value.asset = assets[row];
        value.factor = factors[row];
        value.quantile = quantiles[row];
        value.has_group = has_groups && !groups[row].empty();
        value.group = has_groups ? groups[row] : std::string{};
        value.forward_returns.reserve(labels.size());
        for (const auto& label : labels) {
            if (label == default_label && !frame.has_column(label.c_str())) {
                value.forward_returns.push_back(0.0);
            } else {
                const auto& column = require_double_column(frame, label);
                value.forward_returns.push_back(column[row]);
            }
        }
        data.rows.push_back(std::move(value));
    }
    return data;
}

prepared_factor_data parse_prepared(const runtime_inputs& inputs,
                                    const std::string& name,
                                    const std::string& default_label) {
    auto data = parse_prepared_frame(require_frame(inputs, name),
                                     default_label);
    const auto labels = parse_string_list(inputs, "forward_labels", false);
    if (!labels.empty()) {
        std::vector<std::size_t> positions;
        positions.reserve(labels.size());
        for (const auto& label : labels) {
            const auto found = std::find(data.forward_labels.begin(),
                                         data.forward_labels.end(), label);
            if (found == data.forward_labels.end()) {
                throw input_error{"forward label is absent from factor data: " +
                                  label};
            }
            positions.push_back(static_cast<std::size_t>(
                std::distance(data.forward_labels.begin(), found)));
        }
        for (auto& row : data.rows) {
            std::vector<double> selected;
            selected.reserve(positions.size());
            for (const auto position : positions) {
                selected.push_back(row.forward_returns[position]);
            }
            row.forward_returns = std::move(selected);
        }
        data.forward_labels = labels;
    }
    return data;
}

factor_ic_table parse_ic_table(const runtime_inputs& inputs,
                               const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    const auto dates = int64_values(frame, "date");
    const auto& labels = require_string_column(frame, "label");
    const auto values = double_values(frame, "value");
    const bool has_groups = frame.has_column("group");
    std::vector<std::string> groups(dates.size());
    if (has_groups) {
        const auto& column = require_string_column(frame, "group");
        groups.assign(column.begin(), column.end());
    }
    if (labels.size() != dates.size() || values.size() != dates.size()) {
        throw input_error{"IC table columns must align: " + name};
    }

    factor_ic_table table;
    std::map<std::string, std::size_t> label_positions;
    std::map<std::pair<timestamp, std::string>, std::size_t> row_positions;
    for (std::size_t row = 0U; row < dates.size(); ++row) {
        if (!label_positions.contains(labels[row])) {
            label_positions.emplace(labels[row], table.labels.size());
            table.labels.push_back(labels[row]);
        }
        const auto key = std::make_pair(dates[row], groups[row]);
        auto found = row_positions.find(key);
        if (found == row_positions.end()) {
            found = row_positions.emplace(key, table.dates.size()).first;
            table.dates.push_back(dates[row]);
            if (has_groups) {
                table.groups.push_back(groups[row]);
            }
        }
    }
    table.values.assign(table.dates.size() * table.labels.size(),
                        std::numeric_limits<double>::quiet_NaN());
    for (std::size_t row = 0U; row < dates.size(); ++row) {
        const auto row_index =
            row_positions.at(std::make_pair(dates[row], groups[row]));
        table.values[row_index * table.labels.size() +
                     label_positions.at(labels[row])] = values[row];
    }
    return table;
}

quantile_return_table parse_quantile_table(const runtime_inputs& inputs,
                                           const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    const auto quantiles = int_values(frame, "quantile");
    const auto& labels = require_string_column(frame, "label");
    const auto means = double_values(frame, "mean");
    std::vector<double> standard_errors(means.size(),
                                        std::numeric_limits<double>::quiet_NaN());
    if (frame.has_column("standard_error")) {
        standard_errors = double_values(frame, "standard_error");
    }
    const bool has_groups = frame.has_column("group");
    std::vector<std::string> groups(quantiles.size());
    if (has_groups) {
        const auto& column = require_string_column(frame, "group");
        groups.assign(column.begin(), column.end());
    }
    if (labels.size() != quantiles.size() || means.size() != quantiles.size() ||
        standard_errors.size() != quantiles.size()) {
        throw input_error{"quantile table columns must align: " + name};
    }

    quantile_return_table table;
    std::map<std::string, std::size_t> label_positions;
    std::map<std::pair<int, std::string>, std::size_t> row_positions;
    for (std::size_t row = 0U; row < quantiles.size(); ++row) {
        if (!label_positions.contains(labels[row])) {
            label_positions.emplace(labels[row], table.labels.size());
            table.labels.push_back(labels[row]);
        }
        const auto key = std::make_pair(quantiles[row], groups[row]);
        auto found = row_positions.find(key);
        if (found == row_positions.end()) {
            found = row_positions.emplace(key, table.quantiles.size()).first;
            table.quantiles.push_back(quantiles[row]);
            if (has_groups) {
                table.groups.push_back(groups[row]);
            }
        }
    }
    const std::size_t cells = table.quantiles.size() * table.labels.size();
    table.means.assign(cells, std::numeric_limits<double>::quiet_NaN());
    table.standard_errors.assign(cells,
                                 std::numeric_limits<double>::quiet_NaN());
    for (std::size_t row = 0U; row < quantiles.size(); ++row) {
        const auto row_index =
            row_positions.at(std::make_pair(quantiles[row], groups[row]));
        const std::size_t cell =
            row_index * table.labels.size() + label_positions.at(labels[row]);
        table.means[cell] = means[row];
        table.standard_errors[cell] = standard_errors[row];
    }
    return table;
}

void apply_standard_errors(const runtime_inputs& inputs,
                           quantile_return_table& table) {
    const auto* frame = optional_frame(inputs, "std_err");
    if (frame == nullptr) {
        return;
    }
    const auto quantiles = int_values(*frame, "quantile");
    const auto& labels = require_string_column(*frame, "label");
    const auto values = double_values(*frame, "mean");
    const bool has_groups = frame->has_column("group");
    std::vector<std::string> groups(quantiles.size());
    if (has_groups) {
        const auto& column = require_string_column(*frame, "group");
        groups.assign(column.begin(), column.end());
    }
    std::map<std::string, std::size_t> label_positions;
    for (std::size_t position = 0U; position < table.labels.size();
         ++position) {
        label_positions.emplace(table.labels[position], position);
    }
    std::map<std::pair<int, std::string>, std::size_t> row_positions;
    for (std::size_t position = 0U; position < table.quantiles.size();
         ++position) {
        row_positions.emplace(std::make_pair(table.quantiles[position],
                                             table.groups.empty()
                                                 ? std::string{}
                                                 : table.groups[position]),
                              position);
    }
    for (std::size_t row = 0U; row < quantiles.size(); ++row) {
        const auto label = label_positions.find(labels[row]);
        const auto quantile = row_positions.find(
            std::make_pair(quantiles[row], groups[row]));
        if (label == label_positions.end() ||
            quantile == row_positions.end()) {
            throw input_error{"std_err rows must match mean_returns rows"};
        }
        table.standard_errors[quantile->second * table.labels.size() +
                              label->second] = values[row];
    }
}

forward_return_table parse_forward_table(const runtime_inputs& inputs,
                                         const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    forward_return_table table;
    table.dates = frame_dates(frame);
    const auto& assets = require_string_column(frame, "asset");
    if (assets.size() != table.dates.size()) {
        throw input_error{"forward-return asset column must match rows: " +
                          name};
    }
    table.assets.assign(assets.begin(), assets.end());
    for (const auto& info : frame.get_columns_info<double>()) {
        const std::string label{std::get<0>(info).c_str()};
        table.labels.push_back(label);
        const auto& values = require_double_column(frame, label);
        table.values.emplace_back(values.begin(), values.end());
    }
    return table;
}

std::vector<pit_point> parse_pit_points(const runtime_inputs& inputs,
                                        const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    const auto as_of = int64_values(frame, "as_of");
    const auto known_at = int64_values(frame, "known_at");
    const auto effective_from = int64_values(frame, "effective_from");
    const auto values = double_values(frame, "value");
    if (as_of.size() != known_at.size() ||
        as_of.size() != effective_from.size() ||
        as_of.size() != values.size()) {
        throw input_error{"PIT point columns must align: " + name};
    }
    std::vector<pit_point> points;
    points.reserve(as_of.size());
    for (std::size_t row = 0U; row < as_of.size(); ++row) {
        pit_point point;
        point.as_of = as_of[row];
        point.known_at = known_at[row];
        point.effective_from = effective_from[row];
        point.value = values[row];
        points.push_back(point);
    }
    return points;
}

std::vector<pit_observation>
parse_pit_observations(const runtime_inputs& inputs,
                       const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    const auto& assets = require_string_column(frame, "asset");
    const auto as_of = int64_values(frame, "as_of");
    const auto known_at = int64_values(frame, "known_at");
    const auto effective_from = int64_values(frame, "effective_from");
    const auto values = double_values(frame, "value");
    std::vector<bool> in_universe(assets.size(), true);
    if (frame.has_column("in_universe")) {
        if (!has_column_of_type(frame, "in_universe",
                                std::type_index(typeid(bool)))) {
            throw input_error{"in_universe must be a boolean column"};
        }
        const auto& column = frame.get_column<bool>("in_universe");
        in_universe.assign(column.begin(), column.end());
    }
    if (as_of.size() != assets.size() || known_at.size() != assets.size() ||
        effective_from.size() != assets.size() ||
        values.size() != assets.size() ||
        in_universe.size() != assets.size()) {
        throw input_error{"PIT observation columns must align: " + name};
    }
    std::vector<pit_observation> observations;
    observations.reserve(assets.size());
    for (std::size_t row = 0U; row < assets.size(); ++row) {
        pit_point point;
        point.as_of = as_of[row];
        point.known_at = known_at[row];
        point.effective_from = effective_from[row];
        point.value = values[row];
        observations.push_back({assets[row], point, in_universe[row]});
    }
    return observations;
}

// ---------------------------------------------------------------------------
// DataFrame projections for native record types.
// ---------------------------------------------------------------------------

dataframe series_frame(const factor_series_result& result) {
    auto frame = make_frame(result.dates);
    add_double_column(frame, "value", result.values);
    return frame;
}

dataframe factor_series_frame(const factor_series& series) {
    if (series.assets.empty()) {
        return series_frame({series.dates, series.values});
    }
    auto frame = make_frame(ordinal_index(series.dates.size()));
    add_int64_column(frame, "date", series.dates);
    add_string_column(frame, "asset", series.assets);
    add_double_column(frame, "value", series.values);
    return frame;
}

dataframe weights_frame(const std::vector<factor_weight_row>& rows) {
    auto frame = make_frame(ordinal_index(rows.size()));
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<double> weights;
    dates.reserve(rows.size());
    assets.reserve(rows.size());
    weights.reserve(rows.size());
    for (const auto& row : rows) {
        dates.push_back(row.date);
        assets.push_back(row.asset);
        weights.push_back(row.weight);
    }
    add_int64_column(frame, "date", std::move(dates));
    add_string_column(frame, "asset", std::move(assets));
    add_double_column(frame, "weight", std::move(weights));
    return frame;
}

dataframe prepared_frame(const prepared_factor_data& data,
                         const std::string& horizon = {}) {
    auto frame = make_frame(ordinal_index(data.rows.size()));
    if (!horizon.empty()) {
        add_string_column(frame, "horizon",
                          std::vector<std::string>(data.rows.size(), horizon));
    }
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<int> quantiles;
    std::vector<std::string> groups;
    std::vector<double> factors;
    std::vector<std::vector<double>> forwards(data.forward_labels.size());
    bool has_groups = false;
    dates.reserve(data.rows.size());
    assets.reserve(data.rows.size());
    quantiles.reserve(data.rows.size());
    groups.reserve(data.rows.size());
    factors.reserve(data.rows.size());
    for (const auto& row : data.rows) {
        dates.push_back(row.date);
        assets.push_back(row.asset);
        quantiles.push_back(row.quantile);
        groups.push_back(row.group);
        factors.push_back(row.factor);
        has_groups = has_groups || row.has_group;
        for (std::size_t label = 0U; label < data.forward_labels.size();
             ++label) {
            forwards[label].push_back(label < row.forward_returns.size()
                                          ? row.forward_returns[label]
                                          : std::numeric_limits<double>::quiet_NaN());
        }
    }
    add_int64_column(frame, "date", std::move(dates));
    add_string_column(frame, "asset", std::move(assets));
    add_int_column(frame, "quantile", std::move(quantiles));
    if (has_groups) {
        add_string_column(frame, "group", std::move(groups));
    }
    add_double_column(frame, "factor", std::move(factors));
    for (std::size_t label = 0U; label < data.forward_labels.size(); ++label) {
        add_double_column(frame, data.forward_labels[label],
                          std::move(forwards[label]));
    }
    add_int64_column(frame, "loss_input_count",
                     std::vector<std::int64_t>(
                         data.rows.size(),
                         static_cast<std::int64_t>(data.loss_report.input_count)));
    add_int64_column(
        frame, "loss_finite_factor_count",
        std::vector<std::int64_t>(
            data.rows.size(),
            static_cast<std::int64_t>(data.loss_report.finite_factor_count)));
    add_int64_column(
        frame, "loss_forward_returns_count",
        std::vector<std::int64_t>(
            data.rows.size(),
            static_cast<std::int64_t>(data.loss_report.forward_returns_count)));
    add_int64_column(
        frame, "loss_binning_count",
        std::vector<std::int64_t>(
            data.rows.size(),
            static_cast<std::int64_t>(data.loss_report.binning_count)));
    add_double_column(
        frame, "loss_factor_input_loss",
        std::vector<double>(data.rows.size(),
                            data.loss_report.factor_input_loss));
    add_double_column(
        frame, "loss_forward_returns_loss",
        std::vector<double>(data.rows.size(),
                            data.loss_report.forward_returns_loss));
    add_double_column(frame, "loss_binning_loss",
                      std::vector<double>(data.rows.size(),
                                          data.loss_report.binning_loss));
    add_double_column(frame, "loss_total_loss",
                      std::vector<double>(data.rows.size(),
                                          data.loss_report.total_loss));
    return frame;
}

dataframe forward_table_frame(const forward_return_table& table) {
    auto frame = make_frame(table.dates);
    add_string_column(frame, "asset", table.assets);
    for (std::size_t label = 0U; label < table.labels.size(); ++label) {
        add_double_column(frame, table.labels[label], table.values[label]);
    }
    return frame;
}

dataframe ic_table_frame(const factor_ic_table& table) {
    auto frame = make_frame(ordinal_index(table.dates.size()));
    std::vector<timestamp> dates;
    std::vector<std::string> groups;
    std::vector<std::string> labels;
    std::vector<double> values;
    std::vector<std::string> group_values;
    const bool has_groups = !table.groups.empty();
    for (std::size_t row = 0U; row < table.dates.size(); ++row) {
        for (std::size_t label = 0U; label < table.labels.size(); ++label) {
            dates.push_back(table.dates[row]);
            labels.push_back(table.labels[label]);
            values.push_back(table.values[row * table.labels.size() + label]);
            if (has_groups) {
                group_values.push_back(table.groups[row]);
            }
        }
    }
    add_int64_column(frame, "date", std::move(dates));
    if (has_groups) {
        add_string_column(frame, "group", std::move(group_values));
    }
    add_string_column(frame, "label", std::move(labels));
    add_double_column(frame, "value", std::move(values));
    return frame;
}

dataframe ic_summary_frame(const factor_ic_summary& summary) {
    auto frame = make_frame(ordinal_index(summary.means.size()));
    std::vector<std::string> labels;
    std::vector<double> means;
    std::vector<std::string> groups;
    const bool has_groups = !summary.groups.empty();
    if (has_groups) {
        for (std::size_t group = 0U; group < summary.groups.size(); ++group) {
            for (std::size_t label = 0U; label < summary.labels.size();
                 ++label) {
                groups.push_back(summary.groups[group]);
                labels.push_back(summary.labels[label]);
                means.push_back(
                    summary.means[group * summary.labels.size() + label]);
            }
        }
    } else {
        for (std::size_t label = 0U; label < summary.labels.size();
             ++label) {
            labels.push_back(summary.labels[label]);
            means.push_back(summary.means[label]);
        }
    }
    if (has_groups) {
        add_string_column(frame, "group", std::move(groups));
    }
    add_string_column(frame, "label", std::move(labels));
    add_double_column(frame, "mean", std::move(means));
    return frame;
}

dataframe quantile_spread_frame(const quantile_spread_result& result) {
    auto frame = make_frame(ordinal_index(result.labels.size()));
    if (!result.groups.empty()) {
        add_string_column(frame, "group", result.groups);
    }
    add_string_column(frame, "label", result.labels);
    add_double_column(frame, "spread", result.spreads);
    add_double_column(frame, "standard_error", result.standard_errors);
    return frame;
}

dataframe quantile_return_frame(const quantile_return_table& table) {
    auto frame = make_frame(ordinal_index(
        table.quantiles.size() * table.labels.size()));
    std::vector<int> quantiles;
    std::vector<std::string> groups;
    std::vector<std::string> labels;
    std::vector<double> means;
    std::vector<double> standard_errors;
    const bool has_groups = !table.groups.empty();
    for (std::size_t row = 0U; row < table.quantiles.size(); ++row) {
        for (std::size_t label = 0U; label < table.labels.size(); ++label) {
            quantiles.push_back(table.quantiles[row]);
            if (has_groups) {
                groups.push_back(table.groups[row]);
            }
            labels.push_back(table.labels[label]);
            means.push_back(table.means[row * table.labels.size() + label]);
            standard_errors.push_back(
                table.standard_errors[row * table.labels.size() + label]);
        }
    }
    add_int_column(frame, "quantile", std::move(quantiles));
    if (has_groups) {
        add_string_column(frame, "group", std::move(groups));
    }
    add_string_column(frame, "label", std::move(labels));
    add_double_column(frame, "mean", std::move(means));
    add_double_column(frame, "standard_error", std::move(standard_errors));
    return frame;
}

dataframe quantile_date_frame(const quantile_date_return_table& table) {
    const std::size_t rows = table.dates.size() * table.quantiles.size() *
                             table.labels.size();
    auto frame = make_frame(ordinal_index(rows));
    std::vector<timestamp> dates;
    std::vector<int> quantiles;
    std::vector<std::string> groups;
    std::vector<std::string> labels;
    std::vector<double> means;
    std::vector<double> standard_errors;
    const bool has_groups = !table.groups.empty();
    const std::size_t stride = table.quantiles.size() * table.labels.size();
    for (std::size_t date = 0U; date < table.dates.size(); ++date) {
        for (std::size_t quantile = 0U; quantile < table.quantiles.size();
             ++quantile) {
            for (std::size_t label = 0U; label < table.labels.size();
                 ++label) {
                dates.push_back(table.dates[date]);
                quantiles.push_back(table.quantiles[quantile]);
                if (has_groups) {
                    groups.push_back(table.groups[date]);
                }
                labels.push_back(table.labels[label]);
                means.push_back(table.means[date * stride +
                                            quantile * table.labels.size() +
                                            label]);
                standard_errors.push_back(
                    table.standard_errors[date * stride +
                                          quantile * table.labels.size() +
                                          label]);
            }
        }
    }
    add_int64_column(frame, "date", std::move(dates));
    add_int_column(frame, "quantile", std::move(quantiles));
    if (has_groups) {
        add_string_column(frame, "group", std::move(groups));
    }
    add_string_column(frame, "label", std::move(labels));
    add_double_column(frame, "mean", std::move(means));
    add_double_column(frame, "standard_error", std::move(standard_errors));
    return frame;
}

dataframe alpha_beta_frame(
    const std::vector<factor_alpha_beta_row>& rows) {
    auto frame = make_frame(ordinal_index(rows.size()));
    std::vector<std::string> labels;
    std::vector<double> alphas;
    std::vector<double> betas;
    for (const auto& row : rows) {
        labels.push_back(row.label);
        alphas.push_back(row.annual_alpha);
        betas.push_back(row.beta);
    }
    add_string_column(frame, "label", std::move(labels));
    add_double_column(frame, "annual_alpha", std::move(alphas));
    add_double_column(frame, "beta", std::move(betas));
    return frame;
}

dataframe event_window_frame(const event_window_result& result) {
    auto frame = make_frame(ordinal_index(result.cells.size()));
    std::vector<int> offsets;
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<double> values;
    for (const auto& cell : result.cells) {
        offsets.push_back(cell.offset);
        dates.push_back(cell.event_date);
        assets.push_back(cell.asset);
        values.push_back(cell.value);
    }
    add_int_column(frame, "offset", std::move(offsets));
    add_int64_column(frame, "event_date", std::move(dates));
    add_string_column(frame, "asset", std::move(assets));
    add_double_column(frame, "value", std::move(values));
    return frame;
}

dataframe quantile_event_frame(
    const std::vector<quantile_event_statistics>& statistics) {
    std::size_t rows = 0U;
    for (const auto& item : statistics) {
        rows += item.offsets.size();
    }
    auto frame = make_frame(ordinal_index(rows));
    std::vector<int> quantiles;
    std::vector<int> offsets;
    std::vector<double> means;
    std::vector<double> deviations;
    for (const auto& item : statistics) {
        for (std::size_t offset = 0U; offset < item.offsets.size();
             ++offset) {
            quantiles.push_back(item.quantile);
            offsets.push_back(item.offsets[offset]);
            means.push_back(item.means.empty()
                                ? std::numeric_limits<double>::quiet_NaN()
                                : item.means[offset]);
            deviations.push_back(
                item.standard_deviations.size() > offset
                    ? item.standard_deviations[offset]
                    : std::numeric_limits<double>::quiet_NaN());
        }
    }
    add_int_column(frame, "quantile", std::move(quantiles));
    add_int_column(frame, "offset", std::move(offsets));
    add_double_column(frame, "mean", std::move(means));
    add_double_column(frame, "standard_deviation", std::move(deviations));
    return frame;
}

dataframe fdr_frame(const fdr_result& result) {
    auto frame = make_frame(ordinal_index(result.p_values.size()));
    std::vector<double> adjusted = result.adjusted_p_values;
    std::vector<bool> rejected = result.rejected;
    std::vector<double> alpha(result.p_values.size(), result.alpha);
    add_double_column(frame, "p_value", result.p_values);
    add_double_column(frame, "adjusted_p_value", std::move(adjusted));
    add_bool_column(frame, "rejected", std::move(rejected));
    add_double_column(frame, "alpha", std::move(alpha));
    return frame;
}

dataframe ic_interval_frame(const ic_interval& interval) {
    auto frame = make_frame(std::vector<timestamp>{0});
    add_double_column(frame, "lower", {interval.lower});
    add_double_column(frame, "upper", {interval.upper});
    return frame;
}

dataframe fama_macbeth_frame(const fama_macbeth_result& result) {
    auto frame = make_frame(std::vector<timestamp>{0});
    add_double_column(frame, "intercept_mean", {result.intercept.mean});
    add_double_column(frame, "intercept_std_error",
                      {result.intercept.std_error});
    add_double_column(frame, "intercept_t_stat", {result.intercept.t_stat});
    add_double_column(frame, "exposure_mean", {result.exposure.mean});
    add_double_column(frame, "exposure_std_error",
                      {result.exposure.std_error});
    add_double_column(frame, "exposure_t_stat", {result.exposure.t_stat});
    add_int64_column(
        frame, "n_cross_sections",
        {static_cast<std::int64_t>(result.n_cross_sections)});
    add_int64_column(frame, "newey_west_lags",
                     {static_cast<std::int64_t>(result.newey_west_lags)});
    add_string_column(frame, "covariance",
                      {result.covariance == fama_macbeth_covariance::iid
                           ? "iid"
                           : "newey_west"});
    return frame;
}

dataframe ic_inference_frame(const ic_inference_result& result) {
    auto frame = make_frame(ordinal_index(result.hypotheses.size()));
    std::vector<double> alpha(result.hypotheses.size(), result.alpha);
    std::vector<std::string> labels;
    std::vector<std::int64_t> counts;
    std::vector<double> means;
    std::vector<double> statistics;
    std::vector<double> p_values;
    std::vector<double> adjusted;
    std::vector<bool> rejected;
    std::vector<bool> testable;
    for (const auto& hypothesis : result.hypotheses) {
        labels.push_back(hypothesis.label);
        counts.push_back(
            static_cast<std::int64_t>(hypothesis.n_observations));
        means.push_back(hypothesis.mean_ic);
        statistics.push_back(hypothesis.t_stat);
        p_values.push_back(hypothesis.p_value);
        adjusted.push_back(hypothesis.adjusted_p_value);
        rejected.push_back(hypothesis.rejected);
        testable.push_back(hypothesis.testable);
    }
    add_double_column(frame, "alpha", std::move(alpha));
    add_string_column(frame, "label", std::move(labels));
    add_int64_column(frame, "n_observations", std::move(counts));
    add_double_column(frame, "mean_ic", std::move(means));
    add_double_column(frame, "t_stat", std::move(statistics));
    add_double_column(frame, "p_value", std::move(p_values));
    add_double_column(frame, "adjusted_p_value", std::move(adjusted));
    add_bool_column(frame, "rejected", std::move(rejected));
    add_bool_column(frame, "testable", std::move(testable));
    return frame;
}

dataframe calendar_frame(const trading_calendar& calendar) {
    auto frame = make_frame(ordinal_index(7U + calendar.holidays.size()));
    std::vector<int> weekdays;
    std::vector<bool> trading;
    std::vector<std::int64_t> holiday_days;
    for (int weekday = 0; weekday < 7; ++weekday) {
        weekdays.push_back(weekday);
        trading.push_back(calendar.weekdays[weekday]);
        holiday_days.push_back(0);
    }
    for (const auto holiday : calendar.holidays) {
        weekdays.push_back(-1);
        trading.push_back(false);
        holiday_days.push_back(holiday);
    }
    add_int_column(frame, "weekday", std::move(weekdays));
    add_bool_column(frame, "trading", std::move(trading));
    add_int64_column(frame, "holiday", std::move(holiday_days));
    return frame;
}

// ---------------------------------------------------------------------------
// Report document projections. report_document is the owning runtime value for
// composite native results (multiple frames, scalar records and warnings).
// ---------------------------------------------------------------------------

report_document make_document(const std::string& domain,
                              const std::string& title) {
    report_document document;
    document.domain = domain;
    document.title = title;
    return document;
}

report_section& add_section(report_document& document,
                            const std::string& key,
                            const std::string& title) {
    report_section section;
    section.key = key;
    section.title = title;
    document.sections.push_back(std::move(section));
    return document.sections.back();
}

void add_number_metric(report_section& section, const std::string& key,
                       const std::string& label, const std::string& unit,
                       const double value) {
    report_metric metric;
    metric.key = key;
    metric.label = label;
    metric.unit = unit;
    if (std::isfinite(value)) {
        metric.number = value;
    }
    section.metrics.push_back(std::move(metric));
}

void add_text_metric(report_section& section, const std::string& key,
                     const std::string& label, const std::string& value) {
    report_metric metric;
    metric.key = key;
    metric.label = label;
    metric.unit = "text";
    metric.text = value;
    section.metrics.push_back(std::move(metric));
}

template <typename T>
report_cell typed_cell(const dataframe& frame, const std::string& name,
                       const std::size_t row) {
    const auto& values = frame.get_column<T>(name.c_str());
    if constexpr (std::is_same_v<T, bool>) {
        return report_cell::boolean_cell(static_cast<bool>(values[row]));
    } else if constexpr (std::is_same_v<T, std::string>) {
        return report_cell::text_cell(values[row]);
    } else if constexpr (std::is_same_v<T, float> ||
                         std::is_same_v<T, double>) {
        const double value = static_cast<double>(values[row]);
        return std::isfinite(value) ? report_cell::number_cell(value)
                                    : report_cell::missing();
    } else {
        return report_cell::text_cell(
            std::to_string(static_cast<long long>(values[row])));
    }
}

report_cell cell_for(const dataframe& frame, const std::string& name,
                     const std::type_index type, const std::size_t row) {
    if (type == std::type_index(typeid(bool))) {
        return typed_cell<bool>(frame, name, row);
    }
    if (type == std::type_index(typeid(char))) {
        return typed_cell<char>(frame, name, row);
    }
    if (type == std::type_index(typeid(signed char))) {
        return typed_cell<signed char>(frame, name, row);
    }
    if (type == std::type_index(typeid(unsigned char))) {
        return typed_cell<unsigned char>(frame, name, row);
    }
    if (type == std::type_index(typeid(short))) {
        return typed_cell<short>(frame, name, row);
    }
    if (type == std::type_index(typeid(unsigned short))) {
        return typed_cell<unsigned short>(frame, name, row);
    }
    if (type == std::type_index(typeid(int))) {
        return typed_cell<int>(frame, name, row);
    }
    if (type == std::type_index(typeid(unsigned int))) {
        return typed_cell<unsigned int>(frame, name, row);
    }
    if (type == std::type_index(typeid(long))) {
        return typed_cell<long>(frame, name, row);
    }
    if (type == std::type_index(typeid(unsigned long))) {
        return typed_cell<unsigned long>(frame, name, row);
    }
    if (type == std::type_index(typeid(long long))) {
        return typed_cell<long long>(frame, name, row);
    }
    if (type == std::type_index(typeid(unsigned long long))) {
        return typed_cell<unsigned long long>(frame, name, row);
    }
    if (type == std::type_index(typeid(float))) {
        return typed_cell<float>(frame, name, row);
    }
    if (type == std::type_index(typeid(double))) {
        return typed_cell<double>(frame, name, row);
    }
    if (type == std::type_index(typeid(std::string))) {
        return typed_cell<std::string>(frame, name, row);
    }
    throw input_error{"unsupported dataframe column type: " + name};
}

report_table table_from_frame(const std::string& key, const std::string& title,
                              const dataframe& frame) {
    const auto columns = frame.get_columns_info<
        bool, char, signed char, unsigned char, short, unsigned short, int,
        unsigned int, long, unsigned long, long long, unsigned long long,
        float, double, std::string>();
    if (columns.size() != frame.shape().second) {
        throw input_error{"unsupported dataframe column type in projection"};
    }
    report_table table;
    table.key = key;
    table.title = title;
    table.columns.push_back("index");
    for (const auto& info : columns) {
        table.columns.push_back(std::string{std::get<0>(info).c_str()});
    }
    const auto& index = frame.get_index();
    table.rows.reserve(index.size());
    for (std::size_t row = 0U; row < index.size(); ++row) {
        std::vector<report_cell> cells;
        cells.reserve(table.columns.size());
        cells.push_back(
            report_cell::text_cell(std::to_string(index[row])));
        for (const auto& info : columns) {
            cells.push_back(cell_for(
                frame, std::string{std::get<0>(info).c_str()},
                std::get<2>(info), row));
        }
        table.rows.push_back(std::move(cells));
    }
    return table;
}

void add_table(report_section& section, const std::string& key,
               const std::string& title, const dataframe& frame) {
    section.tables.push_back(table_from_frame(key, title, frame));
}

dataframe loading_table_frame(const factor_loading_table& loadings) {
    std::size_t rows = 0U;
    for (std::size_t date = 0U; date < loadings.dates.size(); ++date) {
        rows += loadings.assets.size() * loadings.factors.size();
    }
    auto frame = make_frame(ordinal_index(rows));
    std::vector<std::int64_t> dates;
    std::vector<std::string> assets;
    std::vector<std::string> factors;
    std::vector<double> values;
    dates.reserve(rows);
    assets.reserve(rows);
    factors.reserve(rows);
    values.reserve(rows);
    for (std::size_t date = 0U; date < loadings.dates.size(); ++date) {
        for (std::size_t asset = 0U; asset < loadings.assets.size();
             ++asset) {
            for (std::size_t factor = 0U; factor < loadings.factors.size();
                 ++factor) {
                dates.push_back(loadings.dates[date]);
                assets.push_back(loadings.assets[asset]);
                factors.push_back(loadings.factors[factor]);
                values.push_back(
                    loadings.values[(date * loadings.assets.size() + asset) *
                                        loadings.factors.size() +
                                    factor]);
            }
        }
    }
    add_int64_column(frame, "date", std::move(dates));
    add_string_column(frame, "asset", std::move(assets));
    add_string_column(frame, "factor", std::move(factors));
    add_double_column(frame, "value", std::move(values));
    return frame;
}

factor_loading_table parse_loading_table(const runtime_inputs& inputs,
                                         const std::string& name) {
    const auto& frame = require_frame(inputs, name);
    const auto dates = frame_dates(frame);
    const auto& symbols = require_string_column(frame, "symbol");
    if (symbols.size() != dates.size()) {
        throw input_error{"factor-loading symbol column must match rows: " +
                          name};
    }
    factor_loading_table loadings;
    for (const auto& info : frame.get_columns_info<double>()) {
        loadings.factors.push_back(std::string{std::get<0>(info).c_str()});
    }
    if (loadings.factors.empty()) {
        throw input_error{"factor loadings must have a double factor column: " +
                          name};
    }
    std::vector<timestamp> unique_dates;
    std::set<timestamp> seen_dates;
    for (const auto date : dates) {
        if (seen_dates.insert(date).second) {
            unique_dates.push_back(date);
        }
    }
    std::vector<std::string> unique_assets;
    std::set<std::string> seen_assets;
    for (const auto& symbol : symbols) {
        if (seen_assets.insert(symbol).second) {
            unique_assets.push_back(symbol);
        }
    }
    loadings.dates = unique_dates;
    loadings.assets = unique_assets;
    loadings.values.assign(unique_dates.size() * unique_assets.size() *
                               loadings.factors.size(),
                           std::numeric_limits<double>::quiet_NaN());
    std::map<timestamp, std::size_t> date_positions;
    for (std::size_t position = 0U; position < unique_dates.size();
         ++position) {
        date_positions.emplace(unique_dates[position], position);
    }
    std::map<std::string, std::size_t> asset_positions;
    for (std::size_t position = 0U; position < unique_assets.size();
         ++position) {
        asset_positions.emplace(unique_assets[position], position);
    }
    std::set<std::pair<timestamp, std::string>> seen_keys;
    for (std::size_t row = 0U; row < dates.size(); ++row) {
        const auto key = std::make_pair(dates[row], symbols[row]);
        if (!seen_keys.insert(key).second) {
            throw input_error{
                "factor-loading timestamp and symbol keys must be unique"};
        }
        const std::size_t date = date_positions.at(dates[row]);
        const std::size_t asset = asset_positions.at(symbols[row]);
        for (std::size_t factor = 0U; factor < loadings.factors.size();
             ++factor) {
            const auto& column =
                require_double_column(frame, loadings.factors[factor]);
            loadings.values[(date * unique_assets.size() + asset) *
                                loadings.factors.size() +
                            factor] = column[row];
        }
    }
    return loadings;
}

std::vector<std::string> sector_names(const runtime_inputs& inputs,
                                      const std::string& frame_name) {
    const auto explicit_names = parse_string_list(inputs, "sector_columns",
                                                  false);
    if (!explicit_names.empty()) {
        return explicit_names;
    }
    const auto& frame = require_frame(inputs, frame_name);
    std::vector<std::string> names;
    for (const auto& info : frame.get_columns_info<double>()) {
        names.push_back(std::string{std::get<0>(info).c_str()});
    }
    if (names.empty()) {
        throw input_error{"Brinson weights must have a double sector column"};
    }
    return names;
}

std::vector<std::string_view> views(const std::vector<std::string>& values) {
    std::vector<std::string_view> result;
    result.reserve(values.size());
    for (const auto& value : values) {
        result.push_back(value);
    }
    return result;
}

dataframe with_leading_string_column(const dataframe& source,
                                     const std::string& name,
                                     const std::vector<std::string>& values) {
    if (values.size() != source.get_index().size()) {
        throw input_error{"period labels must match the result row count"};
    }
    auto frame = make_frame(std::vector<timestamp>(source.get_index().begin(),
                                                   source.get_index().end()));
    add_string_column(frame, name, values);
    for (const auto& info : source.get_columns_info<double>()) {
        const std::string column{std::get<0>(info).c_str()};
        add_double_column(frame, column,
                          double_values(source, column));
    }
    return frame;
}

// ---------------------------------------------------------------------------
// Factor configuration readers.
// ---------------------------------------------------------------------------

factor_analysis_config read_analysis_config(const runtime_inputs& inputs) {
    factor_analysis_config config;
    config.long_short = optional_bool(inputs, "long_short", true);
    config.group_neutral = optional_bool(inputs, "group_neutral", false);
    config.equal_weight = optional_bool(inputs, "equal_weight", false);
    config.by_group = optional_bool(inputs, "by_group", false);
    config.periods = parse_string_list(inputs, "periods", false);
    config.turnover_periods = parse_int_list(inputs, "turnover_periods", {1});
    config.time_aggregation =
        parse_string_list(inputs, "time_aggregation", false);
    if (config.time_aggregation.empty()) {
        config.time_aggregation = {"M"};
    }
    config.include_portfolio_inputs =
        optional_bool(inputs, "include_portfolio_inputs", true);
    config.portfolio_benchmark_period =
        optional_string(inputs, "portfolio_benchmark_period", "1D");
    return config;
}

quantization_config read_quantization_config(const runtime_inputs& inputs) {
    quantization_config config;
    config.quantiles = optional_int(inputs, "quantiles", 5);
    config.bins = parse_double_list(inputs, "bins", {});
    config.use_bins = inputs.find("bins") != inputs.end();
    config.by_group = optional_bool(inputs, "by_group", false);
    config.no_raise = optional_bool(inputs, "no_raise", false);
    config.zero_aware = optional_bool(inputs, "zero_aware", false);
    return config;
}

prepare_factor_config read_prepare_config(const runtime_inputs& inputs) {
    prepare_factor_config config;
    config.quantiles = optional_int(inputs, "quantiles", 5);
    config.bins = parse_double_list(inputs, "bins", {});
    config.use_bins = inputs.find("bins") != inputs.end();
    config.periods = parse_int_list(inputs, "periods", {1, 5, 10});
    config.max_loss = optional_number(inputs, "max_loss").value_or(0.35);
    config.binning_by_group =
        optional_bool(inputs, "binning_by_group", false);
    config.filter_zscore = optional_number(inputs, "filter_zscore");
    config.zero_aware = optional_bool(inputs, "zero_aware", false);
    config.cumulative_returns =
        optional_bool(inputs, "cumulative_returns", true);
    config.grouping = parse_grouping(inputs);
    return config;
}

// ---------------------------------------------------------------------------
// Kernel adapters.
// ---------------------------------------------------------------------------

runtime_value brinson_attribution_op(const runtime_inputs& inputs) {
    const auto sectors = sector_names(inputs, "portfolio_weights");
    const auto sector_views = views(sectors);
    const auto result = brinson_attribution(
        require_frame(inputs, "portfolio_returns"),
        require_frame(inputs, "benchmark_returns"),
        require_frame(inputs, "portfolio_weights"),
        require_frame(inputs, "benchmark_weights"), sector_views);
    auto frame = make_frame(std::vector<timestamp>{0});
    add_double_column(frame, brinson_allocation_column.data(),
                      {result.allocation});
    add_double_column(frame, brinson_selection_column.data(),
                      {result.selection});
    add_double_column(frame, brinson_interaction_column.data(),
                      {result.interaction});
    add_double_column(frame, brinson_total_column.data(), {result.total});
    add_double_column(frame, brinson_portfolio_return_column.data(),
                      {result.portfolio_return});
    add_double_column(frame, brinson_benchmark_return_column.data(),
                      {result.benchmark_return});
    return runtime_value(std::move(frame));
}

runtime_value brinson_cumulative_op(const runtime_inputs& inputs) {
    const auto sectors = sector_names(inputs, "portfolio_weights");
    const auto sector_views = views(sectors);
    const auto result = brinson_cumulative(
        require_frame(inputs, "portfolio_returns"),
        require_frame(inputs, "benchmark_returns"),
        require_frame(inputs, "portfolio_weights"),
        require_frame(inputs, "benchmark_weights"), sector_views);
    auto frame = make_frame(std::vector<timestamp>{0});
    add_double_column(frame, brinson_allocation_column.data(),
                      {result.allocation});
    add_double_column(frame, brinson_selection_column.data(),
                      {result.selection});
    add_double_column(frame, brinson_interaction_column.data(),
                      {result.interaction});
    add_double_column(frame, brinson_total_column.data(), {result.total});
    add_double_column(frame, "portfolio_cumulative",
                      {result.portfolio_cumulative});
    add_double_column(frame, "benchmark_cumulative",
                      {result.benchmark_cumulative});
    return runtime_value(std::move(frame));
}

runtime_value brinson_results_op(const runtime_inputs& inputs) {
    const auto sectors = sector_names(inputs, "portfolio_weights");
    const auto sector_views = views(sectors);
    auto frame = brinson_results(
        require_frame(inputs, "portfolio_returns"),
        require_frame(inputs, "benchmark_returns"),
        require_frame(inputs, "portfolio_weights"),
        require_frame(inputs, "benchmark_weights"), sector_views);
    const auto periods = parse_string_list(inputs, "periods", false);
    if (!periods.empty()) {
        frame = with_leading_string_column(frame, "period", periods);
    }
    return runtime_value(std::move(frame));
}

runtime_value idiosyncratic_risk_op(const runtime_inputs& inputs) {
    const auto result = calculate_idiosyncratic_risk(
        require_frame(inputs, "returns"),
        require_frame(inputs, "factor_data"),
        optional_number(inputs, "market_beta"),
        optional_number(inputs, "risk_free").value_or(0.0));
    auto frame = make_frame(ordinal_index(result.assets.size()));
    add_string_column(frame, "asset", result.assets);
    add_double_column(frame, "specific_volatility",
                      result.specific_volatility);
    add_double_column(frame, "market_beta",
                      std::vector<double>(result.assets.size(),
                                          result.market_beta));
    return runtime_value(std::move(frame));
}

runtime_value fetch_ff_factors_op(const runtime_inputs& inputs) {
    const auto start = require_string(inputs, "start");
    const auto end = require_string(inputs, "end");
    const auto library = optional_string(inputs, "library", "french");
    const auto copy = optional_bool(inputs, "copy", true);
    const dataframe provider_frame = require_frame(inputs, "provider");
    return runtime_value(fetch_ff_factors(
        start, end, library,
        [provider_frame](std::string_view, std::string_view,
                         std::string_view) { return provider_frame; },
        copy));
}

runtime_value align_and_warn_op(const runtime_inputs& inputs) {
    const auto loadings = parse_loading_table(inputs, "factor_loadings");
    const auto result = align_and_warn(
        require_frame(inputs, "returns"),
        require_frame(inputs, "positions"),
        require_frame(inputs, "factor_returns"), loadings);
    auto document = make_document("attribution", "Performance Attribution Alignment");
    auto& section = add_section(document, "alignment", "Aligned Inputs");
    add_table(section, "returns", "Returns", result.returns);
    add_table(section, "positions", "Positions", result.positions);
    add_table(section, "factor_returns", "Factor Returns",
              result.factor_returns);
    add_table(section, "factor_loadings", "Factor Loadings",
              loading_table_frame(result.factor_loadings));
    section.notes = result.warnings;
    return runtime_value(std::move(document));
}

runtime_value compute_exposures_impl(const runtime_inputs& inputs,
                                     const bool normalize) {
    const auto loadings = parse_loading_table(inputs, "factor_loadings");
    const auto symbol_column =
        optional_string(inputs, "symbol_column_name",
                        std::string{attribution_symbol_column});
    const auto cash_column =
        optional_string(inputs, "cash_column_name",
                        std::string{fincore::cash_column});
    const auto factor_views = views(loadings.factors);
    dataframe normalized = require_frame(inputs, "positions");
    if (normalize) {
        normalized = normalize_attribution_positions(
            normalized, optional_bool(inputs, "pos_in_dollars", true),
            cash_column);
    }
    return runtime_value(compute_exposures(normalized,
                                           require_frame(inputs,
                                                         "factor_loadings"),
                                           factor_views, cash_column,
                                           symbol_column));
}

runtime_value compute_exposures_op(const runtime_inputs& inputs) {
    return compute_exposures_impl(
        inputs, optional_bool(inputs, "stack_positions", true));
}

runtime_value create_perf_attrib_stats_op(const runtime_inputs& inputs) {
    perf_attribution_result result;
    result.risk_exposures = require_frame(inputs, "risk_exposures");
    result.attribution = require_frame(inputs, "perf_attrib");
    const auto statistics = create_perf_attrib_stats(
        result, optional_number(inputs, "annualization")
                    .value_or(daily_annualization));
    auto document =
        make_document("attribution", "Performance Attribution Statistics");
    auto& summary = add_section(document, "summary", "Summary");
    for (const auto& [key, value] : statistics.summary) {
        add_number_metric(summary, key, key, "ratio", value);
    }
    add_table(summary, "factor_summary", "Factor Summary",
              statistics.factor_summary);
    return runtime_value(std::move(document));
}

runtime_value cumulative_returns_less_costs_op(
    const runtime_inputs& inputs) {
    const auto returns_column =
        optional_string(inputs, "returns_column_name",
                        std::string{fincore::returns_column});
    const auto* costs = optional_frame(inputs, "costs");
    if (costs == nullptr) {
        return runtime_value(cumulative_returns_less_costs(
            require_frame(inputs, "returns"), returns_column));
    }
    const auto costs_column =
        optional_string(inputs, "costs_column_name",
                        std::string{fincore::returns_column});
    return runtime_value(cumulative_returns_less_costs(
        require_frame(inputs, "returns"), *costs, returns_column,
        costs_column));
}

runtime_value perf_attrib_impl(const runtime_inputs& inputs,
                               const bool normalize) {
    perf_attribution_result result;
    if (normalize) {
        result = perf_attrib(
            require_frame(inputs, "returns"),
            require_frame(inputs, "positions"),
            require_frame(inputs, "factor_returns"),
            require_frame(inputs, "factor_loadings"),
            optional_bool(inputs, "positions_in_dollars", true),
            optional_string(inputs, "returns_column_name",
                            std::string{returns_column}),
            optional_string(inputs, "cash_column_name",
                            std::string{cash_column}),
            optional_string(inputs, "symbol_column_name",
                            std::string{attribution_symbol_column}));
    } else {
        result = perf_attrib_core(
            require_frame(inputs, "returns"),
            require_frame(inputs, "positions"),
            require_frame(inputs, "factor_returns"),
            require_frame(inputs, "factor_loadings"),
            optional_string(inputs, "returns_column_name",
                            std::string{returns_column}),
            optional_string(inputs, "cash_column_name",
                            std::string{cash_column}),
            optional_string(inputs, "symbol_column_name",
                            std::string{attribution_symbol_column}));
    }
    auto document = make_document("attribution", "Performance Attribution");
    auto& section = add_section(document, "attribution", "Attribution");
    add_table(section, "risk_exposures", "Risk Exposures",
              result.risk_exposures);
    add_table(section, "attribution", "Factor Attribution",
              result.attribution);
    return runtime_value(std::move(document));
}

runtime_value style_analysis_op(const runtime_inputs& inputs) {
    style_analysis_options options;
    const auto market_caps = parse_double_list(inputs, "market_caps", {});
    const auto book_to_price = parse_double_list(inputs, "book_to_price", {});
    const auto value_scores = parse_double_list(inputs, "value_scores", {});
    const auto quantiles = parse_double_list(inputs, "size_quantiles",
                                             {0.5, 0.5});
    if (quantiles.size() != 2U) {
        throw input_error{"size_quantiles must contain exactly two values"};
    }
    options.market_caps = std::span<const double>{market_caps};
    options.book_to_price = std::span<const double>{book_to_price};
    options.value_scores = std::span<const double>{value_scores};
    options.momentum_window =
        optional_size(inputs, "momentum_window", 252U);
    options.size_quantiles = {quantiles[0], quantiles[1]};
    const auto result = style_analysis(
        require_frame(inputs, "returns"), options);
    auto document = make_document("attribution", "Style Analysis");
    auto& section = add_section(document, "style_analysis", "Style Analysis");
    add_table(section, "exposures", "Factor Exposures", result.exposures);
    add_table(section, "returns_by_style", "Returns by Style",
              result.returns_by_style);
    add_table(section, "overall_returns", "Overall Returns",
              result.overall_returns);
    return runtime_value(std::move(document));
}

runtime_value analyze_performance_by_style_op(const runtime_inputs& inputs) {
    const auto* exposures = optional_frame(inputs, "style_exposures");
    if (exposures == nullptr) {
        return runtime_value(analyze_performance_by_style(
            require_frame(inputs, "returns")));
    }
    return runtime_value(analyze_performance_by_style(
        require_frame(inputs, "returns"), *exposures));
}

runtime_value calculate_style_tilts_op(const runtime_inputs& inputs) {
    return runtime_value(calculate_style_tilts(
        require_frame(inputs, "returns"),
        optional_size(inputs, "window", 252U)));
}

runtime_value calculate_regression_attribution_op(
    const runtime_inputs& inputs) {
    const auto* styles = optional_frame(inputs, "style_returns");
    if (styles == nullptr) {
        return runtime_value(calculate_regression_attribution(
            require_frame(inputs, "portfolio_returns")));
    }
    const auto selected =
        parse_string_list(inputs, "selected_styles", true);
    if (selected.empty()) {
        throw input_error{
            "selected_styles must be non-empty with style_returns"};
    }
    const auto selected_views = views(selected);
    return runtime_value(calculate_regression_attribution(
        require_frame(inputs, "portfolio_returns"), *styles,
        selected_views));
}

runtime_value fetch_style_factors_op(const runtime_inputs& inputs) {
    const auto tickers = parse_string_list(inputs, "tickers", true);
    const auto factor_values =
        parse_string_list(inputs, "factors", false);
    std::optional<std::vector<std::string>> factors;
    if (!factor_values.empty()) {
        factors = factor_values;
    }
    const auto library = optional_string(inputs, "library", "us");
    const dataframe provider_frame = require_frame(inputs, "provider");
    return runtime_value(fetch_style_factors(
        tickers, factors, library,
        [provider_frame](const std::vector<std::string>&,
                         const std::optional<std::vector<std::string>>&,
                         std::string_view) { return provider_frame; }));
}

// Factor calendar -----------------------------------------------------------

runtime_value add_calendar_timedelta_op(const runtime_inputs& inputs) {
    const auto delta = parse_timedelta(inputs, "timedelta");
    const auto calendar = calendar_from_inputs(inputs);
    const auto* value = optional_value(inputs, "input");
    if (value == nullptr) {
        throw input_error{"missing runtime input: input"};
    }
    if (const auto* scalar = std::get_if<std::int64_t>(&value->data)) {
        return runtime_value(
            add_custom_calendar_timedelta(*scalar, delta, calendar));
    }
    const auto values = parse_timestamp_list(inputs, "input", true);
    return runtime_value(
        timestamp_list_frame(add_custom_calendar_timedelta(values, delta,
                                                           calendar),
                             "date"));
}

runtime_value diff_calendar_timedeltas_op(const runtime_inputs& inputs) {
    return runtime_value(diff_custom_calendar_timedeltas(
        require_integer(inputs, "start"), require_integer(inputs, "end"),
        calendar_from_inputs(inputs)));
}

runtime_value infer_trading_calendar_op(const runtime_inputs& inputs) {
    return runtime_value(calendar_frame(infer_trading_calendar(
        parse_timestamp_list(inputs, "factor_idx", true),
        parse_timestamp_list(inputs, "prices_idx", true))));
}

runtime_value timedelta_to_string_op(const runtime_inputs& inputs) {
    return runtime_value(timedelta_to_string(
        parse_timedelta(inputs, "timedelta")));
}

runtime_value timedelta_strings_to_integers_op(
    const runtime_inputs& inputs) {
    return runtime_value(int_list_frame(timedelta_strings_to_integers(
        parse_string_list(inputs, "sequence", true))));
}

runtime_value get_forward_returns_columns_op(
    const runtime_inputs& inputs) {
    const auto columns = get_forward_returns_columns(
        parse_string_list(inputs, "columns", true),
        optional_bool(inputs, "require_exact_day_multiple", false));
    return runtime_value(string_list_frame(columns, "column"));
}

runtime_value backshift_returns_series_op(const runtime_inputs& inputs) {
    const auto series = parse_factor_series(inputs, "series");
    auto levels = parse_timestamp_list(inputs, "date_levels", false);
    if (levels.empty()) {
        levels = series.dates;
        std::sort(levels.begin(), levels.end());
        levels.erase(std::unique(levels.begin(), levels.end()),
                     levels.end());
    }
    return runtime_value(factor_series_frame(backshift_returns_series(
        series, levels, require_int(inputs, "N"))));
}

// Factor costs --------------------------------------------------------------

runtime_value estimate_factor_capacity_op(
    const runtime_inputs& inputs) {
    const auto result = estimate_factor_capacity(
        parse_weight_rows(inputs, "weights"),
        require_frame(inputs, "dollar_volume"),
        require_number(inputs, "max_participation"));
    auto document = make_document("factor_analysis", "Factor Capacity");
    auto& summary = add_section(document, "capacity", "Capacity");
    add_number_metric(summary, "maximum_portfolio_value",
                      "Maximum portfolio value", "currency",
                      result.maximum_portfolio_value);
    add_number_metric(summary, "max_participation", "Maximum participation",
                      "ratio", result.max_participation);
    add_text_metric(summary, "binding_asset", "Binding asset",
                    result.binding_asset);
    add_text_metric(summary, "binding_date", "Binding date",
                    std::to_string(result.binding_date));
    auto by_date = make_frame(ordinal_index(result.dates.size()));
    add_int64_column(by_date, "date", result.dates);
    add_double_column(by_date, "maximum_portfolio_value",
                      result.maximum_portfolio_value_by_date);
    add_table(summary, "capacity_by_date", "Capacity by Date", by_date);
    auto trade_weights =
        make_frame(ordinal_index(result.dates.size() * result.assets.size()));
    std::vector<std::int64_t> dates;
    std::vector<std::string> assets;
    std::vector<double> weights;
    for (std::size_t date = 0U; date < result.dates.size(); ++date) {
        for (std::size_t asset = 0U; asset < result.assets.size(); ++asset) {
            dates.push_back(result.dates[date]);
            assets.push_back(result.assets[asset]);
            weights.push_back(
                result.trade_weights[date * result.assets.size() + asset]);
        }
    }
    add_int64_column(trade_weights, "date", std::move(dates));
    add_string_column(trade_weights, "asset", std::move(assets));
    add_double_column(trade_weights, "weight", std::move(weights));
    add_table(summary, "trade_weights", "Trade Weights", trade_weights);
    return runtime_value(std::move(document));
}

runtime_value apply_factor_costs_op(const runtime_inputs& inputs) {
    factor_cost_model model;
    model.half_spread_bps =
        optional_number(inputs, "half_spread_bps").value_or(0.0);
    model.impact_coefficient =
        optional_number(inputs, "impact_coefficient").value_or(0.0);
    model.max_participation =
        optional_number(inputs, "max_participation").value_or(1.0);
    model.impact_exponent =
        optional_number(inputs, "impact_exponent").value_or(0.5);
    const auto gross_returns = parse_double_list(inputs, "gross_returns");
    const auto result = apply_factor_costs(
        gross_returns, parse_weight_rows(inputs, "weights"),
        require_frame(inputs, "dollar_volume"),
        require_number(inputs, "portfolio_value"), model,
        optional_frame(inputs, "borrow_rates"),
        optional_frame(inputs, "borrow_available"));
    auto document = make_document("factor_analysis", "Factor Costs");
    auto& summary = add_section(document, "costs", "Costs");
    add_number_metric(summary, "portfolio_value", "Portfolio value",
                      "currency", result.portfolio_value);
    add_number_metric(summary, "half_spread_bps", "Half spread", "bps",
                      result.model.half_spread_bps);
    add_number_metric(summary, "impact_coefficient", "Impact coefficient",
                      "ratio", result.model.impact_coefficient);
    add_number_metric(summary, "max_participation", "Maximum participation",
                      "ratio", result.model.max_participation);
    add_number_metric(summary, "impact_exponent", "Impact exponent", "ratio",
                      result.model.impact_exponent);
    auto daily = make_frame(ordinal_index(result.dates.size()));
    add_int64_column(daily, "date", result.dates);
    add_double_column(daily, "gross_returns", result.gross_returns);
    add_double_column(daily, "net_returns", result.net_returns);
    add_double_column(daily, "turnover", result.turnover);
    add_double_column(daily, "spread_cost", result.spread_cost);
    add_double_column(daily, "impact_cost", result.impact_cost);
    add_double_column(daily, "borrow_cost", result.borrow_cost);
    add_double_column(daily, "total_cost", result.total_cost);
    add_table(summary, "daily_costs", "Daily Costs", daily);
    return runtime_value(std::move(document));
}

// Factor data ---------------------------------------------------------------

runtime_value compute_forward_returns_op(const runtime_inputs& inputs) {
    return runtime_value(forward_table_frame(compute_forward_returns(
        parse_factor_series(inputs, "factor"),
        require_frame(inputs, "prices"),
        parse_int_list(inputs, "periods", {1, 5, 10}),
        optional_number(inputs, "filter_zscore"),
        optional_bool(inputs, "cumulative_returns", true))));
}

runtime_value prepare_factor_data_op(const runtime_inputs& inputs) {
    return runtime_value(prepared_frame(prepare_factor_data(
        parse_factor_series(inputs, "factor"),
        require_frame(inputs, "prices"), read_prepare_config(inputs))));
}

runtime_value prepare_factor_data_by_horizon_op(
    const runtime_inputs& inputs) {
    const auto result = prepare_factor_data_by_horizon(
        parse_factor_series(inputs, "factor"),
        require_frame(inputs, "prices"), read_prepare_config(inputs));
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<std::string> horizons;
    std::vector<int> quantiles;
    std::vector<std::string> groups;
    std::vector<double> factors;
    std::vector<std::vector<double>> forwards;
    bool has_groups = false;
    std::vector<std::string> labels;
    if (!result.by_horizon.empty()) {
        labels = result.by_horizon.front().second.forward_labels;
    }
    forwards.resize(labels.size());
    for (const auto& [horizon, data] : result.by_horizon) {
        for (const auto& row : data.rows) {
            dates.push_back(row.date);
            assets.push_back(row.asset);
            horizons.push_back(horizon);
            quantiles.push_back(row.quantile);
            groups.push_back(row.group);
            factors.push_back(row.factor);
            has_groups = has_groups || row.has_group;
            for (std::size_t label = 0U; label < labels.size(); ++label) {
                forwards[label].push_back(
                    label < row.forward_returns.size()
                        ? row.forward_returns[label]
                        : std::numeric_limits<double>::quiet_NaN());
            }
        }
    }
    auto frame = make_frame(ordinal_index(dates.size()));
    add_string_column(frame, "horizon", std::move(horizons));
    add_int64_column(frame, "date", std::move(dates));
    add_string_column(frame, "asset", std::move(assets));
    add_int_column(frame, "quantile", std::move(quantiles));
    if (has_groups) {
        add_string_column(frame, "group", std::move(groups));
    }
    add_double_column(frame, "factor", std::move(factors));
    for (std::size_t label = 0U; label < labels.size(); ++label) {
        add_double_column(frame, labels[label], std::move(forwards[label]));
    }
    return runtime_value(std::move(frame));
}

runtime_value prepare_factor_data_from_forward_returns_op(
    const runtime_inputs& inputs) {
    return runtime_value(prepared_frame(
        prepare_factor_data_from_forward_returns(
            parse_factor_series(inputs, "factor"),
            parse_forward_table(inputs, "forward_returns"),
            read_prepare_config(inputs))));
}

runtime_value prepare_pit_factor_data_op(const runtime_inputs& inputs) {
    return runtime_value(prepared_frame(prepare_pit_factor_data(
        parse_pit_observations(inputs, "observations"),
        require_frame(inputs, "prices"),
        parse_timestamp_list(inputs, "evaluation_dates", true),
        read_prepare_config(inputs))));
}

runtime_value quantize_factor_op(const runtime_inputs& inputs) {
    const auto series = parse_factor_series(inputs, "factor_data");
    factor_grouping grouping;
    const auto& frame = require_frame(inputs, "factor_data");
    if (frame.has_column("group")) {
        const auto& assets = require_string_column(frame, "asset");
        const auto& groups = require_string_column(frame, "group");
        for (std::size_t row = 0U; row < assets.size(); ++row) {
            if (!groups[row].empty()) {
                grouping.group_by_asset.emplace(assets[row], groups[row]);
            }
        }
    }
    const auto values =
        quantize_factor(series, read_quantization_config(inputs), grouping);
    auto result = make_frame(ordinal_index(values.size()));
    add_int64_column(result, "date",
                     std::vector<std::int64_t>(series.dates.begin(),
                                               series.dates.end()));
    add_string_column(result, "asset", series.assets);
    std::vector<int> quantiles;
    std::vector<bool> present;
    for (const auto& value : values) {
        quantiles.push_back(value.value_or(0));
        present.push_back(value.has_value());
    }
    add_int_column(result, "quantile", std::move(quantiles));
    add_bool_column(result, "has_quantile", std::move(present));
    return runtime_value(std::move(result));
}

// Factor inference ----------------------------------------------------------

runtime_value benjamini_hochberg_op(const runtime_inputs& inputs) {
    return runtime_value(fdr_frame(benjamini_hochberg(
        parse_double_list(inputs, "p_values", {}),
        optional_number(inputs, "alpha").value_or(0.05))));
}

runtime_value factor_model_inference_op(const runtime_inputs& inputs) {
    const auto data = parse_prepared(inputs, "factor_data", {});
    const auto model = analyze_factor(data, read_analysis_config(inputs));
    return runtime_value(ic_inference_frame(factor_model_inference(
        model, optional_number(inputs, "alpha").value_or(0.05))));
}

runtime_value fama_macbeth_op(const runtime_inputs& inputs) {
    const auto covariance = optional_string(inputs, "covariance", "iid");
    fama_macbeth_covariance mode = fama_macbeth_covariance::iid;
    if (covariance == "newey_west") {
        mode = fama_macbeth_covariance::newey_west;
    } else if (covariance != "iid") {
        throw input_error{"covariance must be 'iid' or 'newey_west'"};
    }
    return runtime_value(fama_macbeth_frame(fama_macbeth(
        require_frame(inputs, "returns"), require_frame(inputs, "exposures"),
        mode, optional_size(inputs, "newey_west_lags", 1U))));
}

runtime_value ic_confidence_interval_op(const runtime_inputs& inputs) {
    return runtime_value(ic_interval_frame(ic_confidence_interval(
        parse_double_list(inputs, "ic_series", {}),
        optional_number(inputs, "z").value_or(1.96))));
}

runtime_value ic_mean_op(const runtime_inputs& inputs) {
    return runtime_value(
        ic_mean(parse_double_list(inputs, "ic_series", {})));
}

runtime_value ic_t_stat_op(const runtime_inputs& inputs) {
    return runtime_value(
        ic_t_stat(parse_double_list(inputs, "ic_series", {})));
}

runtime_value information_coefficient_inference_op(
    const runtime_inputs& inputs) {
    return runtime_value(ic_inference_frame(information_coefficient_inference(
        parse_ic_table(inputs, "information_coefficient"),
        optional_number(inputs, "alpha").value_or(0.05))));
}

// Factor performance --------------------------------------------------------

runtime_value factor_information_coefficient_op(
    const runtime_inputs& inputs) {
    return runtime_value(ic_table_frame(factor_information_coefficient(
        parse_prepared(inputs, "factor_data", {}),
        optional_bool(inputs, "group_adjust", false),
        optional_bool(inputs, "by_group", false))));
}

runtime_value mean_information_coefficient_op(
    const runtime_inputs& inputs) {
    return runtime_value(ic_summary_frame(mean_information_coefficient(
        parse_prepared(inputs, "factor_data", {}),
        optional_bool(inputs, "group_adjust", false),
        optional_bool(inputs, "by_group", false))));
}

runtime_value factor_weights_op(const runtime_inputs& inputs) {
    return runtime_value(weights_frame(factor_weights(
        parse_prepared(inputs, "factor_data", {}),
        optional_bool(inputs, "demeaned", true),
        optional_bool(inputs, "group_adjust", false),
        optional_bool(inputs, "equal_weight", false))));
}

runtime_value factor_returns_op(const runtime_inputs& inputs) {
    const auto result = factor_returns(
        parse_prepared(inputs, "factor_data", {}),
        optional_bool(inputs, "demeaned", true),
        optional_bool(inputs, "group_adjust", false),
        optional_bool(inputs, "equal_weight", false));
    auto frame = make_frame(result.dates);
    for (std::size_t label = 0U; label < result.labels.size(); ++label) {
        std::vector<double> values;
        values.reserve(result.dates.size());
        for (std::size_t date = 0U; date < result.dates.size(); ++date) {
            values.push_back(
                result.values[date * result.labels.size() + label]);
        }
        add_double_column(frame, result.labels[label], std::move(values));
    }
    return runtime_value(std::move(frame));
}

runtime_value factor_alpha_beta_op(const runtime_inputs& inputs) {
    return runtime_value(alpha_beta_frame(factor_alpha_beta(
        parse_prepared(inputs, "factor_data", {}),
        optional_bool(inputs, "demeaned", true),
        optional_bool(inputs, "group_adjust", false),
        optional_bool(inputs, "equal_weight", false))));
}

runtime_value quantile_turnover_op(const runtime_inputs& inputs) {
    return runtime_value(series_frame(quantile_turnover(
        parse_prepared(inputs, "factor_data", {}),
        require_int(inputs, "quantile"),
        optional_int(inputs, "period", 1))));
}

runtime_value factor_rank_autocorrelation_op(
    const runtime_inputs& inputs) {
    return runtime_value(series_frame(factor_rank_autocorrelation(
        parse_prepared(inputs, "factor_data", {}),
        optional_int(inputs, "period", 1))));
}

runtime_value cumulative_returns_op(const runtime_inputs& inputs) {
    const auto& frame = require_frame(inputs, "returns");
    auto result = make_frame(std::vector<timestamp>(frame.get_index().begin(),
                                                    frame.get_index().end()));
    std::size_t columns = 0U;
    for (const auto& info : frame.get_columns_info<double>()) {
        const std::string column{std::get<0>(info).c_str()};
        const auto compounded = cumulative_returns(frame, 1.0, column);
        add_double_column(result, column,
                          double_values(compounded,
                                        std::string{returns_column}));
        ++columns;
    }
    if (columns == 0U) {
        throw input_error{"cumulative_returns requires a double column"};
    }
    return runtime_value(std::move(result));
}

runtime_value mean_return_by_quantile_op(const runtime_inputs& inputs) {
    const auto data = parse_prepared(inputs, "factor_data", {});
    if (optional_bool(inputs, "by_date", false)) {
        return runtime_value(quantile_date_frame(
            mean_return_by_quantile_by_date(
                data, optional_bool(inputs, "demeaned", true),
                optional_bool(inputs, "group_adjust", false),
                optional_bool(inputs, "by_group", false))));
    }
    return runtime_value(quantile_return_frame(mean_return_by_quantile(
        data, false, optional_bool(inputs, "by_group", false),
        optional_bool(inputs, "demeaned", true),
        optional_bool(inputs, "group_adjust", false))));
}

runtime_value mean_returns_spread_op(const runtime_inputs& inputs) {
    auto table = parse_quantile_table(inputs, "mean_returns");
    apply_standard_errors(inputs, table);
    return runtime_value(quantile_spread_frame(mean_return_spread(
        table, require_int(inputs, "upper_quant"),
        require_int(inputs, "lower_quant"))));
}

runtime_value common_start_returns_op(const runtime_inputs& inputs) {
    const auto factor = parse_factor_series(inputs, "factor");
    const auto* demean_frame = optional_frame(inputs, "demean_by");
    std::optional<factor_series> demean_by;
    if (demean_frame != nullptr) {
        demean_by = factor_series{};
        demean_by->dates = frame_dates(*demean_frame);
        const auto& assets = require_string_column(*demean_frame, "asset");
        const std::string value_column =
            demean_frame->has_column("value") ? "value" : "factor";
        demean_by->values = double_values(*demean_frame, value_column);
        demean_by->assets.assign(assets.begin(), assets.end());
    }
    return runtime_value(event_window_frame(common_start_returns(
        factor, require_frame(inputs, "returns"),
        require_int(inputs, "before"), require_int(inputs, "after"),
        optional_bool(inputs, "cumulative", false),
        optional_bool(inputs, "mean_by_date", false),
        demean_by.has_value() ? &*demean_by : nullptr)));
}

runtime_value average_cumulative_return_by_quantile_op(
    const runtime_inputs& inputs) {
    return runtime_value(quantile_event_frame(
        average_cumulative_return_by_quantile(
            parse_prepared(inputs, "factor_data", {}),
            require_frame(inputs, "returns"),
            optional_int(inputs, "periods_before", 10),
            optional_int(inputs, "periods_after", 15),
            optional_bool(inputs, "demeaned", true))));
}

// Factor PIT ----------------------------------------------------------------

runtime_value materialize_pit_factor_op(const runtime_inputs& inputs) {
    return runtime_value(factor_series_frame(materialize_pit_factor(
        parse_pit_observations(inputs, "observations"),
        parse_timestamp_list(inputs, "evaluation_dates", true))));
}

runtime_value validate_pit_alignment_op(const runtime_inputs& inputs) {
    validate_pit_alignment(parse_pit_points(inputs, "points"));
    return runtime_value(true);
}

// Factor portfolio ----------------------------------------------------------

runtime_value factor_positions_op(const runtime_inputs& inputs) {
    const auto period = require_string(inputs, "period");
    const auto* weights = optional_frame(inputs, "weights");
    std::optional<std::vector<factor_weight_row>> provided;
    if (weights != nullptr) {
        provided = parse_weight_rows(inputs, "weights");
    }
    return runtime_value(weights_frame(factor_positions(
        parse_prepared(inputs, "factor_data", period), period,
        optional_bool(inputs, "long_short", true),
        optional_bool(inputs, "group_neutral", false),
        optional_bool(inputs, "equal_weight", false),
        parse_int_list(inputs, "quantiles", {}),
        parse_string_list(inputs, "groups", false), std::move(provided))));
}

runtime_value factor_cumulative_returns_op(const runtime_inputs& inputs) {
    const auto period = require_string(inputs, "period");
    return runtime_value(series_frame(factor_cumulative_returns(
        parse_prepared(inputs, "factor_data", period), period,
        optional_bool(inputs, "long_short", true),
        optional_bool(inputs, "group_neutral", false),
        optional_bool(inputs, "equal_weight", false),
        parse_int_list(inputs, "quantiles", {}),
        parse_string_list(inputs, "groups", false))));
}

runtime_value build_factor_portfolio_inputs_op(
    const runtime_inputs& inputs) {
    const auto period = require_string(inputs, "period");
    const auto result = build_factor_portfolio_inputs(
        parse_prepared(inputs, "factor_data", period), period,
        optional_bool(inputs, "long_short", true),
        optional_bool(inputs, "group_neutral", false),
        optional_bool(inputs, "equal_weight", false),
        parse_int_list(inputs, "quantiles", {}),
        parse_string_list(inputs, "groups", false));
    auto document = make_document("factor_analysis",
                                  "Factor Portfolio Inputs");
    auto& section =
        add_section(document, "portfolio_inputs", "Portfolio Inputs");
    auto returns = make_frame(result.returns.dates);
    add_double_column(returns, "value", result.returns.values);
    add_table(section, "returns", "Daily Returns", returns);
    add_table(section, "positions", "Positions",
              weights_frame(result.positions));
    if (result.benchmark_returns.has_value()) {
        auto benchmark = make_frame(result.benchmark_returns->dates);
        add_double_column(benchmark, "value",
                          result.benchmark_returns->values);
        add_table(section, "benchmark_returns", "Benchmark Returns",
                  benchmark);
    }
    return runtime_value(std::move(document));
}

runtime_value positions_op(const runtime_inputs& inputs) {
    const auto period = require_string(inputs, "period");
    auto weights = parse_weight_rows(inputs, "weights");
    prepared_factor_data data;
    data.forward_labels = {period};
    for (const auto& row : weights) {
        factor_row value;
        value.date = row.date;
        value.asset = row.asset;
        value.factor = row.weight;
        value.quantile = 0;
        value.forward_returns = {0.0};
        data.rows.push_back(std::move(value));
    }
    return runtime_value(weights_frame(factor_positions(
        data, period, true, false, false, {}, {}, weights)));
}

// Optimization --------------------------------------------------------------

runtime_value efficient_frontier_op(const runtime_inputs& inputs) {
    frontier_config config;
    config.n_points = optional_size(inputs, "n_points", 50U);
    config.risk_free_rate =
        optional_number(inputs, "risk_free_rate").value_or(0.0);
    config.short_allowed = optional_bool(inputs, "short_allowed", false);
    config.max_weight = optional_number(inputs, "max_weight").value_or(1.0);
    config.periods_per_year =
        optional_number(inputs, "periods_per_year").value_or(252.0);
    const auto result =
        efficient_frontier(require_frame(inputs, "returns"), config);
    auto document = make_document("optimization", "Efficient Frontier");
    auto& section = add_section(document, "frontier", "Frontier");
    auto points = make_frame(ordinal_index(result.frontier_returns.size()));
    add_int_column(points, "point",
                   [&result] {
                       std::vector<int> values;
                       values.reserve(result.frontier_returns.size());
                       for (std::size_t point = 0U;
                            point < result.frontier_returns.size(); ++point) {
                           values.push_back(static_cast<int>(point));
                       }
                       return values;
                   }());
    add_double_column(points, "expected_return", result.frontier_returns);
    add_double_column(points, "volatility", result.frontier_volatilities);
    add_double_column(points, "sharpe", result.frontier_sharpe);
    for (std::size_t asset = 0U; asset < result.asset_names.size(); ++asset) {
        std::vector<double> weights;
        weights.reserve(result.frontier_weights.size());
        for (const auto& point : result.frontier_weights) {
            weights.push_back(asset < point.size() ? point[asset]
                                                   : std::numeric_limits<
                                                         double>::quiet_NaN());
        }
        add_double_column(points, result.asset_names[asset],
                          std::move(weights));
    }
    add_table(section, "frontier_points", "Frontier Points", points);
    add_number_metric(section, "min_variance_return",
                      "Minimum variance return", "return",
                      result.min_variance.expected_return);
    add_number_metric(section, "min_variance_volatility",
                      "Minimum variance volatility", "volatility",
                      result.min_variance.volatility);
    add_number_metric(section, "max_sharpe_return", "Maximum Sharpe return",
                      "return", result.max_sharpe.expected_return);
    add_number_metric(section, "max_sharpe_volatility",
                      "Maximum Sharpe volatility", "volatility",
                      result.max_sharpe.volatility);
    add_number_metric(section, "max_sharpe_ratio", "Maximum Sharpe ratio",
                      "ratio", result.max_sharpe_ratio);
    add_number_metric(section, "periods_per_year", "Periods per year",
                      "count", result.periods_per_year);
    return runtime_value(std::move(document));
}

runtime_value optimize_op(const runtime_inputs& inputs) {
    portfolio_optimization_config config;
    const auto objective =
        optional_string(inputs, "objective", "max_sharpe");
    if (objective == "max_sharpe") {
        config.objective = portfolio_objective::max_sharpe;
    } else if (objective == "min_variance") {
        config.objective = portfolio_objective::min_variance;
    } else if (objective == "target_return") {
        config.objective = portfolio_objective::target_return;
    } else if (objective == "target_risk") {
        config.objective = portfolio_objective::target_risk;
    } else {
        throw input_error{"unsupported portfolio objective: " + objective};
    }
    config.risk_free_rate =
        optional_number(inputs, "risk_free_rate").value_or(0.0);
    config.target_return = optional_number(inputs, "target_return");
    config.target_volatility = optional_number(inputs, "target_volatility");
    config.short_allowed = optional_bool(inputs, "short_allowed", false);
    config.max_weight = optional_number(inputs, "max_weight").value_or(1.0);
    config.min_weight = optional_number(inputs, "min_weight");
    const auto* constraints = optional_frame(inputs, "sector_constraints");
    if (constraints != nullptr) {
        const auto& sectors = require_string_column(*constraints, "sector");
        const auto lower = double_values(*constraints, "lower");
        const auto upper = double_values(*constraints, "upper");
        for (std::size_t row = 0U; row < sectors.size(); ++row) {
            config.sector_constraints.push_back(
                {sectors[row], lower[row], upper[row]});
        }
    }
    const auto* sector_map = optional_frame(inputs, "sector_map");
    if (sector_map != nullptr) {
        const auto& assets = require_string_column(*sector_map, "asset");
        const auto& sectors = require_string_column(*sector_map, "sector");
        for (std::size_t row = 0U; row < assets.size(); ++row) {
            config.sector_map.emplace(assets[row], sectors[row]);
        }
    }
    const auto result =
        optimize_portfolio(require_frame(inputs, "returns"), config);
    auto document = make_document("optimization", "Portfolio Optimization");
    auto& section = add_section(document, "optimization", "Optimization");
    add_text_metric(section, "objective", "Objective", objective);
    add_number_metric(section, "expected_return", "Expected return",
                      "return", result.expected_return);
    add_number_metric(section, "volatility", "Volatility", "volatility",
                      result.volatility);
    add_number_metric(section, "sharpe", "Sharpe ratio", "ratio",
                      result.sharpe);
    auto weights = make_frame(ordinal_index(result.weights.size()));
    add_string_column(weights, "asset", result.asset_names);
    add_double_column(weights, "weight", result.weights);
    add_table(section, "weights", "Weights", weights);
    return runtime_value(std::move(document));
}

runtime_value risk_parity_op(const runtime_inputs& inputs) {
    std::optional<std::vector<double>> budget;
    if (inputs.find("risk_budget") != inputs.end()) {
        budget = parse_double_list(inputs, "risk_budget");
    }
    const auto result = risk_parity(
        require_frame(inputs, "returns"), budget,
        optional_size(inputs, "max_iter", 1000U));
    auto document = make_document("optimization", "Risk Parity");
    auto& section = add_section(document, "risk_parity", "Risk Parity");
    add_number_metric(section, "volatility", "Volatility", "volatility",
                      result.volatility);
    auto weights = make_frame(ordinal_index(result.weights.size()));
    add_string_column(weights, "asset", result.asset_names);
    add_double_column(weights, "weight", result.weights);
    add_double_column(weights, "risk_contribution",
                      result.risk_contributions);
    add_table(section, "weights", "Weights", weights);
    return runtime_value(std::move(document));
}

// Report and data -----------------------------------------------------------

report_document analysis_report(const runtime_inputs& inputs,
                                const std::string& title) {
    const auto data = parse_prepared(inputs, "factor_data", {});
    const auto config = read_analysis_config(inputs);
    const auto* events = optional_frame(inputs, "event_returns");
    if (events == nullptr) {
        return build_factor_report(analyze_factor(data, config), title);
    }
    return build_factor_report(
        analyze_factor(data, config, *events, require_int(inputs, "event_before"),
                       require_int(inputs, "event_after")),
        title);
}

runtime_value analyze_factor_op(const runtime_inputs& inputs) {
    return runtime_value(analysis_report(inputs, "Factor Analysis Report"));
}

runtime_value build_factor_report_op(const runtime_inputs& inputs) {
    return runtime_value(analysis_report(
        inputs, optional_string(inputs, "title", "Factor Analysis Report")));
}

runtime_value data_snapshot_from_frame_op(const runtime_inputs& inputs) {
    const auto snapshot = data_snapshot::from_frame(
        require_frame(inputs, "frame"), require_string(inputs, "provider"),
        require_string(inputs, "requested_start"),
        require_string(inputs, "requested_end"), require_string(inputs, "as_of"),
        optional_string(inputs, "timezone", "UTC"),
        optional_string(inputs, "price_adjustment", "raw"));
    return runtime_value(snapshot.to_manifest());
}

// ---------------------------------------------------------------------------
// Schema/provenance helpers.
// ---------------------------------------------------------------------------

std::string json_string(const std::string& value) {
    std::string result{"\""};
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    result.push_back('"');
    return result;
}

using schema_entry = std::pair<std::string, std::string>;

std::string input_schema(
    const std::initializer_list<schema_entry>& required,
    const std::initializer_list<schema_entry>& optional = {}) {
    std::string result{"{\"required\":{"};
    bool first = true;
    for (const auto& [key, type] : required) {
        if (!first) {
            result.push_back(',');
        }
        first = false;
        result += json_string(key);
        result.push_back(':');
        result += json_string(type);
    }
    result += "},\"optional\":{";
    first = true;
    for (const auto& [key, type] : optional) {
        if (!first) {
            result.push_back(',');
        }
        first = false;
        result += json_string(key);
        result.push_back(':');
        result += json_string(type);
    }
    result += "}}";
    return result;
}

}  // namespace

std::vector<operation_spec> attribution_factor_operations() {
    std::vector<operation_spec> operations;
    operations.reserve(63U);
    const auto add = [&operations](const std::string& operation_id,
                                   const std::string& capability_id,
                                   const std::string& kernel_symbol,
                                   const std::string& output_projection,
                                   const std::string& schema,
                                   std::function<runtime_value(
                                       const runtime_inputs&)>
                                       callable) {
        const std::string domain =
            operation_id.substr(0U, operation_id.find('.'));
        operations.push_back(
            {operation_id, capability_id, domain, std::move(callable), true,
             "none", std::nullopt, std::nullopt,
             {{"owner", domain},
              {"kernel_symbol", kernel_symbol},
              {"source_operation_id", operation_id},
              {"input_schema", schema},
              {"output_projection", output_projection}}});
    };

    // attribution (17) -----------------------------------------------------
    add("attribution.brinson.brinson_attribution",
        "fincore.attribution.brinson.brinson_attribution",
        "fincore::brinson_attribution", "dataframe[1 x 6]",
        input_schema({{"portfolio_returns", "dataframe"},
                      {"benchmark_returns", "dataframe"},
                      {"portfolio_weights", "dataframe"},
                      {"benchmark_weights", "dataframe"}},
                     {{"sector_columns", "dataframe[string-list]"}}),
        brinson_attribution_op);
    add("attribution.brinson.brinson_cumulative",
        "fincore.attribution.brinson.brinson_cumulative",
        "fincore::brinson_cumulative", "dataframe[1 x 6]",
        input_schema({{"portfolio_returns", "dataframe"},
                      {"benchmark_returns", "dataframe"},
                      {"portfolio_weights", "dataframe"},
                      {"benchmark_weights", "dataframe"}},
                     {{"sector_columns", "dataframe[string-list]"}}),
        brinson_cumulative_op);
    add("attribution.brinson.brinson_results",
        "fincore.attribution.brinson.brinson_results",
        "fincore::brinson_results", "dataframe",
        input_schema({{"portfolio_returns", "dataframe"},
                      {"benchmark_returns", "dataframe"},
                      {"portfolio_weights", "dataframe"},
                      {"benchmark_weights", "dataframe"}},
                     {{"sector_columns", "dataframe[string-list]"},
                      {"periods", "dataframe[string-list]"}}),
        brinson_results_op);
    add("attribution.fama_french.calculate_idiosyncratic_risk",
        "fincore.attribution.fama_french.calculate_idiosyncratic_risk",
        "fincore::calculate_idiosyncratic_risk", "dataframe",
        input_schema({{"returns", "dataframe"},
                      {"factor_data", "dataframe"}},
                     {{"market_beta", "double"}, {"risk_free", "double"}}),
        idiosyncratic_risk_op);
    add("attribution.fama_french.fetch_ff_factors",
        "fincore.attribution.fama_french.fetch_ff_factors",
        "fincore::fetch_ff_factors", "dataframe",
        input_schema({{"start", "string"},
                      {"end", "string"},
                      {"provider", "dataframe"}},
                     {{"library", "string"}, {"copy", "boolean"}}),
        fetch_ff_factors_op);
    add("attribution.performance.align_and_warn",
        "fincore.attribution.performance.align_and_warn",
        "fincore::align_and_warn", "report_document",
        input_schema({{"returns", "dataframe"},
                      {"positions", "dataframe"},
                      {"factor_returns", "dataframe"},
                      {"factor_loadings", "dataframe"}}),
        align_and_warn_op);
    add("attribution.performance.compute_exposures",
        "fincore.attribution.performance.compute_exposures",
        "fincore::compute_exposures", "dataframe",
        input_schema({{"positions", "dataframe"},
                      {"factor_loadings", "dataframe"}},
                     {{"stack_positions", "boolean"},
                      {"pos_in_dollars", "boolean"},
                      {"cash_column_name", "string"},
                      {"symbol_column_name", "string"}}),
        compute_exposures_op);
    add("attribution.performance.compute_exposures_internal",
        "fincore.attribution.performance.compute_exposures_internal",
        "fincore::compute_exposures", "dataframe",
        input_schema({{"positions", "dataframe"},
                      {"factor_loadings", "dataframe"}},
                     {{"cash_column_name", "string"},
                      {"symbol_column_name", "string"}}),
        [](const runtime_inputs& inputs) {
            return compute_exposures_impl(inputs, false);
        });
    add("attribution.performance.create_perf_attrib_stats",
        "fincore.attribution.performance.create_perf_attrib_stats",
        "fincore::create_perf_attrib_stats", "report_document",
        input_schema({{"perf_attrib", "dataframe"},
                      {"risk_exposures", "dataframe"}},
                     {{"annualization", "double"}}),
        create_perf_attrib_stats_op);
    add("attribution.performance.cumulative_returns_less_costs",
        "fincore.attribution.performance.cumulative_returns_less_costs",
        "fincore::cumulative_returns_less_costs", "dataframe",
        input_schema({{"returns", "dataframe"}},
                     {{"costs", "dataframe"},
                      {"returns_column_name", "string"},
                      {"costs_column_name", "string"}}),
        cumulative_returns_less_costs_op);
    add("attribution.performance.perf_attrib",
        "fincore.attribution.performance.perf_attrib",
        "fincore::perf_attrib", "report_document",
        input_schema({{"returns", "dataframe"},
                      {"positions", "dataframe"},
                      {"factor_returns", "dataframe"},
                      {"factor_loadings", "dataframe"}},
                     {{"positions_in_dollars", "boolean"},
                      {"returns_column_name", "string"},
                      {"cash_column_name", "string"},
                      {"symbol_column_name", "string"}}),
        [](const runtime_inputs& inputs) {
            return perf_attrib_impl(inputs, true);
        });
    add("attribution.performance.perf_attrib_core",
        "fincore.attribution.performance.perf_attrib_core",
        "fincore::perf_attrib_core", "report_document",
        input_schema({{"returns", "dataframe"},
                      {"positions", "dataframe"},
                      {"factor_returns", "dataframe"},
                      {"factor_loadings", "dataframe"}},
                     {{"returns_column_name", "string"},
                      {"cash_column_name", "string"},
                      {"symbol_column_name", "string"}}),
        [](const runtime_inputs& inputs) {
            return perf_attrib_impl(inputs, false);
        });
    add("attribution.style.analyze_performance_by_style",
        "fincore.attribution.style.analyze_performance_by_style",
        "fincore::analyze_performance_by_style", "dataframe",
        input_schema({{"returns", "dataframe"}},
                     {{"style_exposures", "dataframe"}}),
        analyze_performance_by_style_op);
    add("attribution.style.calculate_regression_attribution",
        "fincore.attribution.style.calculate_regression_attribution",
        "fincore::calculate_regression_attribution", "dataframe",
        input_schema({{"portfolio_returns", "dataframe"}},
                     {{"style_returns", "dataframe"},
                      {"selected_styles", "dataframe[string-list]"}}),
        calculate_regression_attribution_op);
    add("attribution.style.calculate_style_tilts",
        "fincore.attribution.style.calculate_style_tilts",
        "fincore::calculate_style_tilts", "dataframe",
        input_schema({{"returns", "dataframe"}},
                     {{"window", "integer"}}),
        calculate_style_tilts_op);
    add("attribution.style.fetch_style_factors",
        "fincore.attribution.style.fetch_style_factors",
        "fincore::fetch_style_factors", "dataframe",
        input_schema({{"tickers", "dataframe[string-list]"},
                      {"provider", "dataframe"}},
                     {{"factors", "dataframe[string-list]"},
                      {"library", "string"}}),
        fetch_style_factors_op);
    add("attribution.style.style_analysis",
        "fincore.attribution.style.style_analysis",
        "fincore::style_analysis", "report_document",
        input_schema({{"returns", "dataframe"}},
                     {{"market_caps", "dataframe[double-list]"},
                      {"book_to_price", "dataframe[double-list]"},
                      {"value_scores", "dataframe[double-list]"},
                      {"momentum_window", "integer"},
                      {"size_quantiles", "dataframe[double-list]"}}),
        style_analysis_op);

    // factor_analysis (41) ------------------------------------------------
    add("factor_analysis.analysis.analyze_factor",
        "fincore.factor_analysis.analysis.analyze_factor",
        "fincore::analyze_factor", "report_document",
        input_schema({{"factor_data", "dataframe"}},
                     {{"long_short", "boolean"},
                      {"group_neutral", "boolean"},
                      {"equal_weight", "boolean"},
                      {"by_group", "boolean"},
                      {"periods", "dataframe[string-list]"},
                      {"turnover_periods", "dataframe[int-list]"},
                      {"time_aggregation", "dataframe[string-list]"},
                      {"include_portfolio_inputs", "boolean"},
                      {"portfolio_benchmark_period", "string"},
                      {"event_returns", "dataframe"},
                      {"event_before", "integer"},
                      {"event_after", "integer"},
                      {"forward_labels", "dataframe[string-list]"}}),
        analyze_factor_op);
    add("factor_analysis.calendar.add_custom_calendar_timedelta",
        "fincore.factor_analysis.calendar.add_custom_calendar_timedelta",
        "fincore::add_custom_calendar_timedelta", "dataframe|integer",
        input_schema({{"input", "dataframe|integer"},
                      {"timedelta", "integer|string"}},
                     {{"factor_dates", "dataframe[timestamp-list]"},
                      {"price_dates", "dataframe[timestamp-list]"}}),
        add_calendar_timedelta_op);
    add("factor_analysis.calendar.backshift_returns_series",
        "fincore.factor_analysis.calendar.backshift_returns_series",
        "fincore::backshift_returns_series", "dataframe",
        input_schema({{"series", "dataframe"}, {"N", "integer"}},
                     {{"date_levels", "dataframe[timestamp-list]"}}),
        backshift_returns_series_op);
    add("factor_analysis.calendar.diff_custom_calendar_timedeltas",
        "fincore.factor_analysis.calendar.diff_custom_calendar_timedeltas",
        "fincore::diff_custom_calendar_timedeltas", "integer",
        input_schema({{"start", "integer"}, {"end", "integer"}},
                     {{"factor_dates", "dataframe[timestamp-list]"},
                      {"price_dates", "dataframe[timestamp-list]"}}),
        diff_calendar_timedeltas_op);
    add("factor_analysis.calendar.get_forward_returns_columns",
        "fincore.factor_analysis.calendar.get_forward_returns_columns",
        "fincore::get_forward_returns_columns", "dataframe",
        input_schema({{"columns", "dataframe[string-list]"}},
                     {{"require_exact_day_multiple", "boolean"}}),
        get_forward_returns_columns_op);
    add("factor_analysis.calendar.infer_trading_calendar",
        "fincore.factor_analysis.calendar.infer_trading_calendar",
        "fincore::infer_trading_calendar", "dataframe",
        input_schema({{"factor_idx", "dataframe[timestamp-list]"},
                      {"prices_idx", "dataframe[timestamp-list]"}}),
        infer_trading_calendar_op);
    add("factor_analysis.calendar.timedelta_strings_to_integers",
        "fincore.factor_analysis.calendar.timedelta_strings_to_integers",
        "fincore::timedelta_strings_to_integers", "dataframe",
        input_schema({{"sequence", "dataframe[string-list]"}}),
        timedelta_strings_to_integers_op);
    add("factor_analysis.calendar.timedelta_to_string",
        "fincore.factor_analysis.calendar.timedelta_to_string",
        "fincore::timedelta_to_string", "string",
        input_schema({{"timedelta", "integer|string"}}),
        timedelta_to_string_op);
    add("factor_analysis.costs.apply_factor_costs",
        "fincore.factor_analysis.costs.apply_factor_costs",
        "fincore::apply_factor_costs", "report_document",
        input_schema({{"gross_returns", "dataframe[double-list]"},
                      {"weights", "dataframe"},
                      {"dollar_volume", "dataframe"},
                      {"portfolio_value", "double"}},
                     {{"half_spread_bps", "double"},
                      {"impact_coefficient", "double"},
                      {"max_participation", "double"},
                      {"impact_exponent", "double"},
                      {"borrow_rates", "dataframe"},
                      {"borrow_available", "dataframe"}}),
        apply_factor_costs_op);
    add("factor_analysis.costs.estimate_factor_capacity",
        "fincore.factor_analysis.costs.estimate_factor_capacity",
        "fincore::estimate_factor_capacity", "report_document",
        input_schema({{"weights", "dataframe"},
                      {"dollar_volume", "dataframe"},
                      {"max_participation", "double"}}),
        estimate_factor_capacity_op);
    add("factor_analysis.data.compute_forward_returns",
        "fincore.factor_analysis.data.compute_forward_returns",
        "fincore::compute_forward_returns", "dataframe",
        input_schema({{"factor", "dataframe"}, {"prices", "dataframe"}},
                     {{"periods", "dataframe[int-list]"},
                      {"filter_zscore", "double"},
                      {"cumulative_returns", "boolean"}}),
        compute_forward_returns_op);
    add("factor_analysis.data.prepare_factor_data",
        "fincore.factor_analysis.data.prepare_factor_data",
        "fincore::prepare_factor_data", "dataframe",
        input_schema({{"factor", "dataframe"}, {"prices", "dataframe"}},
                     {{"groupby", "dataframe"},
                      {"quantiles", "integer"},
                      {"bins", "dataframe[double-list]"},
                      {"periods", "dataframe[int-list]"},
                      {"max_loss", "double"},
                      {"binning_by_group", "boolean"},
                      {"filter_zscore", "double"},
                      {"groupby_labels", "dataframe"},
                      {"zero_aware", "boolean"},
                      {"cumulative_returns", "boolean"}}),
        prepare_factor_data_op);
    add("factor_analysis.data.prepare_factor_data_by_horizon",
        "fincore.factor_analysis.data.prepare_factor_data_by_horizon",
        "fincore::prepare_factor_data_by_horizon", "dataframe",
        input_schema({{"factor", "dataframe"}, {"prices", "dataframe"}},
                     {{"groupby", "dataframe"},
                      {"quantiles", "integer"},
                      {"bins", "dataframe[double-list]"},
                      {"periods", "dataframe[int-list]"},
                      {"max_loss", "double"},
                      {"binning_by_group", "boolean"},
                      {"filter_zscore", "double"},
                      {"groupby_labels", "dataframe"},
                      {"zero_aware", "boolean"},
                      {"cumulative_returns", "boolean"}}),
        prepare_factor_data_by_horizon_op);
    add("factor_analysis.data.prepare_factor_data_from_forward_returns",
        "fincore.factor_analysis.data.prepare_factor_data_from_forward_returns",
        "fincore::prepare_factor_data_from_forward_returns", "dataframe",
        input_schema({{"factor", "dataframe"},
                      {"forward_returns", "dataframe"}},
                     {{"groupby", "dataframe"},
                      {"binning_by_group", "boolean"},
                      {"quantiles", "integer"},
                      {"bins", "dataframe[double-list]"},
                      {"groupby_labels", "dataframe"},
                      {"max_loss", "double"},
                      {"zero_aware", "boolean"}}),
        prepare_factor_data_from_forward_returns_op);
    add("factor_analysis.data.prepare_pit_factor_data",
        "fincore.factor_analysis.data.prepare_pit_factor_data",
        "fincore::prepare_pit_factor_data", "dataframe",
        input_schema({{"observations", "dataframe"},
                      {"prices", "dataframe"},
                      {"evaluation_dates", "dataframe[timestamp-list]"}},
                     {{"groupby", "dataframe"},
                      {"quantiles", "integer"},
                      {"bins", "dataframe[double-list]"},
                      {"periods", "dataframe[int-list]"},
                      {"max_loss", "double"},
                      {"binning_by_group", "boolean"},
                      {"filter_zscore", "double"},
                      {"groupby_labels", "dataframe"},
                      {"zero_aware", "boolean"},
                      {"cumulative_returns", "boolean"}}),
        prepare_pit_factor_data_op);
    add("factor_analysis.data.quantize_factor",
        "fincore.factor_analysis.data.quantize_factor",
        "fincore::quantize_factor", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"quantiles", "integer"},
                      {"bins", "dataframe[double-list]"},
                      {"by_group", "boolean"},
                      {"no_raise", "boolean"},
                      {"zero_aware", "boolean"}}),
        quantize_factor_op);
    add("factor_analysis.inference.benjamini_hochberg",
        "fincore.factor_analysis.inference.benjamini_hochberg",
        "fincore::benjamini_hochberg", "dataframe",
        input_schema({{"p_values", "dataframe[double-list]"}},
                     {{"alpha", "double"}}),
        benjamini_hochberg_op);
    add("factor_analysis.inference.factor_model_inference",
        "fincore.factor_analysis.inference.factor_model_inference",
        "fincore::factor_model_inference", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"long_short", "boolean"},
                      {"group_neutral", "boolean"},
                      {"equal_weight", "boolean"},
                      {"by_group", "boolean"},
                      {"periods", "dataframe[string-list]"},
                      {"turnover_periods", "dataframe[int-list]"},
                      {"time_aggregation", "dataframe[string-list]"},
                      {"include_portfolio_inputs", "boolean"},
                      {"portfolio_benchmark_period", "string"},
                      {"forward_labels", "dataframe[string-list]"},
                      {"alpha", "double"}}),
        factor_model_inference_op);
    add("factor_analysis.inference.fama_macbeth",
        "fincore.factor_analysis.inference.fama_macbeth",
        "fincore::fama_macbeth", "dataframe",
        input_schema({{"returns", "dataframe"},
                      {"exposures", "dataframe"}},
                     {{"covariance", "string"},
                      {"newey_west_lags", "integer"}}),
        fama_macbeth_op);
    add("factor_analysis.inference.ic_confidence_interval",
        "fincore.factor_analysis.inference.ic_confidence_interval",
        "fincore::ic_confidence_interval", "dataframe[1 x 2]",
        input_schema({{"ic_series", "dataframe[double-list]"}},
                     {{"z", "double"}}),
        ic_confidence_interval_op);
    add("factor_analysis.inference.ic_mean",
        "fincore.factor_analysis.inference.ic_mean",
        "fincore::ic_mean", "double",
        input_schema({{"ic_series", "dataframe[double-list]"}}),
        ic_mean_op);
    add("factor_analysis.inference.ic_t_stat",
        "fincore.factor_analysis.inference.ic_t_stat",
        "fincore::ic_t_stat", "double",
        input_schema({{"ic_series", "dataframe[double-list]"}}),
        ic_t_stat_op);
    add("factor_analysis.inference.information_coefficient_inference",
        "fincore.factor_analysis.inference.information_coefficient_inference",
        "fincore::information_coefficient_inference", "dataframe",
        input_schema({{"information_coefficient", "dataframe"}},
                     {{"alpha", "double"}}),
        information_coefficient_inference_op);
    add("factor_analysis.performance.average_cumulative_return_by_quantile",
        "fincore.factor_analysis.performance.average_cumulative_return_by_quantile",
        "fincore::average_cumulative_return_by_quantile", "dataframe",
        input_schema({{"factor_data", "dataframe"},
                      {"returns", "dataframe"}},
                     {{"periods_before", "integer"},
                      {"periods_after", "integer"},
                      {"demeaned", "boolean"}}),
        average_cumulative_return_by_quantile_op);
    add("factor_analysis.performance.common_start_returns",
        "fincore.factor_analysis.performance.common_start_returns",
        "fincore::common_start_returns", "dataframe",
        input_schema({{"factor", "dataframe"},
                      {"returns", "dataframe"},
                      {"before", "integer"},
                      {"after", "integer"}},
                     {{"cumulative", "boolean"},
                      {"mean_by_date", "boolean"},
                      {"demean_by", "dataframe"}}),
        common_start_returns_op);
    add("factor_analysis.performance.compute_mean_returns_spread",
        "fincore.factor_analysis.performance.compute_mean_returns_spread",
        "fincore::mean_return_spread", "dataframe",
        input_schema({{"mean_returns", "dataframe"},
                      {"upper_quant", "integer"},
                      {"lower_quant", "integer"}},
                     {{"std_err", "dataframe"}}),
        mean_returns_spread_op);
    add("factor_analysis.performance.cumulative_returns",
        "fincore.factor_analysis.performance.cumulative_returns",
        "fincore::cumulative_returns", "dataframe",
        input_schema({{"returns", "dataframe"}}),
        cumulative_returns_op);
    add("factor_analysis.performance.factor_alpha_beta",
        "fincore.factor_analysis.performance.factor_alpha_beta",
        "fincore::factor_alpha_beta", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"demeaned", "boolean"},
                      {"group_adjust", "boolean"},
                      {"equal_weight", "boolean"}}),
        factor_alpha_beta_op);
    add("factor_analysis.performance.factor_information_coefficient",
        "fincore.factor_analysis.performance.factor_information_coefficient",
        "fincore::factor_information_coefficient", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"group_adjust", "boolean"},
                      {"by_group", "boolean"}}),
        factor_information_coefficient_op);
    add("factor_analysis.performance.factor_rank_autocorrelation",
        "fincore.factor_analysis.performance.factor_rank_autocorrelation",
        "fincore::factor_rank_autocorrelation", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"period", "integer"}}),
        factor_rank_autocorrelation_op);
    add("factor_analysis.performance.factor_returns",
        "fincore.factor_analysis.performance.factor_returns",
        "fincore::factor_returns", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"demeaned", "boolean"},
                      {"group_adjust", "boolean"},
                      {"equal_weight", "boolean"}}),
        factor_returns_op);
    add("factor_analysis.performance.factor_weights",
        "fincore.factor_analysis.performance.factor_weights",
        "fincore::factor_weights", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"demeaned", "boolean"},
                      {"group_adjust", "boolean"},
                      {"equal_weight", "boolean"}}),
        factor_weights_op);
    add("factor_analysis.performance.mean_information_coefficient",
        "fincore.factor_analysis.performance.mean_information_coefficient",
        "fincore::mean_information_coefficient", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"group_adjust", "boolean"},
                      {"by_group", "boolean"}}),
        mean_information_coefficient_op);
    add("factor_analysis.performance.mean_return_by_quantile",
        "fincore.factor_analysis.performance.mean_return_by_quantile",
        "fincore::mean_return_by_quantile", "dataframe",
        input_schema({{"factor_data", "dataframe"}},
                     {{"by_date", "boolean"},
                      {"by_group", "boolean"},
                      {"demeaned", "boolean"},
                      {"group_adjust", "boolean"}}),
        mean_return_by_quantile_op);
    add("factor_analysis.performance.quantile_turnover",
        "fincore.factor_analysis.performance.quantile_turnover",
        "fincore::quantile_turnover", "dataframe",
        input_schema({{"factor_data", "dataframe"},
                      {"quantile", "integer"}},
                     {{"period", "integer"}}),
        quantile_turnover_op);
    add("factor_analysis.pit.materialize_pit_factor",
        "fincore.factor_analysis.pit.materialize_pit_factor",
        "fincore::materialize_pit_factor", "dataframe",
        input_schema({{"observations", "dataframe"},
                      {"evaluation_dates", "dataframe[timestamp-list]"}}),
        materialize_pit_factor_op);
    add("factor_analysis.pit.validate_pit_alignment",
        "fincore.factor_analysis.pit.validate_pit_alignment",
        "fincore::validate_pit_alignment", "boolean",
        input_schema({{"points", "dataframe"}}),
        validate_pit_alignment_op);
    add("factor_analysis.portfolio.build_factor_portfolio_inputs",
        "fincore.factor_analysis.portfolio.build_factor_portfolio_inputs",
        "fincore::build_factor_portfolio_inputs", "report_document",
        input_schema({{"factor_data", "dataframe"},
                      {"period", "string"}},
                     {{"long_short", "boolean"},
                      {"group_neutral", "boolean"},
                      {"equal_weight", "boolean"},
                      {"quantiles", "dataframe[int-list]"},
                      {"groups", "dataframe[string-list]"}}),
        build_factor_portfolio_inputs_op);
    add("factor_analysis.portfolio.factor_cumulative_returns",
        "fincore.factor_analysis.portfolio.factor_cumulative_returns",
        "fincore::factor_cumulative_returns", "dataframe",
        input_schema({{"factor_data", "dataframe"},
                      {"period", "string"}},
                     {{"long_short", "boolean"},
                      {"group_neutral", "boolean"},
                      {"equal_weight", "boolean"},
                      {"quantiles", "dataframe[int-list]"},
                      {"groups", "dataframe[string-list]"}}),
        factor_cumulative_returns_op);
    add("factor_analysis.portfolio.factor_positions",
        "fincore.factor_analysis.portfolio.factor_positions",
        "fincore::factor_positions", "dataframe",
        input_schema({{"factor_data", "dataframe"},
                      {"period", "string"}},
                     {{"long_short", "boolean"},
                      {"group_neutral", "boolean"},
                      {"equal_weight", "boolean"},
                      {"quantiles", "dataframe[int-list]"},
                      {"groups", "dataframe[string-list]"},
                      {"weights", "dataframe"}}),
        factor_positions_op);
    add("factor_analysis.portfolio.positions",
        "fincore.factor_analysis.portfolio.positions",
        "fincore::factor_positions", "dataframe",
        input_schema({{"weights", "dataframe"},
                      {"period", "string"}}),
        positions_op);

    // optimization (3) ----------------------------------------------------
    add("optimization.frontier.efficient_frontier",
        "fincore.optimization.frontier.efficient_frontier",
        "fincore::efficient_frontier", "report_document",
        input_schema({{"returns", "dataframe"}},
                     {{"n_points", "integer"},
                      {"risk_free_rate", "double"},
                      {"short_allowed", "boolean"},
                      {"max_weight", "double"},
                      {"periods_per_year", "double"}}),
        efficient_frontier_op);
    add("optimization.objectives.optimize",
        "fincore.optimization.objectives.optimize",
        "fincore::optimize_portfolio", "report_document",
        input_schema({{"returns", "dataframe"}},
                     {{"objective", "string"},
                      {"risk_free_rate", "double"},
                      {"target_return", "double"},
                      {"target_volatility", "double"},
                      {"short_allowed", "boolean"},
                      {"max_weight", "double"},
                      {"min_weight", "double"},
                      {"sector_constraints", "dataframe"},
                      {"sector_map", "dataframe"}}),
        optimize_op);
    add("optimization.risk_parity.risk_parity",
        "fincore.optimization.risk_parity.risk_parity",
        "fincore::risk_parity", "report_document",
        input_schema({{"returns", "dataframe"}},
                     {{"risk_budget", "dataframe[double-list]"},
                      {"max_iter", "integer"}}),
        risk_parity_op);

    // report (1) ----------------------------------------------------------
    add("report.factor.build_factor_report",
        "fincore.report.factor.compute.build_factor_report",
        "fincore::build_factor_report", "report_document",
        input_schema({{"factor_data", "dataframe"}},
                     {{"title", "string"},
                      {"long_short", "boolean"},
                      {"group_neutral", "boolean"},
                      {"equal_weight", "boolean"},
                      {"by_group", "boolean"},
                      {"periods", "dataframe[string-list]"},
                      {"turnover_periods", "dataframe[int-list]"},
                      {"time_aggregation", "dataframe[string-list]"},
                      {"include_portfolio_inputs", "boolean"},
                      {"portfolio_benchmark_period", "string"},
                      {"event_returns", "dataframe"},
                      {"event_before", "integer"},
                      {"event_after", "integer"},
                      {"forward_labels", "dataframe[string-list]"}}),
        build_factor_report_op);

    // data (1 native snapshot; the three provider routes are external) ----
    add("data.snapshots.DataSnapshot.from_frame",
        "fincore.data.snapshots.DataSnapshot.from_frame",
        "fincore::data_snapshot::from_frame", "string[manifest-json]",
        input_schema({{"frame", "dataframe"},
                      {"provider", "string"},
                      {"requested_start", "string"},
                      {"requested_end", "string"},
                      {"as_of", "string"}},
                     {{"timezone", "string"},
                      {"price_adjustment", "string"}}),
        data_snapshot_from_frame_op);

    return operations;
}

}  // namespace fincore
