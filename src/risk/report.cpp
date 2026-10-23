#include <fincore/risk/report.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double report_tolerance = 1e-12;
constexpr double report_tolerance_floor = 1e-15;

bool is_sha256_hex(const std::string_view value) {
    if (value.size() != 64U) {
        return false;
    }
    for (const char character : value) {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

bool close_enough(const double actual, const double expected) {
    if (!std::isfinite(actual) || !std::isfinite(expected)) {
        return false;
    }
    return std::abs(actual - expected) <=
           report_tolerance_floor + report_tolerance * std::abs(expected);
}

std::string python_float(const double value,
                         const std::string_view context) {
    if (!std::isfinite(value)) {
        throw input_error{std::string(context) +
                          " must not contain non-finite numeric values"};
    }
    if (value == 0.0) {
        return std::signbit(value) ? "-0.0" : "0.0";
    }

    char buffer[64];
    const auto conversion =
        std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (conversion.ec != std::errc{}) {
        throw input_error{std::string(context) +
                          " could not be serialized"};
    }
    const std::string text(buffer, conversion.ptr);

    std::size_t position = 0U;
    const bool negative = text[position] == '-';
    if (negative) {
        ++position;
    }
    std::string digits;
    std::size_t digits_before_point = 0U;
    bool seen_point = false;
    while (position < text.size() && text[position] != 'e' &&
           text[position] != 'E') {
        if (text[position] == '.') {
            seen_point = true;
        } else {
            digits.push_back(text[position]);
            if (!seen_point) {
                ++digits_before_point;
            }
        }
        ++position;
    }
    int exponent = 0;
    if (position < text.size()) {
        exponent = std::atoi(text.c_str() + position + 1U);
    }
    const int decimal_position =
        static_cast<int>(digits_before_point) + exponent;

    std::string result;
    if (negative) {
        result.push_back('-');
    }
    if (decimal_position <= -4 || decimal_position > 16) {
        result.push_back(digits.front());
        if (digits.size() > 1U) {
            result.push_back('.');
            result.append(digits.substr(1U));
        }
        const int scientific_exponent = decimal_position - 1;
        result.push_back('e');
        result.push_back(scientific_exponent < 0 ? '-' : '+');
        const int magnitude = std::abs(scientific_exponent);
        if (magnitude < 10) {
            result.push_back('0');
        }
        result += std::to_string(magnitude);
        return result;
    }

    if (decimal_position <= 0) {
        result += "0.";
        result.append(static_cast<std::size_t>(-decimal_position), '0');
        result += digits;
        return result;
    }
    if (static_cast<std::size_t>(decimal_position) >= digits.size()) {
        result += digits;
        result.append(
            static_cast<std::size_t>(decimal_position) - digits.size(), '0');
        result += ".0";
        return result;
    }
    result.append(digits.substr(0U,
                                static_cast<std::size_t>(decimal_position)));
    result.push_back('.');
    result.append(digits.substr(static_cast<std::size_t>(decimal_position)));
    return result;
}

std::string format_timestamp(const timestamp value) {
    constexpr std::int64_t nanoseconds_per_second = 1'000'000'000LL;
    constexpr std::int64_t seconds_per_day = 86'400LL;
    std::int64_t seconds = value / nanoseconds_per_second;
    std::int64_t nanoseconds = value % nanoseconds_per_second;
    if (nanoseconds < 0) {
        nanoseconds += nanoseconds_per_second;
        --seconds;
    }
    std::int64_t days = seconds / seconds_per_day;
    std::int64_t seconds_of_day = seconds % seconds_per_day;
    if (seconds_of_day < 0) {
        seconds_of_day += seconds_per_day;
        --days;
    }

    days += 719468;
    const std::int64_t era =
        (days >= 0 ? days : days - 146096) / 146097;
    const auto day_of_era =
        static_cast<std::uint64_t>(days - era * 146097);
    const auto year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
         day_of_era / 146096U) /
        365U;
    const std::int64_t year_base =
        static_cast<std::int64_t>(year_of_era) + era * 400;
    const auto day_of_year =
        day_of_era -
        (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    const auto month_prime = (5U * day_of_year + 2U) / 153U;
    const auto day = day_of_year - (153U * month_prime + 2U) / 5U + 1U;
    const auto month = month_prime < 10U ? month_prime + 3U
                                         : month_prime - 9U;
    const std::int64_t year =
        year_base + (month <= 2U ? 1 : 0);

    char buffer[64];
    std::snprintf(
        buffer, sizeof(buffer), "%04lld-%02d-%02dT%02d:%02d:%02d",
        static_cast<long long>(year), static_cast<int>(month),
        static_cast<int>(day),
        static_cast<int>(seconds_of_day / 3600),
        static_cast<int>((seconds_of_day % 3600) / 60),
        static_cast<int>(seconds_of_day % 60));
    std::string result(buffer);
    if (nanoseconds == 0) {
        return result;
    }
    result.push_back('.');
    if (nanoseconds % 1000 == 0) {
        std::snprintf(buffer, sizeof(buffer), "%06lld",
                      static_cast<long long>(nanoseconds / 1000));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%09lld",
                      static_cast<long long>(nanoseconds));
    }
    result += buffer;
    return result;
}

void append_unicode_escape(std::string& output, std::uint32_t code_point) {
    if (code_point > 0x10FFFFU) {
        throw input_error{"report text must be valid UTF-8"};
    }
    static constexpr char hex[] = "0123456789abcdef";
    const auto append_unit = [&output](const std::uint16_t unit) {
        output += "\\u";
        output.push_back(hex[(unit >> 12U) & 0xFU]);
        output.push_back(hex[(unit >> 8U) & 0xFU]);
        output.push_back(hex[(unit >> 4U) & 0xFU]);
        output.push_back(hex[unit & 0xFU]);
    };
    if (code_point <= 0xFFFFU) {
        append_unit(static_cast<std::uint16_t>(code_point));
        return;
    }
    const std::uint32_t adjusted = code_point - 0x10000U;
    append_unit(static_cast<std::uint16_t>(0xD800U + (adjusted >> 10U)));
    append_unit(static_cast<std::uint16_t>(0xDC00U + (adjusted & 0x3FFU)));
}

void append_json_string(std::string& output, const std::string_view text) {
    output.push_back('"');
    for (std::size_t position = 0U; position < text.size();) {
        const auto character =
            static_cast<unsigned char>(text[position]);
        if (character < 0x80U) {
            switch (character) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (character < 0x20U) {
                        append_unicode_escape(output, character);
                    } else {
                        output.push_back(static_cast<char>(character));
                    }
                    break;
            }
            ++position;
            continue;
        }

        std::uint32_t code_point = 0U;
        std::size_t length = 0U;
        if ((character & 0xE0U) == 0xC0U) {
            code_point = character & 0x1FU;
            length = 2U;
        } else if ((character & 0xF0U) == 0xE0U) {
            code_point = character & 0x0FU;
            length = 3U;
        } else if ((character & 0xF8U) == 0xF0U) {
            code_point = character & 0x07U;
            length = 4U;
        } else {
            throw input_error{"report text must be valid UTF-8"};
        }
        if (position + length > text.size()) {
            throw input_error{"report text must be valid UTF-8"};
        }
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(text[position + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                throw input_error{"report text must be valid UTF-8"};
            }
            code_point = (code_point << 6U) | (continuation & 0x3FU);
        }
        append_unicode_escape(output, code_point);
        position += length;
    }
    output.push_back('"');
}

