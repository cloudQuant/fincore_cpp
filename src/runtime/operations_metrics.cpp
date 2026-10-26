#include <fincore/runtime/operations_metrics.hpp>

#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/basic.hpp>
#include <fincore/metrics/consecutive.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/rolling.hpp>
#include <fincore/metrics/stats.hpp>
#include <fincore/metrics/timing.hpp>
#include <fincore/metrics/yearly.hpp>
#include <fincore/performance/cashflows.hpp>
#include <fincore/performance/disclosures.hpp>
#include <fincore/performance/inference.hpp>
#include <fincore/performance/returns.hpp>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr timestamp missing_timestamp = std::numeric_limits<timestamp>::min();

[[noreturn]] void reject_input(const std::string& message) {
    throw input_error{message};
}

const runtime_value& require_value(const runtime_inputs& inputs,
                                   const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        reject_input("missing runtime input: " + name);
    }
    return found->second;
}

const dataframe& require_frame(const runtime_inputs& inputs,
                               const std::string& name) {
    const auto& value = require_value(inputs, name);
    const auto* frame = std::get_if<dataframe>(&value.data);
    if (frame == nullptr) {
        reject_input("runtime input must be a dataframe: " + name);
    }
    return *frame;
}

// Absent and wrong-typed are different answers: only a missing key yields the null
// pointer, so an optional input that was supplied but carries the wrong type fails
// loudly instead of being read as "not given" and silently defaulted.
const dataframe* optional_frame(const runtime_inputs& inputs,
                                const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return nullptr;
    }
    const auto* frame = std::get_if<dataframe>(&found->second.data);
    if (frame == nullptr) {
        reject_input("runtime input must be a dataframe: " + name);
    }
    return frame;
}

std::optional<double> optional_double(const runtime_inputs& inputs,
                                      const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return std::nullopt;
    }
    const auto* number = std::get_if<double>(&found->second.data);
    if (number == nullptr) {
        reject_input("runtime input must be a number: " + name);
    }
    return *number;
}

std::int64_t require_integer(const runtime_inputs& inputs,
                             const std::string& name) {
    const auto& value = require_value(inputs, name);
    const auto* number = std::get_if<std::int64_t>(&value.data);
    if (number == nullptr) {
        reject_input("runtime input must be an integer: " + name);
    }
    return *number;
}

std::int64_t integer_or(const runtime_inputs& inputs,
                        const std::string& name,
                        const std::int64_t fallback) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return fallback;
    }
    const auto* number = std::get_if<std::int64_t>(&found->second.data);
    if (number == nullptr) {
        reject_input("runtime input must be an integer: " + name);
    }
    return *number;
}

std::size_t to_size(const std::int64_t value, const std::string& name) {
    if (value < 0) {
        reject_input("runtime input must be non-negative: " + name);
    }
    const auto unsigned_value = static_cast<std::uint64_t>(value);
    if (unsigned_value >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        reject_input("runtime input is too large: " + name);
    }
    return static_cast<std::size_t>(unsigned_value);
}

std::size_t require_size(const runtime_inputs& inputs,
                         const std::string& name) {
    return to_size(require_integer(inputs, name), name);
}

std::size_t size_or(const runtime_inputs& inputs, const std::string& name,
                    const std::size_t fallback) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return fallback;
    }
    const auto* number = std::get_if<std::int64_t>(&found->second.data);
    if (number == nullptr) {
        reject_input("runtime input must be an integer: " + name);
    }
    return to_size(*number, name);
}

std::optional<std::string> optional_string(const runtime_inputs& inputs,
                                           const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return std::nullopt;
    }
    const auto* text = std::get_if<std::string>(&found->second.data);
    if (text == nullptr) {
        reject_input("runtime input must be a string: " + name);
    }
    return *text;
}

std::string string_or(const runtime_inputs& inputs, const std::string& name,
                      std::string fallback) {
    if (auto value = optional_string(inputs, name)) {
        return std::move(*value);
    }
    return fallback;
}

bool bool_or(const runtime_inputs& inputs, const std::string& name,
             const bool fallback) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return fallback;
    }
    const auto* value = std::get_if<bool>(&found->second.data);
    if (value == nullptr) {
        reject_input("runtime input must be a boolean: " + name);
    }
    return *value;
}

frequency parse_frequency(const std::string& text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char character : text) {
        lowered.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    if (lowered == "daily") {
        return frequency::daily;
    }
    if (lowered == "weekly") {
        return frequency::weekly;
    }
    if (lowered == "monthly") {
        return frequency::monthly;
    }
    if (lowered == "quarterly") {
        return frequency::quarterly;
    }
    if (lowered == "yearly") {
        return frequency::yearly;
    }
    reject_input("unsupported period: " + text);
}

frequency period_input(const runtime_inputs& inputs) {
    return parse_frequency(string_or(inputs, "period", "daily"));
}

double annualization_input(const runtime_inputs& inputs) {
    return annualization_factor(period_input(inputs),
                                optional_double(inputs, "annualization"));
}

return_aggregation_period aggregation_period_input(const runtime_inputs& inputs) {
    const auto text = string_or(inputs, "convert_to", "monthly");
    std::string lowered;
    lowered.reserve(text.size());
    for (const char character : text) {
        lowered.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(character))));
    }
    if (lowered == "weekly") {
        return return_aggregation_period::weekly;
    }
    if (lowered == "monthly") {
        return return_aggregation_period::monthly;
    }
    if (lowered == "quarterly") {
        return return_aggregation_period::quarterly;
    }
    if (lowered == "yearly") {
        return return_aggregation_period::yearly;
    }
    reject_input("unsupported convert_to: " + text);
}

