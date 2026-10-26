#include <fincore/factor/serialization.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/data/snapshots.hpp>
#include <fincore/error.hpp>
#include <fincore/factor/research.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr const char* factor_serialization_schema =
    "fincore-factor-analysis-native-v1";

factor_value::array_type number_array(const std::vector<timestamp>& values) {
    factor_value::array_type result;
    result.reserve(values.size());
    for (const timestamp value : values) {
        result.emplace_back(value);
    }
    return result;
}

factor_value::array_type string_array(const std::vector<std::string>& values) {
    factor_value::array_type result;
    result.reserve(values.size());
    for (const std::string& value : values) {
        result.emplace_back(value);
    }
    return result;
}

factor_value::array_type int_array(const std::vector<int>& values) {
    factor_value::array_type result;
    result.reserve(values.size());
    for (const int value : values) {
        result.emplace_back(static_cast<std::int64_t>(value));
    }
    return result;
}

template <typename Value>
factor_value::array_type double_array(const std::vector<Value>& values) {
    factor_value::array_type result;
    result.reserve(values.size());
    for (const double value : values) {
        if (std::isfinite(value)) {
            result.emplace_back(value);
        } else {
            result.emplace_back();
        }
    }
    return result;
}

factor_value config_value(const factor_analysis_config& config) {
    factor_value::object_type object;
    object["long_short"] = config.long_short;
    object["group_neutral"] = config.group_neutral;
    object["equal_weight"] = config.equal_weight;
    object["by_group"] = config.by_group;
    object["periods"] = string_array(config.periods);
    object["turnover_periods"] = int_array(config.turnover_periods);
    object["time_aggregation"] = string_array(config.time_aggregation);
    object["include_portfolio_inputs"] = config.include_portfolio_inputs;
    object["portfolio_benchmark_period"] = config.portfolio_benchmark_period;
    return factor_value{std::move(object)};
}

factor_value ic_table_value(const factor_ic_table& table) {
    factor_value::object_type object;
    object["dates"] = number_array(table.dates);
    object["labels"] = string_array(table.labels);
    object["values"] = double_array(table.values);
    object["groups"] = string_array(table.groups);
    return factor_value{std::move(object)};
}

factor_value ic_summary_value(const factor_ic_summary& summary) {
    factor_value::object_type object;
    object["labels"] = string_array(summary.labels);
    object["means"] = double_array(summary.means);
    object["groups"] = string_array(summary.groups);
    return factor_value{std::move(object)};
}

factor_value quantile_table_value(const quantile_return_table& table) {
    factor_value::object_type object;
    object["quantiles"] = int_array(table.quantiles);
    object["labels"] = string_array(table.labels);
    object["means"] = double_array(table.means);
    object["standard_errors"] = double_array(table.standard_errors);
    object["groups"] = string_array(table.groups);
    return factor_value{std::move(object)};
}

factor_value spread_value(const std::vector<factor_spread_row>& spreads) {
    factor_value::array_type rows;
    rows.reserve(spreads.size());
    for (const factor_spread_row& spread : spreads) {
        factor_value::object_type row;
        row["period"] = spread.period;
        row["spread"] = spread.spread;
        row["standard_error"] = spread.standard_error;
        row["group"] = spread.group;
        rows.emplace_back(std::move(row));
    }
    return factor_value{std::move(rows)};
}

factor_value quantile_statistics_value(
    const std::vector<factor_quantile_statistics>& statistics) {
    factor_value::array_type rows;
    rows.reserve(statistics.size());
    for (const factor_quantile_statistics& item : statistics) {
        factor_value::object_type row;
        row["quantile"] = static_cast<std::int64_t>(item.quantile);
        row["minimum"] = item.minimum;
        row["maximum"] = item.maximum;
        row["mean"] = item.mean;
        row["stddev"] = item.stddev;
        row["count"] = static_cast<std::int64_t>(item.count);
        row["count_percent"] = item.count_percent;
        rows.emplace_back(std::move(row));
    }
    return factor_value{std::move(rows)};
}

factor_value turnover_value(const std::vector<factor_turnover_series>& series) {
    factor_value::array_type rows;
    rows.reserve(series.size());
    for (const factor_turnover_series& item : series) {
        factor_value::object_type row;
        row["quantile"] = static_cast<std::int64_t>(item.quantile);
        row["lag"] = static_cast<std::int64_t>(item.lag);
        row["dates"] = number_array(item.values.dates);
        row["values"] = double_array(item.values.values);
        rows.emplace_back(std::move(row));
    }
    return factor_value{std::move(rows)};
}

factor_value loss_report_value(const factor_loss_report& report) {
    factor_value::object_type object;
    object["input_count"] = static_cast<std::int64_t>(report.input_count);
    object["finite_factor_count"] =
        static_cast<std::int64_t>(report.finite_factor_count);
    object["forward_returns_count"] =
        static_cast<std::int64_t>(report.forward_returns_count);
    object["binning_count"] = static_cast<std::int64_t>(report.binning_count);
    object["factor_input_loss"] = report.factor_input_loss;
    object["forward_returns_loss"] = report.forward_returns_loss;
    object["binning_loss"] = report.binning_loss;
    object["total_loss"] = report.total_loss;
    return factor_value{std::move(object)};
}

