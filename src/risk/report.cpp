#include <fincore/risk/report.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>
#include <fincore/errors.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
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

using detail::format_timestamp;
using detail::json_writer;
using detail::python_float;

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

namespace {

void require_finite_report_values(const risk_validation_report& report) {
    for (const auto& event : report.forecast_events) {
        if (!std::isfinite(event.forecast) || !std::isfinite(event.realized)) {
            throw fincore_error{validation_error(
                "forecast events must contain only finite values")};
        }
    }
    for (const auto& refit : report.refits) {
        if (!std::isfinite(refit.forecast) || !std::isfinite(refit.mean) ||
            !std::isfinite(refit.standard_deviation)) {
            throw fincore_error{
                validation_error("refits must contain only finite values")};
        }
    }
    if (report.has_backtest) {
        const auto& backtest = report.backtest;
        for (const double value :
             {backtest.confidence_level, backtest.expected_exceptions,
              backtest.exception_rate, backtest.kupiec_lr,
              backtest.kupiec_pvalue, backtest.christoffersen_lr,
              backtest.christoffersen_pvalue}) {
            if (!std::isfinite(value)) {
                throw fincore_error{validation_error(
                    "backtest fields must contain only finite values")};
            }
        }
    }
}

}  // namespace

risk_validation_report_dict risk_validation_report::to_dict() const {
    require_finite_report_values(*this);

    risk_validation_report_dict result;
    result.schema_version = schema_version;
    result.status = std::string{walk_forward_status_name(status)};
    result.inputs_digest = inputs_digest;
    result.timestamp_index_name = timestamp_index_name;
    result.timestamp_timezone = timestamp_timezone;
    result.specification = specification;
    result.forecast_events = forecast_events;
    result.refits = refits;
    result.diagnostics = diagnostics;
    if (has_backtest) {
        result.backtest = backtest;
    }
    result.traffic_light_zone = traffic_light_zone;
    result.disclosure = disclosure;
    return result;
}

std::filesystem::path risk_validation_report::write_json(
    const std::filesystem::path& path) const {
    // Validate once through the native dict projection so contract failures
    // surface as fincore_error rather than an I/O-side type error.
    static_cast<void>(to_dict());

    std::string text;
    try {
        text = to_json();
    } catch (const fincore_error&) {
        throw;
    } catch (const std::exception& error) {
        throw fincore_error{validation_error(error.what())};
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw fincore_error{validation_error(
            "could not open risk report path for writing", "path",
            path.string())};
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    if (!output) {
        throw fincore_error{validation_error(
            "could not write risk report JSON", "path", path.string())};
    }
    return path;
}

}  // namespace fincore