class json_writer {
  public:
    explicit json_writer(std::string& output) : output_(output) {}

    void begin_object() {
        value_prefix();
        output_.push_back('{');
        levels_.push_back({true, true});
    }

    void end_object() {
        const bool empty = levels_.back().first;
        levels_.pop_back();
        close_container('}', empty);
    }

    void begin_array() {
        value_prefix();
        output_.push_back('[');
        levels_.push_back({true, false});
    }

    void end_array() {
        const bool empty = levels_.back().first;
        levels_.pop_back();
        close_container(']', empty);
    }

    void key(const std::string_view name) {
        auto& current = levels_.back();
        if (!current.first) {
            output_.push_back(',');
        }
        current.first = false;
        output_.push_back('\n');
        append_indent(levels_.size());
        append_json_string(output_, name);
        output_ += ": ";
        after_key_ = true;
    }

    void value_string(const std::string_view value) {
        value_prefix();
        append_json_string(output_, value);
    }

    void value_number(const double value, const std::string_view context) {
        value_prefix();
        output_ += python_float(value, context);
    }

    void value_integer(const std::int64_t value) {
        value_prefix();
        output_ += std::to_string(value);
    }

    void value_boolean(const bool value) {
        value_prefix();
        output_ += value ? "true" : "false";
    }