const factor_value& require_key(const factor_value& object,
                                const std::string_view key) {
    const factor_value* value = object.find(key);
    if (value == nullptr) {
        throw input_error{"serialized factor model is missing: " +
                          std::string(key)};
    }
    return *value;
}

std::int64_t read_integer(const factor_value& value) {
    if (value.is_integer()) {
        return value.as_integer();
    }
    throw input_error{"serialized factor model expected an integer"};
}

std::string read_string(const factor_value& value) {
    if (value.is_string()) {
        return value.as_string();
    }
    throw input_error{"serialized factor model expected a string"};
}

std::vector<std::string> read_strings(const factor_value& value) {
    if (!value.is_array()) {
        throw input_error{"serialized factor model expected a string array"};
    }
    std::vector<std::string> result;
    for (const factor_value& item : value.as_array()) {
        result.push_back(read_string(item));
    }
    return result;
}

std::vector<int> read_integers(const factor_value& value) {
    if (!value.is_array()) {
        throw input_error{"serialized factor model expected an integer array"};
    }
    std::vector<int> result;
    for (const factor_value& item : value.as_array()) {
        result.push_back(static_cast<int>(read_integer(item)));
    }
    return result;
}

bool read_bool(const factor_value& value) {
    if (value.is_bool()) {
        return value.as_bool();
    }
    throw input_error{"serialized factor model expected a boolean"};
}

factor_analysis_config read_config(const factor_value& value) {
    factor_analysis_config config;
    config.long_short = read_bool(require_key(value, "long_short"));
    config.group_neutral = read_bool(require_key(value, "group_neutral"));
    config.equal_weight = read_bool(require_key(value, "equal_weight"));
    config.by_group = read_bool(require_key(value, "by_group"));
    config.periods = read_strings(require_key(value, "periods"));
    config.turnover_periods = read_integers(require_key(value, "turnover_periods"));
    config.time_aggregation =
        read_strings(require_key(value, "time_aggregation"));
    config.include_portfolio_inputs =
        read_bool(require_key(value, "include_portfolio_inputs"));
    config.portfolio_benchmark_period =
        read_string(require_key(value, "portfolio_benchmark_period"));
    return config;
}

factor_loss_report read_loss_report(const factor_value& value) {
    factor_loss_report report;
    report.input_count =
        static_cast<std::size_t>(read_integer(require_key(value, "input_count")));
    report.finite_factor_count = static_cast<std::size_t>(
        read_integer(require_key(value, "finite_factor_count")));
    report.forward_returns_count = static_cast<std::size_t>(
        read_integer(require_key(value, "forward_returns_count")));
    report.binning_count =
        static_cast<std::size_t>(read_integer(require_key(value, "binning_count")));
    report.factor_input_loss = require_key(value, "factor_input_loss").as_number();
    report.forward_returns_loss =
        require_key(value, "forward_returns_loss").as_number();
    report.binning_loss = require_key(value, "binning_loss").as_number();
    report.total_loss = require_key(value, "total_loss").as_number();
    return report;
}

// --- Strict JSON parser ---------------------------------------------------

class json_parser {
public:
    explicit json_parser(const std::string_view text) : text_(text) {}

    factor_value parse() {
        skip_whitespace();
        factor_value value = parse_value();
        skip_whitespace();
        if (position_ != text_.size()) {
            throw input_error{"unexpected trailing JSON content"};
        }
        return value;
    }

private:
    factor_value parse_value() {
        if (position_ >= text_.size()) {
            throw input_error{"unexpected end of JSON input"};
        }
        const char character = text_[position_];
        switch (character) {
            case '{':
                return parse_object();
            case '[':
                return parse_array();
            case '"':
                return factor_value{parse_string()};
            case 't':
                expect_literal("true");
                return factor_value{true};
            case 'f':
                expect_literal("false");
                return factor_value{false};
            case 'n':
                expect_literal("null");
                return factor_value{};
            default:
                return parse_number();
        }
    }