fee_treatment fee_treatment_input(const runtime_inputs& inputs) {
    const auto text = string_or(inputs, "fee_treatment", "net");
    if (text == "net") {
        return fee_treatment::net;
    }
    if (text == "gross") {
        return fee_treatment::gross;
    }
    reject_input("fee_treatment must be either 'net' or 'gross'");
}

double timing_input(const runtime_inputs& inputs) {
    const auto text = string_or(inputs, "timing", "end");
    if (text == "end") {
        return 0.0;
    }
    if (text == "start") {
        return 1.0;
    }
    reject_input("timing must be either 'end' or 'start'");
}

// ---------------------------------------------------------------------------
// Input-schema serialization.
// ---------------------------------------------------------------------------

struct optional_field {
    std::string_view name;
    std::string_view type;
    std::string_view default_json;
};

std::string make_input_schema(
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        required,
    std::initializer_list<optional_field> optional = {}) {
    std::string result{"{\"required\":{"};
    bool first = true;
    for (const auto& [name, type] : required) {
        if (!first) {
            result.push_back(',');
        }
        first = false;
        result.push_back('"');
        result.append(name);
        result.append("\":\"");
        result.append(type);
        result.push_back('"');
    }
    result.append("},\"optional\":{");
    first = true;
    for (const auto& field : optional) {
        if (!first) {
            result.push_back(',');
        }
        first = false;
        result.push_back('"');
        result.append(field.name);
        result.append("\":{\"type\":\"");
        result.append(field.type);
        result.append("\",\"default\":");
        result.append(field.default_json);
        result.push_back('}');
    }
    result.append("}}");
    return result;
}

// ---------------------------------------------------------------------------
// Result marshalling helpers.
// ---------------------------------------------------------------------------

dataframe indexed_frame(std::vector<timestamp> index) {
    dataframe frame;
    frame.load_index(index.begin(), index.end());
    return frame;
}

dataframe positional_frame(const std::size_t rows) {
    std::vector<timestamp> index(rows);
    for (std::size_t position = 0; position < rows; ++position) {
        index[position] = static_cast<timestamp>(position);
    }
    return indexed_frame(std::move(index));
}

void add_double_column(dataframe& frame, const std::string_view name,
                       std::vector<double> values) {
    frame.load_column<double>(std::string(name).c_str(), std::move(values),
                              hmdf::nan_policy::dont_pad_with_nans);
}

void add_integer_column(dataframe& frame, const std::string_view name,
                        std::vector<std::int64_t> values) {
    frame.load_column<std::int64_t>(std::string(name).c_str(),
                                    std::move(values),
                                    hmdf::nan_policy::dont_pad_with_nans);
}