    void value_null() {
        value_prefix();
        output_ += "null";
    }

  private:
    struct level {
        bool first;
        bool object;
    };

    void value_prefix() {
        if (after_key_) {
            after_key_ = false;
            return;
        }
        if (levels_.empty()) {
            return;
        }
        auto& current = levels_.back();
        if (!current.first) {
            output_.push_back(',');
        }
        current.first = false;
        output_.push_back('\n');
        append_indent(levels_.size());
    }

    void close_container(const char closing, const bool empty) {
        if (!empty) {
            output_.push_back('\n');
            append_indent(levels_.size());
        }
        output_.push_back(closing);
    }

    void append_indent(const std::size_t depth) {
        output_.append(depth * 2U, ' ');
    }

    std::string& output_;
    std::vector<level> levels_;
    bool after_key_ = false;
};

std::string_view backtest_method_name(const risk_backtest_method method) {
    switch (method) {
        case risk_backtest_method::var:
            return "var";
        case risk_backtest_method::es:
            return "es";
    }
    return "var";
}

bool backtest_matches(const risk_backtest_result& actual,
                      const risk_backtest_result& expected) {
    return actual.method == expected.method &&
           actual.confidence_level == expected.confidence_level &&
           actual.observations == expected.observations &&
           actual.exceptions == expected.exceptions &&
           actual.inputs_digest == expected.inputs_digest &&
           actual.diagnostics == expected.diagnostics &&
           actual.status == expected.status &&
           close_enough(actual.expected_exceptions,
                        expected.expected_exceptions) &&
           close_enough(actual.exception_rate, expected.exception_rate) &&
           close_enough(actual.kupiec_lr, expected.kupiec_lr) &&
           close_enough(actual.kupiec_pvalue, expected.kupiec_pvalue) &&
           close_enough(actual.christoffersen_lr,
                        expected.christoffersen_lr) &&
           close_enough(actual.christoffersen_pvalue,
                        expected.christoffersen_pvalue);
}

void require_reason(const walk_forward_var_result& result) {
    if (result.unsupported_reason.empty()) {
        throw input_error{
            std::string(walk_forward_status_name(result.status)) +
            " result must include a non-empty diagnostic reason"};
    }
}