    factor_value parse_object() {
        ++position_;
        factor_value::object_type object;
        skip_whitespace();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            return factor_value{std::move(object)};
        }
        while (true) {
            skip_whitespace();
            if (position_ >= text_.size() || text_[position_] != '"') {
                throw input_error{"JSON object keys must be strings"};
            }
            std::string key = parse_string();
            skip_whitespace();
            if (position_ >= text_.size() || text_[position_] != ':') {
                throw input_error{"JSON object expected ':'"};
            }
            ++position_;
            skip_whitespace();
            object.emplace(std::move(key), parse_value());
            skip_whitespace();
            if (position_ >= text_.size()) {
                throw input_error{"unterminated JSON object"};
            }
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == '}') {
                ++position_;
                return factor_value{std::move(object)};
            }
            throw input_error{"JSON object expected ',' or '}'"};
        }
    }

    factor_value parse_array() {
        ++position_;
        factor_value::array_type array;
        skip_whitespace();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            return factor_value{std::move(array)};
        }
        while (true) {
            skip_whitespace();
            array.push_back(parse_value());
            skip_whitespace();
            if (position_ >= text_.size()) {
                throw input_error{"unterminated JSON array"};
            }
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == ']') {
                ++position_;
                return factor_value{std::move(array)};
            }
            throw input_error{"JSON array expected ',' or ']'"};
        }
    }

    static void append_utf8(std::string& output, std::uint32_t code_point) {
        if (code_point <= 0x7FU) {
            output.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7FFU) {
            output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else if (code_point <= 0xFFFFU) {
            output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
            output.push_back(
                static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
            output.push_back(
                static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
            output.push_back(
                static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
    }

    std::uint32_t parse_hex_unit() {
        if (position_ + 4U > text_.size()) {
            throw input_error{"truncated JSON unicode escape"};
        }
        std::uint32_t value = 0U;
        for (int index = 0; index < 4; ++index) {
            const char character = text_[position_++];
            value <<= 4U;
            if (character >= '0' && character <= '9') {
                value |= static_cast<std::uint32_t>(character - '0');
            } else if (character >= 'a' && character <= 'f') {
                value |= static_cast<std::uint32_t>(character - 'a' + 10);
            } else if (character >= 'A' && character <= 'F') {
                value |= static_cast<std::uint32_t>(character - 'A' + 10);
            } else {
                throw input_error{"invalid JSON unicode escape"};
            }
        }
        return value;
    }

    std::string parse_string() {
        ++position_;
        std::string result;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '"') {
                return result;
            }
            if (character != '\\') {
                result.push_back(character);
                continue;
            }
            if (position_ >= text_.size()) {
                throw input_error{"unterminated JSON escape"};
            }
            const char escape = text_[position_++];
            switch (escape) {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escape);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u': {
                    std::uint32_t code_point = parse_hex_unit();
                    if (code_point >= 0xD800U && code_point <= 0xDBFFU) {
                        if (position_ + 2U > text_.size() ||
                            text_[position_] != '\\' ||
                            text_[position_ + 1U] != 'u') {
                            throw input_error{"invalid JSON surrogate pair"};
                        }
                        position_ += 2U;
                        const std::uint32_t low = parse_hex_unit();
                        if (low < 0xDC00U || low > 0xDFFFU) {
                            throw input_error{"invalid JSON surrogate pair"};
                        }
                        code_point =
                            0x10000U + ((code_point - 0xD800U) << 10U) +
                            (low - 0xDC00U);
                    }
                    append_utf8(result, code_point);
                    break;
                }
                default:
                    throw input_error{"invalid JSON escape"};
            }
        }
        throw input_error{"unterminated JSON string"};
    }

    factor_value parse_number() {
        const std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') {
            ++position_;
        }
        while (position_ < text_.size() &&
               text_[position_] >= '0' && text_[position_] <= '9') {
            ++position_;
        }
        bool floating = false;
        if (position_ < text_.size() && text_[position_] == '.') {
            floating = true;
            ++position_;
            while (position_ < text_.size() &&
                   text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        if (position_ < text_.size() &&
            (text_[position_] == 'e' || text_[position_] == 'E')) {
            floating = true;
            ++position_;
            if (position_ < text_.size() &&
                (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            while (position_ < text_.size() &&
                   text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        if (position_ == start ||
            (position_ == start + 1U && text_[start] == '-')) {
            throw input_error{"invalid JSON number"};
        }
        const std::string_view token = text_.substr(start, position_ - start);
        if (!floating) {
            std::int64_t integer = 0;
            const auto [pointer, error] =
                std::from_chars(token.data(), token.data() + token.size(),
                                integer);
            if (error == std::errc{} && pointer == token.data() + token.size()) {
                return factor_value{integer};
            }
        }
        double number = 0.0;
        const auto [pointer, error] =
            std::from_chars(token.data(), token.data() + token.size(), number);
        if (error != std::errc{} || pointer != token.data() + token.size() ||
            !std::isfinite(number)) {
            throw input_error{"invalid JSON number"};
        }
        return factor_value{number};
    }

    void expect_literal(const std::string_view literal) {
        if (text_.substr(position_, literal.size()) != literal) {
            throw input_error{"invalid JSON literal"};
        }
        position_ += literal.size();
    }

    void skip_whitespace() {
        while (position_ < text_.size()) {
            const char character = text_[position_];
            if (character != ' ' && character != '\t' && character != '\n' &&
                character != '\r') {
                return;
            }
            ++position_;
        }
    }

    std::string_view text_;
    std::size_t position_ = 0U;
};

void append_big_endian(std::string& output, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}

}  // namespace

const factor_value& factor_value::at(const std::string_view key) const {
    const factor_value* value = find(key);
    if (value == nullptr) {
        throw input_error{"factor value has no key: " + std::string(key)};
    }
    return *value;
}

const factor_value* factor_value::find(const std::string_view key) const noexcept {
    if (!is_object()) {
        return nullptr;
    }
    const object_type& object = as_object();
    const auto found = object.find(std::string(key));
    return found == object.end() ? nullptr : &found->second;
}

std::string to_json_string(const factor_value& value) {
    std::string output;
    detail::json_writer writer(output);
    const auto write = [&writer](const factor_value& item, const auto& self) -> void {
        if (item.is_null()) {
            writer.value_null();
        } else if (item.is_bool()) {
            writer.value_boolean(item.as_bool());
        } else if (item.is_integer()) {
            writer.value_integer(item.as_integer());
        } else if (item.is_number()) {
            writer.value_number(item.as_number(), "factor value");
        } else if (item.is_string()) {
            writer.value_string(item.as_string());
        } else if (item.is_array()) {
            writer.begin_array();
            for (const factor_value& entry : item.as_array()) {
                self(entry, self);
            }
            writer.end_array();
        } else {
            writer.begin_object();
            for (const auto& [key, entry] : item.as_object()) {
                writer.key(key);
                self(entry, self);
            }
            writer.end_object();
        }
    };
    write(value, write);
    return output;
}

factor_value parse_json_value(const std::string_view text) {
    return json_parser(text).parse();
}

factor_value serializable_value(const factor_analysis_model& model) {
    factor_value::object_type object;
    object["schema"] = factor_serialization_schema;
    object["fingerprint_version"] =
        static_cast<std::int64_t>(model.result_fingerprint_version);
    object["result_fingerprint"] = model.result_fingerprint;
    object["config"] = config_value(model.config);
    object["forward_periods"] = string_array(model.forward_periods);
    object["loss_report"] = loss_report_value(model.factor_data.loss_report);
    object["quantile_statistics"] =
        quantile_statistics_value(model.quantile_statistics);
    object["information_coefficient"] =
        ic_table_value(model.information_coefficient);
    object["mean_information_coefficient"] =
        ic_summary_value(model.mean_information_coefficient);
    object["mean_returns_by_quantile"] =
        quantile_table_value(model.mean_returns_by_quantile);
    object["mean_return_spread"] = spread_value(model.mean_return_spread);
    factor_value::array_type ranks;
    ranks.reserve(model.rank_autocorrelation.size());
    for (const factor_rank_autocorrelation_series& item :
         model.rank_autocorrelation) {
        factor_value::object_type row;
        row["lag"] = static_cast<std::int64_t>(item.lag);
        row["dates"] = number_array(item.values.dates);
        row["values"] = double_array(item.values.values);
        ranks.emplace_back(std::move(row));
    }
    object["factor_rank_autocorrelation"] = factor_value{std::move(ranks)};
    factor_value::array_type turnover;
    turnover.reserve(model.quantile_turnover.size());
    for (const factor_turnover_series& item : model.quantile_turnover) {
        factor_value::object_type row;
        row["quantile"] = static_cast<std::int64_t>(item.quantile);
        row["lag"] = static_cast<std::int64_t>(item.lag);
        row["dates"] = number_array(item.values.dates);
        row["values"] = double_array(item.values.values);
        turnover.emplace_back(std::move(row));
    }
    object["quantile_turnover"] = factor_value{std::move(turnover)};
    if (model.event_analysis.has_value()) {
        const factor_event_model& event = *model.event_analysis;
        factor_value::object_type event_object;
        event_object["offsets"] = int_array(event.offsets);
        event_object["event_dates"] = number_array(event.event_dates);
        event_object["mean_returns"] = double_array(event.mean_returns);
        event_object["return_distribution"] =
            double_array(event.return_distribution);
        object["event_analysis"] = factor_value{std::move(event_object)};
    } else {
        object["event_analysis"] = factor_value{};
    }
    return factor_value{std::move(object)};
}

factor_value serializable_value(const factor_loss_report& report) {
    return loss_report_value(report);
}

std::string to_serializable(const factor_analysis_model& model) {
    return to_json_string(serializable_value(model));
}

std::string factor_analysis_model::to_serializable() const {
    return fincore::to_serializable(*this);
}

factor_value deserialize_serializable_value(const std::string_view text) {
    return parse_json_value(text);
}

factor_model_envelope deserialize_factor_model(const std::string_view text) {
    const factor_value root = parse_json_value(text);
    if (!root.is_object()) {
        throw input_error{"serialized factor model must be an object"};
    }
    factor_model_envelope envelope;
    envelope.schema = read_string(require_key(root, "schema"));
    if (envelope.schema != factor_serialization_schema) {
        throw input_error{"unsupported factor model serialization schema"};
    }
    const std::int64_t version =
        read_integer(require_key(root, "fingerprint_version"));
    if (version < 0 || version > std::numeric_limits<std::uint32_t>::max()) {
        throw input_error{"invalid factor model fingerprint version"};
    }
    envelope.fingerprint_version = static_cast<std::uint32_t>(version);
    envelope.result_fingerprint =
        read_string(require_key(root, "result_fingerprint"));
    envelope.config = read_config(require_key(root, "config"));
    envelope.forward_periods =
        read_strings(require_key(root, "forward_periods"));
    envelope.loss_report = read_loss_report(require_key(root, "loss_report"));
    return envelope;
}

std::string detail::canonical_factor_value(const factor_value& value) {
    std::string output;
    output.reserve(256U);
    // Length-prefixed big-endian encoding: a payload never needs delimiter
    // escaping, so a string containing a tag letter cannot imitate another
    // value, and nesting is unambiguous without a recursive terminator.
    const auto encode = [&output](const factor_value& item, const auto& self) -> void {
        if (item.is_null()) {
            output += "N";
            return;
        }
        if (item.is_bool()) {
            output += item.as_bool() ? "B1" : "B0";
            return;
        }
        if (item.is_integer()) {
            output += "I";
            append_big_endian(output,
                              static_cast<std::uint64_t>(item.as_integer()));
            return;
        }
        if (item.is_number()) {
            output += "D";
            double number = item.as_number();
            // Quiet-NaN normalization keeps signalling NaN payloads and the
            // platform-specific NaN bits out of the fingerprint.
            if (std::isnan(number)) {
                number = std::numeric_limits<double>::quiet_NaN();
            }
            std::uint64_t bits = 0U;
            std::memcpy(&bits, &number, sizeof(bits));
            append_big_endian(output, bits);
            return;
        }
        if (item.is_string()) {
            const std::string& text = item.as_string();
            output += "S";
            append_big_endian(output, text.size());
            output += text;
            return;
        }
        if (item.is_array()) {
            output += "A";
            append_big_endian(output, item.as_array().size());
            for (const factor_value& entry : item.as_array()) {
                self(entry, self);
            }
            return;
        }
        output += "O";
        append_big_endian(output, item.as_object().size());
        // factor_value objects are ordered containers, so the emitted key order
        // is fixed and independent of the order entries were inserted in.
        for (const auto& [key, entry] : item.as_object()) {
            append_big_endian(output, key.size());
            output += key;
            self(entry, self);
        }
    };
    // Tagged so a future encoding change cannot silently collide with an
    // existing fingerprint.
    output += "fincore-factor-value-v1";
    encode(value, encode);
    return output;
}

std::string fingerprint_value(const factor_value& value) {
    const std::string canonical = detail::canonical_factor_value(value);
    return detail::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(canonical.data()),
        canonical.size()));
}

std::string fingerprint_value(const factor_analysis_model& model) {
    return fingerprint_value(serializable_value(model));
}

std::string fingerprint_value(const factor_loss_report& report) {
    return fingerprint_value(serializable_value(report));
}

dataframe_snapshot::dataframe_snapshot(dataframe value)
    : data_(std::move(value)), content_sha256_(dataframe_content_sha256(data_)) {}

dataframe_snapshot snapshot_pandas(const dataframe& value) {
    // StdDataFrame copies own their storage, so this is a recursive native
    // snapshot equivalent to the pinned pandas pickle round-trip.
    return dataframe_snapshot{value};
}

dataframe_snapshot snapshot_pandas(dataframe&& value) {
    return dataframe_snapshot{std::move(value)};
}

namespace {

constexpr const char* research_result_tables[] = {
    "signals",     "labels",      "ic",          "ic_summary",
    "quantile_returns", "turnover", "spreads",  "stability",
    "correlation", "comparisons", "coverage",   "diagnostics"};

factor_value cell(const double value) {
    return std::isfinite(value) ? factor_value{value} : factor_value{};
}

factor_value cell(const timestamp value) { return factor_value{value}; }

factor_value cell(const std::string& value) { return factor_value{value}; }

factor_value cell(const std::size_t value) {
    return factor_value{static_cast<std::int64_t>(value)};
}

void append_manifest(factor_value::object_type& object,
                     const std::map<std::string, std::string>& manifest) {
    factor_value::object_type entries;
    for (const auto& [key, value] : manifest) {
        entries[key] = value;
    }
    object["manifest"] = factor_value{std::move(entries)};
}

research_table make_table(std::string name, std::vector<std::string> columns) {
    research_table table;
    table.name = std::move(name);
    table.columns = std::move(columns);
    return table;
}

research_table signals_table(const research_result& result) {
    research_table table = make_table(
        "signals", {"date", "asset", "factor", "direction", "value",
                    "processed_value", "preprocess", "group"});
    for (const research_signal_row& row : result.signals) {
        table.rows.push_back({cell(row.date), cell(row.asset), cell(row.factor),
                              cell(static_cast<std::int64_t>(row.direction)),
                              cell(row.value), cell(row.processed_value),
                              cell(row.preprocess), cell(row.group)});
    }
    return table;
}

research_table labels_table(const research_result& result) {
    research_table table =
        make_table("labels", {"date", "asset", "period", "value"});
    for (const research_label_row& row : result.labels) {
        table.rows.push_back(
            {cell(row.date), cell(row.asset), cell(row.period), cell(row.value)});
    }
    return table;
}

research_table ic_table(const research_result& result) {
    research_table table = make_table("ic", {"date", "group", "label", "value"});
    const factor_ic_table& ic = result.information_coefficient;
    const std::size_t labels = ic.labels.size();
    const std::size_t rows = labels == 0U ? 0U : ic.values.size() / labels;
    for (std::size_t row = 0; row < rows; ++row) {
        const std::string group =
            row < ic.groups.size() ? ic.groups[row] : std::string{};
        const timestamp date = row < ic.dates.size() ? ic.dates[row] : 0;
        for (std::size_t label = 0; label < labels; ++label) {
            table.rows.push_back(
                {cell(date), cell(group), cell(ic.labels[label]),
                 cell(ic.values[row * labels + label])});
        }
    }
    return table;
}

research_table ic_summary_table(const research_result& result) {
    research_table table = make_table("ic_summary", {"group", "label", "mean"});
    const factor_ic_summary& summary = result.information_coefficient_summary;
    const std::size_t labels = summary.labels.size();
    const std::size_t rows =
        labels == 0U ? 0U : summary.means.size() / labels;
    for (std::size_t row = 0; row < rows; ++row) {
        const std::string group =
            row < summary.groups.size() ? summary.groups[row] : std::string{};
        for (std::size_t label = 0; label < labels; ++label) {
            table.rows.push_back({cell(group), cell(summary.labels[label]),
                                  cell(summary.means[row * labels + label])});
        }
    }
    return table;
}

research_table quantile_returns_table(const research_result& result) {
    research_table table =
        make_table("quantile_returns",
                   {"quantile", "group", "label", "mean", "standard_error"});
    const quantile_return_table& returns = result.quantile_returns;
    const std::size_t labels = returns.labels.size();
    const std::size_t rows = labels == 0U ? 0U : returns.means.size() / labels;
    for (std::size_t row = 0; row < rows; ++row) {
        const std::string group =
            row < returns.groups.size() ? returns.groups[row] : std::string{};
        const int quantile =
            row < returns.quantiles.size() ? returns.quantiles[row] : 0;
        for (std::size_t label = 0; label < labels; ++label) {
            const std::size_t index = row * labels + label;
            table.rows.push_back(
                {cell(static_cast<std::int64_t>(quantile)), cell(group),
                 cell(returns.labels[label]), cell(returns.means[index]),
                 cell(index < returns.standard_errors.size()
                          ? returns.standard_errors[index]
                          : std::numeric_limits<double>::quiet_NaN())});
        }
    }
    return table;
}

research_table series_table(const std::string& name,
                            const std::vector<factor_series_result>& series,
                            const std::vector<int>& lags) {
    research_table table = make_table(name, {"lag", "date", "value"});
    for (std::size_t index = 0; index < series.size(); ++index) {
        const int lag = index < lags.size() ? lags[index]
                                            : static_cast<int>(index) + 1;
        const factor_series_result& values = series[index];
        for (std::size_t row = 0; row < values.dates.size(); ++row) {
            table.rows.push_back(
                {cell(static_cast<std::int64_t>(lag)), cell(values.dates[row]),
                 cell(values.values[row])});
        }
    }
    return table;
}

research_table spread_table(const research_result& result) {
    research_table table = make_table("spreads", {"period", "value"});
    for (std::size_t index = 0; index < result.spread.size(); ++index) {
        table.rows.push_back(
            {cell("period_" + std::to_string(index)), cell(result.spread[index])});
    }
    return table;
}

research_table coverage_table(const research_result& result) {
    research_table table = make_table("coverage", {"period", "value"});
    for (std::size_t index = 0; index < result.coverage.size(); ++index) {
        table.rows.push_back({cell("period_" + std::to_string(index)),
                              cell(result.coverage[index])});
    }
    return table;
}

research_table diagnostics_table(const research_result& result) {
    research_table table = make_table("diagnostics", {"message"});
    for (const std::string& message : result.diagnostics) {
        table.rows.push_back({cell(message)});
    }
    return table;
}

research_table empty_table(const std::string& name,
                           std::vector<std::string> columns) {
    return make_table(name, std::move(columns));
}

}  // namespace

research_table research_result::table(const std::string_view name) const {
    const std::string key(name);
    if (key == "signals") {
        return signals_table(*this);
    }
    if (key == "labels") {
        return labels_table(*this);
    }
    if (key == "ic") {
        return ic_table(*this);
    }
    if (key == "ic_summary") {
        return ic_summary_table(*this);
    }
    if (key == "quantile_returns") {
        return quantile_returns_table(*this);
    }
    if (key == "turnover") {
        return series_table("turnover", turnover, {});
    }
    if (key == "stability") {
        return series_table("stability", stability, stability_lags);
    }
    if (key == "spreads") {
        return spread_table(*this);
    }
    if (key == "coverage") {
        return coverage_table(*this);
    }
    if (key == "diagnostics") {
        return diagnostics_table(*this);
    }
    if (key == "correlation") {
        return empty_table("correlation",
                           {"left", "right", "mean_correlation",
                            "std_correlation", "n_dates"});
    }
    if (key == "comparisons") {
        return empty_table("comparisons",
                           {"comparison", "left", "right", "period", "mean_ic",
                            "icir", "mean_ic_left", "mean_ic_right",
                            "icir_left", "icir_right", "n_dates_left",
                            "n_dates_right"});
    }
    throw research_dataset_error("unknown result table '" + key + "'",
                                 "unknown_result_table",
                                 {{"table", key}});
}

factor_value serializable_value(const research_result& result) {
    factor_value::object_type object;
    object["status"] = result.status;
    object["schema_version"] =
        static_cast<std::int64_t>(result.schema_version);
    append_manifest(object, result.manifest);
    factor_value::object_type tables;
    factor_value::object_type columns;
    for (const char* name : research_result_tables) {
        const research_table table = result.table(name);
        factor_value::array_type rows;
        rows.reserve(table.rows.size());
        for (const std::vector<factor_value>& row : table.rows) {
            factor_value::object_type entry;
            for (std::size_t index = 0;
                 index < table.columns.size() && index < row.size(); ++index) {
                entry[table.columns[index]] = row[index];
            }
            rows.emplace_back(std::move(entry));
        }
        tables[name] = factor_value{std::move(rows)};
        factor_value::array_type column_names;
        column_names.reserve(table.columns.size());
        for (const std::string& column : table.columns) {
            column_names.emplace_back(column);
        }
        columns[name] = factor_value{std::move(column_names)};
    }
    object["tables"] = factor_value{std::move(tables)};
    object["columns"] = factor_value{std::move(columns)};
    return factor_value{std::move(object)};
}

std::string research_result::to_serializable() const {
    return to_json_string(serializable_value(*this));
}

std::string fingerprint_value(const research_result& result) {
    return fingerprint_value(serializable_value(result));
}

// --- Research specification normalization --------------------------------

namespace {

constexpr const char* research_labels[] = {"close_to_close",
                                           "next_open_to_open", "precomputed"};
constexpr const char* research_preprocess_methods[] = {
    "cross_section_rank", "cross_section_zscore", "none"};

void validate_string_tuple(const std::vector<std::string>& values,
                           const std::string& field, bool allow_empty) {
    if (values.empty() && !allow_empty) {
        throw research_config_error(field + " must not be empty",
                                    "spec_field_empty", {{"field", field}});
    }
    std::vector<std::string> seen;
    for (const std::string& value : values) {
        if (value.empty()) {
            throw research_config_error(
                field + " must contain non-empty strings", "spec_field_value",
                {{"field", field}});
        }
        if (std::find(seen.begin(), seen.end(), value) != seen.end()) {
            throw research_config_error(
                field + " must not contain duplicates",
                "spec_field_duplicate", {{"field", field}});
        }
        seen.push_back(value);
    }
}

void validate_int_tuple(const std::vector<int>& values,
                        const std::string& field, bool allow_empty) {
    if (values.empty() && !allow_empty) {
        throw research_config_error(field + " must not be empty",
                                    "spec_field_empty", {{"field", field}});
    }
    std::vector<int> seen;
    for (const int value : values) {
        if (value < 1) {
            throw research_config_error(
                field + " must be at least 1", "spec_field_value",
                {{"field", field}, {"value", std::to_string(value)}});
        }
        if (std::find(seen.begin(), seen.end(), value) != seen.end()) {
            throw research_config_error(
                field + " must not contain duplicates",
                "spec_field_duplicate", {{"field", field}});
        }
        seen.push_back(value);
    }
}

void validate_positive(const int value, const std::string& field) {
    if (value < 1) {
        throw research_config_error(field + " must be at least 1",
                                    "spec_field_value",
                                    {{"field", field},
                                     {"value", std::to_string(value)}});
    }
}

std::string required_string(const std::optional<std::string>& value,
                            const std::string& field) {
    if (!value.has_value()) {
        throw research_config_error(field + " must be a string",
                                    "spec_field_type", {{"field", field}});
    }
    return *value;
}

}  // namespace

research_evaluation_spec normalize_research_evaluation_spec(
    const research_evaluation_input& values) {
    if (!values.unknown_fields.empty()) {
        std::string fields;
        for (const std::string& field : values.unknown_fields) {
            if (!fields.empty()) {
                fields += ", ";
            }
            fields += field;
        }
        throw research_config_error("unknown research evaluation spec fields",
                                    "unknown_spec_field", {{"fields", fields}});
    }
    if (!values.targets.has_value()) {
        throw research_config_error("targets must be a sequence of strings",
                                    "spec_field_type", {{"field", "targets"}});
    }
    research_evaluation_spec spec;
    spec.targets = *values.targets;
    validate_string_tuple(spec.targets, "targets", false);

    if (!values.start.has_value() || !values.end.has_value()) {
        const std::string field = !values.start.has_value() ? "start" : "end";
        throw research_config_error(field + " must be a timestamp",
                                    "spec_field_type", {{"field", field}});
    }
    spec.start = *values.start;
    spec.end = *values.end;
    if (spec.start > spec.end) {
        throw research_config_error("spec.start must not be after spec.end",
                                    "spec_window", {});
    }
    if (values.snapshot_id.has_value()) {
        spec.snapshot_id = *values.snapshot_id;
    }
    if (values.price_basis.has_value()) {
        spec.price_basis = *values.price_basis;
    }
    if (values.calendar_id.has_value()) {
        spec.calendar_id = *values.calendar_id;
    }
    if (values.frequency.has_value()) {
        spec.frequency = *values.frequency;
    }
    if (values.source_timezone.has_value()) {
        spec.source_timezone = *values.source_timezone;
    }
    if (values.feed_time_kind.has_value()) {
        spec.feed_time_kind = *values.feed_time_kind;
    }
    if (values.periods.has_value()) {
        spec.periods = *values.periods;
    }
    validate_int_tuple(spec.periods, "periods", false);

    if (values.label.has_value()) {
        spec.label = *values.label;
    }
    if (spec.label.empty()) {
        throw research_config_error("label must not be empty",
                                    "spec_field_empty", {{"field", "label"}});
    }
    const bool known_label =
        std::find(std::begin(research_labels), std::end(research_labels),
                  spec.label) != std::end(research_labels);
    if (!known_label) {
        throw research_config_error("unsupported label '" + spec.label + "'",
                                    "unsupported_label",
                                    {{"label", spec.label}});
    }
    if (values.precomputed_entry.has_value()) {
        spec.precomputed_entry = *values.precomputed_entry;
    }
    if (values.preprocess.has_value()) {
        spec.preprocess = *values.preprocess;
    }
    if (std::find(std::begin(research_preprocess_methods),
                  std::end(research_preprocess_methods),
                  spec.preprocess) == std::end(research_preprocess_methods)) {
        throw research_config_error(
            "unsupported preprocess '" + spec.preprocess + "'",
            "unsupported_preprocess", {{"preprocess", spec.preprocess}});
    }
    if (values.quantiles.has_value()) {
        spec.quantiles = *values.quantiles;
    }
    validate_positive(spec.quantiles, "quantiles");
    if (values.min_assets.has_value()) {
        spec.min_assets = *values.min_assets;
    }
    validate_positive(spec.min_assets, "min_assets");
    if (values.min_quantile_assets.has_value()) {
        spec.min_quantile_assets = *values.min_quantile_assets;
    }
    validate_positive(spec.min_quantile_assets, "min_quantile_assets");
    if (values.max_loss.has_value()) {
        spec.max_loss = *values.max_loss;
    }
    if (!std::isfinite(spec.max_loss) || spec.max_loss < 0.0 ||
        spec.max_loss > 1.0) {
        throw research_config_error("max_loss must be between 0 and 1",
                                    "spec_field_value",
                                    {{"field", "max_loss"}});
    }
    if (values.stability_lags.has_value()) {
        spec.stability_lags = *values.stability_lags;
    }
    validate_int_tuple(spec.stability_lags, "stability_lags", true);
    if (values.direction_overrides.has_value()) {
        spec.direction_overrides = *values.direction_overrides;
    }
    for (const auto& [name, direction] : spec.direction_overrides) {
        if (name.empty() || (direction != 1 && direction != -1)) {
            throw research_config_error(
                "direction_overrides values must be +1 or -1",
                "spec_field_value", {{"field", "direction_overrides"}});
        }
    }
    if (values.combinations.has_value()) {
        spec.combinations = *values.combinations;
    }
    for (const auto& [name, weights] : spec.combinations) {
        if (name.empty() || weights.empty()) {
            throw research_config_error(
                "combinations require at least one non-zero weight",
                "spec_field_value", {{"combination", name}});
        }
        bool non_zero = false;
        for (const auto& [factor, weight] : weights) {
            if (factor.empty() || !std::isfinite(weight)) {
                throw research_config_error(
                    "combination weights must be finite numbers",
                    "spec_field_value",
                    {{"combination", name}, {"factor", factor}});
            }
            non_zero = non_zero || weight != 0.0;
        }
        if (!non_zero) {
            throw research_config_error(
                "combinations require at least one non-zero weight",
                "spec_field_value", {{"combination", name}});
        }
    }
    if (values.comparisons.has_value()) {
        spec.comparisons = *values.comparisons;
    }
    for (const auto& [name, pair] : spec.comparisons) {
        if (name.empty() || pair.first.empty() || pair.second.empty()) {
            throw research_config_error("comparison names must be non-empty",
                                        "spec_field_value",
                                        {{"field", "comparisons"}});
        }
        if (pair.first == pair.second) {
            throw research_config_error("comparison sides must be distinct",
                                        "spec_field_value",
                                        {{"comparison", name}});
        }
    }
    if (values.segments.has_value()) {
        spec.segments = *values.segments;
    }
    for (const auto& [name, bounds] : spec.segments) {
        if (name.empty()) {
            throw research_config_error("segment names must be non-empty",
                                        "spec_field_value",
                                        {{"field", "segments"}});
        }
    }
    return spec;
}

research_spec to_research_spec(const research_evaluation_spec& values) {
    research_spec spec;
    spec.targets = values.targets;
    spec.start = values.start;
    spec.end = values.end;
    spec.periods = values.periods;
    spec.quantiles = values.quantiles;
    spec.min_assets = values.min_assets;
    spec.min_quantile_assets = values.min_quantile_assets;
    spec.label = values.label;
    spec.preprocess = values.preprocess;
    spec.max_loss = values.max_loss;
    spec.stability_lags = values.stability_lags;
    spec.direction_overrides = values.direction_overrides;
    spec.segments = values.segments;
    spec.comparisons = values.comparisons;
    spec.combinations = values.combinations;
    spec.snapshot_id = values.snapshot_id;
    spec.price_basis = values.price_basis;
    spec.calendar_id = values.calendar_id;
    spec.frequency = values.frequency;
    return spec;
}

}  // namespace fincore