void add_string_column(dataframe& frame, const std::string_view name,
                       std::vector<std::string> values) {
    frame.load_column<std::string>(std::string(name).c_str(),
                                   std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
}

dataframe two_value_frame(const std::string_view first_name,
                          const double first_value,
                          const std::string_view second_name,
                          const double second_value) {
    auto frame = positional_frame(1U);
    add_double_column(frame, first_name, {first_value});
    add_double_column(frame, second_name, {second_value});
    return frame;
}

dataframe optional_timestamp_frame(const std::string_view column,
                                   const std::optional<timestamp> value) {
    if (!value.has_value()) {
        auto frame = indexed_frame({});
        add_integer_column(frame, column, {});
        return frame;
    }
    auto frame = positional_frame(1U);
    add_integer_column(frame, column, {*value});
    return frame;
}

dataframe timestamp_pair_frame(const std::optional<timestamp> start,
                               const std::optional<timestamp> end) {
    auto frame = positional_frame(1U);
    add_integer_column(frame, "start",
                       {start.value_or(missing_timestamp)});
    add_integer_column(frame, "end", {end.value_or(missing_timestamp)});
    return frame;
}

dataframe drawdown_windows_frame(
    const std::vector<drawdown_window>& windows) {
    auto frame = positional_frame(windows.size());
    std::vector<std::int64_t> peaks;
    std::vector<std::int64_t> valleys;
    std::vector<std::int64_t> recoveries;
    peaks.reserve(windows.size());
    valleys.reserve(windows.size());
    recoveries.reserve(windows.size());
    for (const auto& window : windows) {
        peaks.push_back(window.peak);
        valleys.push_back(window.valley);
        recoveries.push_back(window.recovery.value_or(missing_timestamp));
    }
    add_integer_column(frame, "peak", std::move(peaks));
    add_integer_column(frame, "valley", std::move(valleys));
    add_integer_column(frame, "recovery", std::move(recoveries));
    return frame;
}

dataframe drawdown_windows_frame(
    const std::optional<drawdown_window>& window) {
    std::vector<drawdown_window> windows;
    if (window.has_value()) {
        windows.push_back(*window);
    }
    if (!window.has_value()) {
        auto frame = positional_frame(1U);
        add_integer_column(frame, "peak", {missing_timestamp});
        add_integer_column(frame, "valley", {missing_timestamp});
        add_integer_column(frame, "recovery", {missing_timestamp});
        return frame;
    }
    return drawdown_windows_frame(windows);
}

dataframe drawdown_periods_frame(
    const std::vector<drawdown_period>& periods) {
    auto frame = positional_frame(periods.size());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<std::int64_t> starts;
    std::vector<std::int64_t> valleys;
    std::vector<std::int64_t> recoveries;
    std::vector<double> values;
    std::vector<double> durations;
    std::vector<double> recovery_durations;
    starts.reserve(periods.size());
    valleys.reserve(periods.size());
    recoveries.reserve(periods.size());
    values.reserve(periods.size());
    durations.reserve(periods.size());
    recovery_durations.reserve(periods.size());
    for (const auto& period : periods) {
        starts.push_back(period.start);
        valleys.push_back(period.valley);
        recoveries.push_back(period.recovery.value_or(missing_timestamp));
        values.push_back(period.value);
        durations.push_back(static_cast<double>(period.duration_periods));
        recovery_durations.push_back(
            period.recovery_duration_periods.has_value()
                ? static_cast<double>(*period.recovery_duration_periods)
                : nan);
    }
    add_integer_column(frame, "start", std::move(starts));
    add_integer_column(frame, "valley", std::move(valleys));
    add_integer_column(frame, "recovery", std::move(recoveries));
    add_double_column(frame, "value", std::move(values));
    add_double_column(frame, "duration", std::move(durations));
    add_double_column(frame, "recovery_duration",
                      std::move(recovery_durations));
    return frame;
}

dataframe drawdown_table_frame(
    const std::vector<drawdown_table_row>& rows) {
    auto frame = positional_frame(rows.size());
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> net_drawdowns;
    std::vector<std::int64_t> peaks;
    std::vector<std::int64_t> valleys;
    std::vector<std::int64_t> recoveries;
    std::vector<double> durations;
    net_drawdowns.reserve(rows.size());
    peaks.reserve(rows.size());
    valleys.reserve(rows.size());
    recoveries.reserve(rows.size());
    durations.reserve(rows.size());
    for (const auto& row : rows) {
        net_drawdowns.push_back(row.net_drawdown_percent);
        peaks.push_back(row.peak.value_or(missing_timestamp));
        valleys.push_back(row.valley.value_or(missing_timestamp));
        recoveries.push_back(row.recovery.value_or(missing_timestamp));
        durations.push_back(row.duration_business_days.has_value()
                                ? static_cast<double>(*row.duration_business_days)
                                : nan);
    }
    add_double_column(frame, "net_drawdown_percent",
                      std::move(net_drawdowns));
    add_integer_column(frame, "peak", std::move(peaks));
    add_integer_column(frame, "valley", std::move(valleys));
    add_integer_column(frame, "recovery", std::move(recoveries));
    add_double_column(frame, "duration_business_days",
                      std::move(durations));
    return frame;
}

dataframe double_series_frame(const std::string_view column,
                              const std::vector<double>& values) {
    auto frame = positional_frame(values.size());
    add_double_column(frame, column, values);
    return frame;
}

dataframe statistics_frame(const std::map<std::string, double>& values) {
    auto frame = positional_frame(values.size());
    std::vector<std::string> names;
    std::vector<double> numbers;
    names.reserve(values.size());
    numbers.reserve(values.size());
    for (const auto& [name, value] : values) {
        names.push_back(name);
        numbers.push_back(value);
    }
    add_string_column(frame, "statistic", std::move(names));
    add_double_column(frame, "value", std::move(numbers));
    return frame;
}

dataframe named_returns_frame(const std::vector<named_returns>& ranges) {
    std::vector<timestamp> index;
    std::vector<std::string> names;
    std::vector<double> values;
    for (const auto& range : ranges) {
        const auto series =
            require_double_column(range.returns, returns_column);
        const auto& series_index = range.returns.get_index();
        for (std::size_t position = 0; position < series.size(); ++position) {
            index.push_back(series_index[position]);
            names.push_back(range.name);
            values.push_back(series[position]);
        }
    }
    auto frame = indexed_frame(std::move(index));
    add_string_column(frame, "name", std::move(names));
    add_double_column(frame, "returns", std::move(values));
    return frame;
}

bool has_double_column(const dataframe& frame, const std::string_view name) {
    for (const auto& info : frame.get_columns_info<double>()) {
        if (std::string_view(std::get<0>(info).c_str()) == name) {
            return true;
        }
    }
    return false;
}

std::optional<dataframe> optional_cashflow_ledger(
    const runtime_inputs& inputs, const std::string& key,
    const double default_timing) {
    const auto* source = optional_frame(inputs, key);
    if (source == nullptr) {
        return std::nullopt;
    }
    if (has_double_column(*source, "timing")) {
        return *source;
    }
    const auto amounts = require_double_column(*source, "amount");
    auto ledger = indexed_frame(
        std::vector<timestamp>(source->get_index().begin(),
                               source->get_index().end()));
    add_double_column(ledger, "amount",
                      std::vector<double>(amounts.begin(), amounts.end()));
    add_double_column(
        ledger, "timing",
        std::vector<double>(amounts.size(), default_timing));
    return ledger;
}

dataframe peer_frame(const dataframe& source, const std::string_view column) {
    const auto values = require_double_column(source, column);
    auto index = std::vector<timestamp>(source.get_index().begin(),
                                        source.get_index().end());
    if (values.size() < index.size()) {
        index.resize(values.size());
    }
    auto frame = indexed_frame(std::move(index));
    add_double_column(frame, "returns",
                      std::vector<double>(values.begin(), values.end()));
    return frame;
}

std::vector<std::string> split_notes(const std::string& text) {
    std::vector<std::string> notes;
    std::size_t start = 0U;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        const auto length =
            end == std::string::npos ? text.size() - start : end - start;
        if (length > 0U) {
            notes.push_back(text.substr(start, length));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }
    return notes;
}

operation_spec make_operation(
    std::string operation_id, std::string kernel_symbol,
    std::function<runtime_value(const runtime_inputs&)> callable,
    std::string input_schema) {
    const auto separator = operation_id.find('.');
    const std::string domain =
        separator == std::string::npos ? operation_id
                                       : operation_id.substr(0U, separator);
    std::string capability_id = "fincore." + operation_id;
    if (operation_id == "metrics.ratios.common_sense_ratio") {
        // The pinned source routes this operation through
        // fincore.metrics.stats.common_sense_ratio.
        capability_id = "fincore.metrics.stats.common_sense_ratio";
    }
    const auto source_operation_id = operation_id;
    std::map<std::string, std::string> provenance{
        {"owner", domain},
        {"kernel_symbol", std::move(kernel_symbol)},
        {"source_operation_id", source_operation_id},
        {"input_schema", std::move(input_schema)}};
    return {std::move(operation_id), std::move(capability_id),
            std::move(domain), std::move(callable), true, "none",
            std::nullopt, std::nullopt, std::move(provenance)};
}

}  // namespace