void validate_ok_result(const walk_forward_var_result& result) {
    if (result.index.empty() || result.forecast.empty() ||
        result.realized.empty()) {
        throw input_error{
            "ok result must contain a non-empty forecast path"};
    }
    if (result.forecast.size() != result.realized.size() ||
        result.forecast.size() != result.index.size()) {
        throw input_error{
            "forecast and realized must have the same index"};
    }
    for (const double value : result.forecast) {
        if (!std::isfinite(value)) {
            throw input_error{
                "ok result forecast and realized must contain only finite "
                "values"};
        }
    }
    for (const double value : result.realized) {
        if (!std::isfinite(value)) {
            throw input_error{
                "ok result forecast and realized must contain only finite "
                "values"};
        }
    }
    if (result.refit_timestamps.empty()) {
        throw input_error{
            "ok result must contain at least one refit timestamp"};
    }
    if (result.refit_timestamps.front() != result.index.front()) {
        throw input_error{
            "the first refit timestamp must equal the first forecast "
            "timestamp"};
    }
    const std::size_t expected_refits =
        (result.index.size() + result.spec.refit_cadence - 1U) /
        result.spec.refit_cadence;
    if (result.refit_timestamps.size() != expected_refits) {
        throw input_error{
            "refit_timestamps must follow the configured refit cadence"};
    }
    for (std::size_t position = 0U;
         position < result.refit_timestamps.size(); ++position) {
        if (result.refit_timestamps[position] !=
            result.index[position * result.spec.refit_cadence]) {
            throw input_error{
                "refit_timestamps must follow the configured refit cadence"};
        }
    }
    if (result.fits.size() != result.refit_timestamps.size()) {
        throw input_error{
            "fit_parameters must have exactly one entry for each refit"};
    }
    for (std::size_t position = 0U; position < result.fits.size();
         ++position) {
        const auto& fit = result.fits[position];
        const std::string timestamp_text = format_timestamp(fit.date);
        if (fit.date != result.refit_timestamps[position]) {
            throw input_error{
                "fit_parameters must have exactly one entry for each refit"};
        }
        if (fit.n_observations != result.spec.window) {
            throw input_error{
                "fit_parameters for refit " + timestamp_text +
                " n_observations must equal the specification window"};
        }
        if (!std::isfinite(fit.mean) ||
            !std::isfinite(fit.standard_deviation) ||
            !std::isfinite(fit.forecast) ||
            fit.standard_deviation < 0.0) {
            throw input_error{
                "fit_parameters for refit " + timestamp_text +
                " must contain finite values"};
        }
        const std::size_t start = position * result.spec.refit_cadence;
        const std::size_t end = std::min(
            start + result.spec.refit_cadence, result.forecast.size());
        for (std::size_t index = start; index < end; ++index) {
            if (!close_enough(result.forecast[index], fit.forecast)) {
                throw input_error{
                    "fit_parameters for refit " + timestamp_text +
                    " must reproduce the forecast segment"};
            }
        }
        if (result.spec.distribution == "normal") {
            const double alpha = 1.0 - result.spec.confidence_level;
            const double expected =
                fit.standard_deviation == 0.0
                    ? fit.mean
                    : fit.mean + fit.standard_deviation *
                                     detail::inverse_standard_normal_cdf(
                                         alpha);
            if (!close_enough(fit.forecast, expected)) {
                throw input_error{
                    "fit_parameters for refit " + timestamp_text +
                    " must reproduce the forecast segment"};
            }
        }
    }
    if (!result.has_backtest) {
        throw input_error{"ok result must contain a backtest"};
    }
    if (result.backtest.method != risk_backtest_method::var) {
        throw input_error{"backtest method must be 'var'"};
    }
    if (result.backtest.confidence_level !=
        result.spec.confidence_level) {
        throw input_error{
            "backtest confidence_level must equal the specification"};
    }
    if (result.backtest.observations != result.forecast.size()) {
        throw input_error{
            "backtest observations must equal forecast length"};
    }
    if (!is_sha256_hex(result.backtest.inputs_digest)) {
        throw input_error{
            "backtest inputs_digest must be a lowercase SHA-256 hex digest"};
    }

    const auto forecast_frame =
        make_series(result.index, "var", result.forecast);
    const auto realized_frame =
        make_series(result.index, "realized", result.realized);
    const auto expected = backtest_var(
        forecast_frame, realized_frame, result.spec.confidence_level, 0.05,
        "var", "realized");
    if (!backtest_matches(result.backtest, expected)) {
        throw input_error{
            "backtest must match the forecast and realized path"};
    }
}

void validate_empty_result(const walk_forward_var_result& result) {
    if (!result.index.empty() || !result.forecast.empty() ||
        !result.realized.empty() || !result.refit_timestamps.empty() ||
        !result.fits.empty()) {
        throw input_error{
            std::string(walk_forward_status_name(result.status)) +
            " result must have an empty forecast path, realized path, and "
            "refit timestamps"};
    }
    if (result.has_backtest) {
        throw input_error{
            std::string(walk_forward_status_name(result.status)) +
            " result must not contain a backtest"};
    }
    require_reason(result);
}

void validate_result(const walk_forward_var_result& result) {
    validate_risk_model_spec(result.spec);
    if (!is_sha256_hex(result.inputs_digest)) {
        throw input_error{
            "inputs_digest must be a lowercase SHA-256 hex digest"};
    }
    for (std::size_t position = 1U; position < result.index.size();
         ++position) {
        if (!(result.index[position] > result.index[position - 1U])) {
            throw input_error{
                "forecast index must be sorted and duplicate-free"};
        }
    }
    if (result.status == walk_forward_status::ok) {
        validate_ok_result(result);
    } else {
        validate_empty_result(result);
    }
}

void write_specification(json_writer& writer,
                         const risk_model_spec& specification) {
    writer.begin_object();
    writer.key("confidence_level");
    writer.value_number(specification.confidence_level, "specification");
    writer.key("distribution");
    writer.value_string(specification.distribution);
    writer.key("forecast_target");
    writer.value_string(risk_forecast_target_name(
        specification.forecast_target));
    writer.key("horizon");
    writer.value_integer(static_cast<std::int64_t>(specification.horizon));
    writer.key("model_version");
    writer.value_string(specification.model_version);
    writer.key("refit_cadence");
    writer.value_integer(
        static_cast<std::int64_t>(specification.refit_cadence));
    writer.key("sign_convention");
    writer.value_string(specification.sign_convention);
    writer.key("tail");
    writer.value_string(risk_tail_name(specification.tail));
    writer.key("window");
    writer.value_integer(static_cast<std::int64_t>(specification.window));
    writer.end_object();
}

void write_diagnostics(json_writer& writer,
                       const risk_validation_report& report) {
    writer.begin_object();
    switch (report.status) {
        case walk_forward_status::ok:
            writer.key("backtest_status");
            writer.value_string(report.diagnostics.backtest_status);
            writer.key("forecast_count");
            writer.value_integer(static_cast<std::int64_t>(
                report.diagnostics.forecast_count));
            writer.key("method");
            writer.value_string(report.diagnostics.method);
            writer.key("quantile_method");
            writer.value_string(report.diagnostics.quantile_method);
            writer.key("refit_cadence");
            writer.value_integer(static_cast<std::int64_t>(
                report.diagnostics.refit_cadence));
            writer.key("window");
            writer.value_integer(static_cast<std::int64_t>(
                report.diagnostics.window));
            break;
        case walk_forward_status::insufficient_data:
            writer.key("n_observations");
            writer.value_integer(static_cast<std::int64_t>(
                report.diagnostics.n_observations));
            writer.key("reason");
            writer.value_string(report.diagnostics.reason);
            writer.key("window");
            writer.value_integer(static_cast<std::int64_t>(
                report.diagnostics.window));
            break;
        case walk_forward_status::unsupported:
            writer.key("reason");
            writer.value_string(report.diagnostics.reason);
            break;
    }
    writer.end_object();
}

void write_backtest_diagnostics(
    json_writer& writer,
    const std::map<std::string, double>& diagnostics) {
    writer.begin_object();
    for (const auto& [name, value] : diagnostics) {
        writer.key(name);
        if (name == "small_sample") {
            writer.value_boolean(value != 0.0);
        } else {
            writer.value_number(value, "backtest diagnostics");
        }
    }
    writer.end_object();
}

void write_backtest(json_writer& writer,
                    const risk_validation_report& report) {
    const auto& backtest = report.backtest;
    writer.begin_object();
    writer.key("christoffersen_lr");
    writer.value_number(backtest.christoffersen_lr, "backtest");
    writer.key("christoffersen_pvalue");
    writer.value_number(backtest.christoffersen_pvalue, "backtest");
    writer.key("confidence_level");
    writer.value_number(backtest.confidence_level, "backtest");
    writer.key("diagnostics");
    write_backtest_diagnostics(writer, backtest.diagnostics);
    writer.key("exception_rate");
    writer.value_number(backtest.exception_rate, "backtest");
    writer.key("exceptions");
    writer.value_integer(static_cast<std::int64_t>(backtest.exceptions));
    writer.key("expected_exceptions");
    writer.value_number(backtest.expected_exceptions, "backtest");
    writer.key("inputs_digest");
    writer.value_string(backtest.inputs_digest);
    writer.key("kupiec_lr");
    writer.value_number(backtest.kupiec_lr, "backtest");
    writer.key("kupiec_pvalue");
    writer.value_number(backtest.kupiec_pvalue, "backtest");
    writer.key("method");
    writer.value_string(backtest_method_name(backtest.method));
    writer.key("observations");
    writer.value_integer(
        static_cast<std::int64_t>(backtest.observations));
    writer.key("status");
    writer.value_string(risk_backtest_status_name(backtest.status));
    writer.key("traffic_light");
    writer.begin_object();
    writer.key("confidence_level");
    writer.value_number(backtest.confidence_level, "traffic light");
    writer.key("observations");
    writer.value_integer(
        static_cast<std::int64_t>(backtest.observations));
    writer.key("zone");
    writer.value_string(report.traffic_light_zone);
    writer.end_object();
    writer.end_object();
}

}  // namespace