std::vector<operation_spec> metrics_performance_operations() {
    std::vector<operation_spec> operations;
    operations.reserve(98U);

    // -----------------------------------------------------------------
    // metrics.alpha_beta
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.alpha_beta.alpha_aligned", "alpha",
        [](const runtime_inputs& inputs) {
            return runtime_value(alpha(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.alpha_beta", "alpha_beta",
        [](const runtime_inputs& inputs) {
            const auto [alpha_value, beta_value] = alpha_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs));
            return runtime_value(
                two_value_frame("alpha", alpha_value, "beta", beta_value));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.alpha_beta_aligned", "alpha_beta",
        [](const runtime_inputs& inputs) {
            const auto [alpha_value, beta_value] = alpha_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs));
            return runtime_value(
                two_value_frame("alpha", alpha_value, "beta", beta_value));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.alpha_percentile_rank", "alpha_percentile_rank",
        [](const runtime_inputs& inputs) {
            const auto& peers = require_frame(inputs, "peer_returns");
            std::vector<dataframe> peer_frames;
            for (const auto& info : peers.get_columns_info<double>()) {
                const std::string name(std::get<0>(info).c_str());
                peer_frames.push_back(peer_frame(peers, name));
            }
            return runtime_value(alpha_percentile_rank(
                require_frame(inputs, "returns"),
                std::span<const dataframe>(peer_frames.data(),
                                           peer_frames.size()),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"},
             {"peer_returns", "dataframe"},
             {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.annual_alpha", "annual_alpha",
        [](const runtime_inputs& inputs) {
            return runtime_value(annual_alpha(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.annual_beta", "annual_beta",
        [](const runtime_inputs& inputs) {
            return runtime_value(annual_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.beta_aligned", "beta",
        [](const runtime_inputs& inputs) {
            return runtime_value(beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.down_alpha_beta", "down_alpha_beta",
        [](const runtime_inputs& inputs) {
            const auto [alpha_value, beta_value] = down_alpha_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs));
            return runtime_value(
                two_value_frame("alpha", alpha_value, "beta", beta_value));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.alpha_beta.up_alpha_beta", "up_alpha_beta",
        [](const runtime_inputs& inputs) {
            const auto [alpha_value, beta_value] = up_alpha_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs));
            return runtime_value(
                two_value_frame("alpha", alpha_value, "beta", beta_value));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));

    // -----------------------------------------------------------------
    // metrics.consecutive
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.consecutive.consecutive_stats", "consecutive_stats",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                statistics_frame(consecutive_stats(
                    require_frame(inputs, "returns"))));
        },
        make_input_schema({{"returns", "dataframe"}})));
    const auto add_consecutive_scalar =
        [&operations](const std::string& id, const std::string& kernel,
                      double (*function)(const dataframe&, std::string_view)) {
            operations.push_back(make_operation(
                id, kernel,
                [function](const runtime_inputs& inputs) {
                    return runtime_value(
                        function(require_frame(inputs, "returns"),
                                 returns_column));
                },
                make_input_schema({{"returns", "dataframe"}})));
        };
    add_consecutive_scalar("metrics.consecutive.max_consecutive_down_days",
                           "max_consecutive_down_days",
                           max_consecutive_down_days);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_down_months",
                           "max_consecutive_down_months",
                           max_consecutive_down_months);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_down_weeks",
                           "max_consecutive_down_weeks",
                           max_consecutive_down_weeks);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_gain",
                           "max_consecutive_gain", max_consecutive_gain);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_loss",
                           "max_consecutive_loss", max_consecutive_loss);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_up_days",
                           "max_consecutive_up_days",
                           max_consecutive_up_days);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_up_months",
                           "max_consecutive_up_months",
                           max_consecutive_up_months);
    add_consecutive_scalar("metrics.consecutive.max_consecutive_up_weeks",
                           "max_consecutive_up_weeks",
                           max_consecutive_up_weeks);
    add_consecutive_scalar("metrics.consecutive.max_single_day_gain",
                           "max_single_day_gain", max_single_day_gain);
    add_consecutive_scalar("metrics.consecutive.max_single_day_loss",
                           "max_single_day_loss", max_single_day_loss);

    const auto add_consecutive_date =
        [&operations](const std::string& id, const std::string& kernel,
                      std::optional<timestamp> (*function)(
                          const dataframe&, std::string_view)) {
            operations.push_back(make_operation(
                id, kernel,
                [function](const runtime_inputs& inputs) {
                    return runtime_value(optional_timestamp_frame(
                        "date",
                        function(require_frame(inputs, "returns"),
                                 returns_column)));
                },
                make_input_schema({{"returns", "dataframe"}})));
        };
    add_consecutive_date(
        "metrics.consecutive.max_consecutive_down_end_date",
        "max_consecutive_down_end_date", max_consecutive_down_end_date);
    add_consecutive_date(
        "metrics.consecutive.max_consecutive_down_start_date",
        "max_consecutive_down_start_date", max_consecutive_down_start_date);
    add_consecutive_date(
        "metrics.consecutive.max_consecutive_up_end_date",
        "max_consecutive_up_end_date", max_consecutive_up_end_date);
    add_consecutive_date(
        "metrics.consecutive.max_consecutive_up_start_date",
        "max_consecutive_up_start_date", max_consecutive_up_start_date);
    add_consecutive_date("metrics.consecutive.max_single_day_gain_date",
                         "max_single_day_gain_date",
                         max_single_day_gain_date);
    add_consecutive_date("metrics.consecutive.max_single_day_loss_date",
                         "max_single_day_loss_date", max_single_day_loss_date);

    // -----------------------------------------------------------------
    // metrics.drawdown
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.drawdown.gen_drawdown_table", "gen_drawdown_table",
        [](const runtime_inputs& inputs) {
            return runtime_value(drawdown_table_frame(gen_drawdown_table(
                require_frame(inputs, "returns"),
                size_or(inputs, "top", 10U))));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"top", "integer", "10"}})));
    operations.push_back(make_operation(
        "metrics.drawdown.get_all_drawdowns", "get_all_drawdowns",
        [](const runtime_inputs& inputs) {
            return runtime_value(double_series_frame(
                "drawdown", get_all_drawdowns(
                                require_frame(inputs, "returns"))));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.drawdown.get_all_drawdowns_detailed",
        "get_all_drawdowns_detailed",
        [](const runtime_inputs& inputs) {
            return runtime_value(drawdown_periods_frame(
                get_all_drawdowns_detailed(
                    require_frame(inputs, "returns"))));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.drawdown.get_max_drawdown", "max_drawdown_window",
        [](const runtime_inputs& inputs) {
            return runtime_value(drawdown_windows_frame(max_drawdown_window(
                require_frame(inputs, "returns"))));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.drawdown.get_max_drawdown_period", "max_drawdown_period",
        [](const runtime_inputs& inputs) {
            const auto [start, end] = max_drawdown_period(
                require_frame(inputs, "returns"));
            return runtime_value(timestamp_pair_frame(start, end));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.drawdown.get_top_drawdowns", "get_top_drawdowns",
        [](const runtime_inputs& inputs) {
            return runtime_value(drawdown_windows_frame(get_top_drawdowns(
                require_frame(inputs, "returns"),
                size_or(inputs, "top", 10U))));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"top", "integer", "10"}})));

    const auto add_drawdown_scalar =
        [&operations](const std::string& id, const std::string& kernel,
                      double (*function)(const dataframe&, std::string_view)) {
            operations.push_back(make_operation(
                id, kernel,
                [function](const runtime_inputs& inputs) {
                    return runtime_value(
                        function(require_frame(inputs, "returns"),
                                 returns_column));
                },
                make_input_schema({{"returns", "dataframe"}})));
        };
    add_drawdown_scalar(
        "metrics.drawdown.max_drawdown_recovery_days",
        "max_drawdown_recovery_days", max_drawdown_recovery_days);
    add_drawdown_scalar(
        "metrics.drawdown.max_drawdown_recovery_months",
        "max_drawdown_recovery_months", max_drawdown_recovery_months);
    add_drawdown_scalar(
        "metrics.drawdown.max_drawdown_recovery_weeks",
        "max_drawdown_recovery_weeks", max_drawdown_recovery_weeks);
    add_drawdown_scalar("metrics.drawdown.second_max_drawdown",
                        "second_max_drawdown", second_max_drawdown);
    add_drawdown_scalar("metrics.drawdown.second_max_drawdown_days",
                        "second_max_drawdown_days",
                        second_max_drawdown_days);
    add_drawdown_scalar(
        "metrics.drawdown.second_max_drawdown_recovery_days",
        "second_max_drawdown_recovery_days",
        second_max_drawdown_recovery_days);
    add_drawdown_scalar("metrics.drawdown.third_max_drawdown",
                        "third_max_drawdown", third_max_drawdown);
    add_drawdown_scalar("metrics.drawdown.third_max_drawdown_days",
                        "third_max_drawdown_days", third_max_drawdown_days);
    add_drawdown_scalar(
        "metrics.drawdown.third_max_drawdown_recovery_days",
        "third_max_drawdown_recovery_days",
        third_max_drawdown_recovery_days);

    // -----------------------------------------------------------------
    // metrics.ratios
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.ratios.cal_treynor_ratio", "cal_treynor_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(cal_treynor_ratio(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.ratios.capture", "capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(capture(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.ratios.common_sense_ratio", "common_sense_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(common_sense_ratio(
                require_frame(inputs, "returns")));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.ratios.down_capture", "down_capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(down_capture(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.ratios.down_capture_return", "down_capture_return",
        [](const runtime_inputs& inputs) {
            return runtime_value(down_capture_return(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.ratios.excess_sharpe", "excess_sharpe",
        [](const runtime_inputs& inputs) {
            return runtime_value(excess_sharpe(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.ratios.kappa_three_ratio", "kappa_three_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(kappa_three_ratio(
                require_frame(inputs, "returns"),
                optional_double(inputs, "mar").value_or(0.0)));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"mar", "double", "0.0"}})));
    operations.push_back(make_operation(
        "metrics.ratios.stability_of_timeseries",
        "stability_of_timeseries",
        [](const runtime_inputs& inputs) {
            return runtime_value(stability_of_timeseries(
                require_frame(inputs, "returns")));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.ratios.treynor_ratio", "cal_treynor_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(cal_treynor_ratio(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                optional_double(inputs, "risk_free").value_or(0.0),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.ratios.up_capture", "up_capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(up_capture(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.ratios.up_capture_return", "up_capture_return",
        [](const runtime_inputs& inputs) {
            return runtime_value(up_capture_return(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                annualization_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));

    // -----------------------------------------------------------------
    // metrics.returns
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.returns.aggregate_returns", "aggregate_returns",
        [](const runtime_inputs& inputs) {
            return runtime_value(aggregate_returns(
                require_frame(inputs, "returns"),
                aggregation_period_input(inputs)));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"convert_to", "string", "\"monthly\""}})));
    operations.push_back(make_operation(
        "metrics.returns.normalize", "normalize",
        [](const runtime_inputs& inputs) {
            return runtime_value(normalize(
                require_frame(inputs, "returns"),
                optional_double(inputs, "starting_value").value_or(1.0)));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"starting_value", "double", "1.0"}})));
    operations.push_back(make_operation(
        "metrics.returns.simple_returns", "simple_returns",
        [](const runtime_inputs& inputs) {
            return runtime_value(simple_returns(
                require_frame(inputs, "prices"),
                string_or(inputs, "price_column", "price")));
        },
        make_input_schema({{"prices", "dataframe"}},
                          {{"price_column", "string", "\"price\""}})));

    // -----------------------------------------------------------------
    // metrics.risk
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.risk.value_at_risk", "historical_value_at_risk",
        [](const runtime_inputs& inputs) {
            return runtime_value(historical_value_at_risk(
                require_frame(inputs, "returns"),
                optional_double(inputs, "cutoff").value_or(0.05)));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"cutoff", "double", "0.05"}})));
    operations.push_back(make_operation(
        "metrics.risk.var_excess_return", "var_excess_return",
        [](const runtime_inputs& inputs) {
            return runtime_value(var_excess_return(
                require_frame(inputs, "returns"),
                optional_double(inputs, "cutoff").value_or(0.05),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}},
            {{"cutoff", "double", "0.05"},
             {"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.risk.beta_fragility_heuristic_aligned",
        "beta_fragility_heuristic",
        [](const runtime_inputs& inputs) {
            return runtime_value(beta_fragility_heuristic(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));

    // -----------------------------------------------------------------
    // metrics.rolling
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.rolling.roll_alpha", "roll_alpha",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_alpha(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_alpha_aligned", "roll_alpha",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_alpha(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_alpha_beta", "roll_alpha_beta",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_alpha_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_alpha_beta_aligned", "roll_alpha_beta",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_alpha_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_beta_aligned", "roll_beta",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_down_capture", "roll_down_capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_down_capture(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U), period_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"period", "string", "\"daily\""}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_up_capture", "roll_up_capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_up_capture(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U), period_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"period", "string", "\"daily\""}})));
    operations.push_back(make_operation(
        "metrics.rolling.roll_up_down_capture", "roll_up_down_capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(roll_up_down_capture(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "window", 252U), period_input(inputs)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"window", "integer", "252"},
             {"period", "string", "\"daily\""}})));
    operations.push_back(make_operation(
        "metrics.rolling.rolling_regression", "rolling_regression",
        [](const runtime_inputs& inputs) {
            return runtime_value(rolling_regression(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                size_or(inputs, "rolling_window", 126U)));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"rolling_window", "integer", "126"}})));
    operations.push_back(make_operation(
        "metrics.rolling.rolling_sharpe", "rolling_sharpe",
        [](const runtime_inputs& inputs) {
            return runtime_value(rolling_sharpe(
                require_frame(inputs, "returns"),
                require_size(inputs, "rolling_sharpe_window"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"},
             {"rolling_sharpe_window", "integer"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.rolling.rolling_volatility", "rolling_volatility",
        [](const runtime_inputs& inputs) {
            return runtime_value(rolling_volatility(
                require_frame(inputs, "returns"),
                require_size(inputs, "rolling_vol_window"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"rolling_vol_window", "integer"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));

    // -----------------------------------------------------------------
    // metrics.stats
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.stats.bond_market_correlation", "safe_correlation",
        [](const runtime_inputs& inputs) {
            return runtime_value(safe_correlation(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.stats.capm_r_squared", "capm_r_squared",
        [](const runtime_inputs& inputs) {
            return runtime_value(capm_r_squared(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.stats.futures_market_correlation", "safe_correlation",
        [](const runtime_inputs& inputs) {
            return runtime_value(safe_correlation(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.stats.r_cubed", "r_cubed",
        [](const runtime_inputs& inputs) {
            return runtime_value(r_cubed(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.stats.relative_win_rate", "relative_win_rate",
        [](const runtime_inputs& inputs) {
            return runtime_value(relative_win_rate(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.stats.serial_correlation", "serial_correlation",
        [](const runtime_inputs& inputs) {
            const auto lag = integer_or(inputs, "lag", 1);
            if (lag < 0 ||
                lag > static_cast<std::int64_t>(
                          std::numeric_limits<std::ptrdiff_t>::max())) {
                reject_input("lag must be a non-negative integer");
            }
            return runtime_value(serial_correlation(
                require_frame(inputs, "returns"),
                static_cast<std::ptrdiff_t>(lag)));
        },
        make_input_schema({{"returns", "dataframe"}},
                          {{"lag", "integer", "1"}})));
    operations.push_back(make_operation(
        "metrics.stats.stock_market_correlation", "safe_correlation",
        [](const runtime_inputs& inputs) {
            return runtime_value(safe_correlation(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.stats.tracking_difference", "tracking_difference",
        [](const runtime_inputs& inputs) {
            return runtime_value(tracking_difference(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}})));

    // -----------------------------------------------------------------
    // metrics.timing
    // -----------------------------------------------------------------
    const auto add_timing_model =
        [&operations](const std::string& id, const std::string& kernel,
                      double (*function)(const dataframe&, const dataframe&,
                                         double, std::string_view,
                                         std::string_view)) {
            operations.push_back(make_operation(
                id, kernel,
                [function](const runtime_inputs& inputs) {
                    return runtime_value(function(
                        require_frame(inputs, "returns"),
                        require_frame(inputs, "benchmark"),
                        optional_double(inputs, "risk_free").value_or(0.0),
                        returns_column, benchmark_column));
                },
                make_input_schema(
                    {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
                    {{"risk_free", "double", "0.0"}})));
        };
    add_timing_model("metrics.timing.treynor_mazuy_timing",
                     "treynor_mazuy_timing", treynor_mazuy_timing);
    add_timing_model("metrics.timing.henriksson_merton_timing",
                     "henriksson_merton_timing", henriksson_merton_timing);
    add_timing_model("metrics.timing.market_timing_return",
                     "market_timing_return", market_timing_return);
    add_timing_model("metrics.timing.cornell_timing", "cornell_timing",
                     cornell_timing);
    operations.push_back(make_operation(
        "metrics.timing.extract_interesting_date_ranges",
        "extract_interesting_date_ranges",
        [](const runtime_inputs& inputs) {
            return runtime_value(named_returns_frame(
                extract_interesting_date_ranges(
                    require_frame(inputs, "returns"))));
        },
        make_input_schema({{"returns", "dataframe"}})));

    // -----------------------------------------------------------------
    // metrics.yearly
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "metrics.yearly.annual_active_return", "annual_active_return",
        [](const runtime_inputs& inputs) {
            return runtime_value(annual_active_return(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.yearly.annual_active_return_by_year",
        "annual_active_return_by_year",
        [](const runtime_inputs& inputs) {
            return runtime_value(annual_active_return_by_year(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.yearly.annual_return_by_year", "annual_return_by_year",
        [](const runtime_inputs& inputs) {
            return runtime_value(annual_return_by_year(
                require_frame(inputs, "returns"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.yearly.annual_volatility_by_year",
        "annual_volatility_by_year",
        [](const runtime_inputs& inputs) {
            return runtime_value(annual_volatility_by_year(
                require_frame(inputs, "returns"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.yearly.information_ratio_by_year",
        "information_ratio_by_year",
        [](const runtime_inputs& inputs) {
            return runtime_value(information_ratio_by_year(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}, {"benchmark", "dataframe"}},
            {{"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));
    operations.push_back(make_operation(
        "metrics.yearly.max_drawdown_by_year", "max_drawdown_by_year",
        [](const runtime_inputs& inputs) {
            return runtime_value(max_drawdown_by_year(
                require_frame(inputs, "returns")));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "metrics.yearly.sharpe_ratio_by_year", "sharpe_ratio_by_year",
        [](const runtime_inputs& inputs) {
            return runtime_value(sharpe_ratio_by_year(
                require_frame(inputs, "returns"),
                optional_double(inputs, "risk_free").value_or(0.0),
                period_input(inputs),
                optional_double(inputs, "annualization")));
        },
        make_input_schema(
            {{"returns", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"period", "string", "\"daily\""},
             {"annualization", "double", "null"}})));

    // -----------------------------------------------------------------
    // performance.cashflows
    // -----------------------------------------------------------------
    const auto cashflow_inputs_schema = [] {
        return make_input_schema(
            {{"valuations", "dataframe"}},
            {{"cashflows", "dataframe", "null"},
             {"fees", "dataframe", "null"},
             {"timing", "string", "\"end\""},
             {"fee_treatment", "string", "\"net\""}});
    };
    operations.push_back(make_operation(
        "performance.cashflows.cashflow_adjusted_returns",
        "cashflow_adjusted_returns",
        [](const runtime_inputs& inputs) {
            const auto cashflows =
                optional_cashflow_ledger(inputs, "cashflows",
                                         timing_input(inputs));
            const auto* fees = optional_frame(inputs, "fees");
            return runtime_value(cashflow_adjusted_returns(
                require_frame(inputs, "valuations"), cashflows,
                fees == nullptr ? std::nullopt
                                : std::optional<dataframe>(*fees),
                fee_treatment_input(inputs)));
        },
        cashflow_inputs_schema()));
    operations.push_back(make_operation(
        "performance.cashflows.cashflow_adjusted_twr",
        "cashflow_adjusted_twr",
        [](const runtime_inputs& inputs) {
            const auto cashflows =
                optional_cashflow_ledger(inputs, "cashflows",
                                         timing_input(inputs));
            const auto* fees = optional_frame(inputs, "fees");
            return runtime_value(cashflow_adjusted_twr(
                require_frame(inputs, "valuations"), cashflows,
                fees == nullptr ? std::nullopt
                                : std::optional<dataframe>(*fees),
                fee_treatment_input(inputs)));
        },
        cashflow_inputs_schema()));

    // -----------------------------------------------------------------
    // performance.disclosures
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "performance.disclosures.render_disclosure", "render_disclosure",
        [](const runtime_inputs& inputs) {
            disclosure_context context;
            context.convention =
                string_or(inputs, "convention", context.convention);
            context.sample_period =
                string_or(inputs, "sample_period", context.sample_period);
            context.data_quality =
                string_or(inputs, "data_quality", context.data_quality);
            context.fees = string_or(inputs, "fees", context.fees);
            context.cashflows =
                string_or(inputs, "cashflows", context.cashflows);
            context.benchmark =
                string_or(inputs, "benchmark", context.benchmark);
            context.risk_free =
                string_or(inputs, "risk_free", context.risk_free);
            context.annualized =
                bool_or(inputs, "annualized", context.annualized);
            context.return_type =
                string_or(inputs, "return_type", context.return_type);
            context.units = string_or(inputs, "units", context.units);
            context.frequency =
                string_or(inputs, "frequency", context.frequency);
            if (const auto notes = optional_string(inputs, "notes")) {
                context.notes = split_notes(*notes);
            }
            return runtime_value(render_disclosure(context));
        },
        make_input_schema(
            {},
            {{"convention", "string", "\"TWR\""},
             {"sample_period", "string", "\"\""},
             {"data_quality", "string", "\"\""},
             {"fees", "string", "\"gross-of-fees\""},
             {"cashflows", "string", "\"none\""},
             {"benchmark", "string", "\"\""},
             {"risk_free", "string", "\"\""},
             {"annualized", "boolean", "true"},
             {"notes", "string", "null"},
             {"return_type", "string", "\"\""},
             {"units", "string", "\"\""},
             {"frequency", "string", "\"\""}})));

    // -----------------------------------------------------------------
    // performance.inference
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "performance.inference.sharpe_confidence_interval",
        "sharpe_confidence_interval",
        [](const runtime_inputs& inputs) {
            const auto [lower, upper] = sharpe_confidence_interval(
                require_frame(inputs, "returns"),
                optional_double(inputs, "risk_free").value_or(0.0),
                optional_double(inputs, "z").value_or(1.96));
            return runtime_value(
                two_value_frame("lower", lower, "upper", upper));
        },
        make_input_schema(
            {{"returns", "dataframe"}},
            {{"risk_free", "double", "0.0"},
             {"z", "double", "1.96"}})));
    operations.push_back(make_operation(
        "performance.inference.sharpe_standard_error",
        "sharpe_standard_error",
        [](const runtime_inputs& inputs) {
            return runtime_value(sharpe_standard_error(
                require_frame(inputs, "returns"),
                optional_double(inputs, "risk_free").value_or(0.0)));
        },
        make_input_schema(
            {{"returns", "dataframe"}},
            {{"risk_free", "double", "0.0"}})));
    operations.push_back(make_operation(
        "performance.inference.standard_error_of_mean",
        "standard_error_of_mean",
        [](const runtime_inputs& inputs) {
            return runtime_value(standard_error_of_mean(
                require_frame(inputs, "returns")));
        },
        make_input_schema({{"returns", "dataframe"}})));

    // -----------------------------------------------------------------
    // performance.returns
    // -----------------------------------------------------------------
    operations.push_back(make_operation(
        "performance.returns.mwr", "mwr",
        [](const runtime_inputs& inputs) {
            return runtime_value(mwr(
                require_frame(inputs, "cashflows"),
                optional_double(inputs, "periods")));
        },
        make_input_schema({{"cashflows", "dataframe"}},
                          {{"periods", "double", "null"}})));
    operations.push_back(make_operation(
        "performance.returns.twr", "twr",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                twr(require_frame(inputs, "returns")));
        },
        make_input_schema({{"returns", "dataframe"}})));
    operations.push_back(make_operation(
        "performance.returns.xirr", "xirr",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                xirr(require_frame(inputs, "cashflows")));
        },
        make_input_schema({{"cashflows", "dataframe"}})));

    operations.push_back(make_operation(
        "metrics.drawdown.get_max_drawdown_underwater",
        "max_drawdown_underwater",
        [](const runtime_inputs& inputs) -> runtime_value {
            const auto window = max_drawdown_underwater(
                require_frame(inputs, "underwater"));
            if (!window.has_value()) {
                return runtime_value{};
            }
            dataframe frame;
            frame.load_index(std::vector<timestamp>{window->peak});
            frame.load_column<std::int64_t>(
                "peak", std::vector<std::int64_t>{window->peak},
                hmdf::nan_policy::dont_pad_with_nans);
            frame.load_column<std::int64_t>(
                "valley", std::vector<std::int64_t>{window->valley},
                hmdf::nan_policy::dont_pad_with_nans);
            frame.load_column<std::int64_t>(
                "recovery",
                std::vector<std::int64_t>{window->recovery.value_or(0)},
                hmdf::nan_policy::dont_pad_with_nans);
            frame.load_column<bool>(
                "recovered",
                std::vector<bool>{window->recovery.has_value()},
                hmdf::nan_policy::dont_pad_with_nans);
            return runtime_value(std::move(frame));
        },
        make_input_schema({{"underwater", "dataframe"}})));

    return operations;
}

}  // namespace fincore