risk_validation_report build_risk_validation_report(
    const walk_forward_var_result& result,
    std::optional<std::string> timestamp_index_name) {
    validate_result(result);

    risk_validation_report report;
    report.status = result.status;
    report.inputs_digest = result.inputs_digest;
    report.specification = result.spec;
    report.timestamp_index_name = std::move(timestamp_index_name);
    report.disclosure = std::string(basel_reference_disclosure);
    report.schema_version = risk_validation_report_schema_version;

    if (result.status == walk_forward_status::ok) {
        report.forecast_events.reserve(result.forecast.size());
        for (std::size_t position = 0U; position < result.forecast.size();
             ++position) {
            const double forecast = result.forecast[position];
            const double realized = result.realized[position];
            report.forecast_events.push_back(
                {result.index[position], forecast, realized,
                 realized < forecast,
                 position % result.spec.refit_cadence == 0U});
        }
        report.refits.reserve(result.fits.size());
        for (const auto& fit : result.fits) {
            report.refits.push_back({fit.date, fit.forecast, fit.mean,
                                     fit.standard_deviation,
                                     fit.n_observations});
        }
        report.diagnostics.method = result.spec.distribution;
        report.diagnostics.quantile_method =
            result.spec.distribution == "historical" ? "weibull"
                                                     : "normal_ppf";
        report.diagnostics.window = result.spec.window;
        report.diagnostics.refit_cadence = result.spec.refit_cadence;
        report.diagnostics.forecast_count = result.forecast.size();
        report.diagnostics.backtest_status =
            std::string(risk_backtest_status_name(result.backtest.status));
        report.has_backtest = true;
        report.backtest = result.backtest;
        report.traffic_light_zone = basel_traffic_light(
            result.backtest.exceptions, result.backtest.observations,
            result.backtest.confidence_level);
        return report;
    }

    report.diagnostics.reason = result.unsupported_reason;
    if (result.status == walk_forward_status::insufficient_data) {
        report.diagnostics.n_observations = result.n_observations;
        report.diagnostics.window = result.spec.window;
    }
    return report;
}

std::string risk_validation_report::to_json() const {
    validate_risk_model_spec(specification);

    std::string output;
    json_writer writer(output);
    writer.begin_object();
    writer.key("backtest");
    if (has_backtest) {
        write_backtest(writer, *this);
    } else {
        writer.value_null();
    }
    writer.key("diagnostics");
    write_diagnostics(writer, *this);
    writer.key("disclosure");
    writer.value_string(disclosure);
    writer.key("forecast_events");
    writer.begin_array();
    for (const auto& event : forecast_events) {
        writer.begin_object();
        writer.key("exception");
        writer.value_boolean(event.exception);
        writer.key("forecast");
        writer.value_number(event.forecast, "forecast event");
        writer.key("realized");
        writer.value_number(event.realized, "forecast event");
        writer.key("refit");
        writer.value_boolean(event.refit);
        writer.key("timestamp");
        writer.value_string(format_timestamp(event.date));
        writer.end_object();
    }
    writer.end_array();
    writer.key("inputs_digest");
    writer.value_string(inputs_digest);
    writer.key("refits");
    writer.begin_array();
    for (const auto& refit : refits) {
        writer.begin_object();
        writer.key("parameters");
        writer.begin_object();
        writer.key("forecast");
        writer.value_number(refit.forecast, "refit parameters");
        if (specification.distribution == "normal") {
            writer.key("mean");
            writer.value_number(refit.mean, "refit parameters");
            writer.key("n_observations");
            writer.value_number(
                static_cast<double>(refit.n_observations),
                "refit parameters");
            writer.key("standard_deviation");
            writer.value_number(refit.standard_deviation,
                                "refit parameters");
        } else {
            writer.key("n_observations");
            writer.value_number(
                static_cast<double>(refit.n_observations),
                "refit parameters");
            writer.key("quantile_method");
            writer.value_string("weibull");
        }
        writer.end_object();
        writer.key("timestamp");
        writer.value_string(format_timestamp(refit.date));
        writer.end_object();
    }
    writer.end_array();
    writer.key("schema_version");
    writer.value_integer(schema_version);
    writer.key("specification");
    write_specification(writer, specification);
    writer.key("status");
    writer.value_string(walk_forward_status_name(status));
    writer.key("timestamp_index_name");
    if (timestamp_index_name) {
        writer.value_string(*timestamp_index_name);
    } else {
        writer.value_null();
    }
    writer.key("timestamp_timezone");
    if (timestamp_timezone) {
        writer.value_string(*timestamp_timezone);
    } else {
        writer.value_null();
    }
    writer.end_object();
    output.push_back('\n');
    return output;
}

}  // namespace fincore
