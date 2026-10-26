#pragma once

#include <fincore/risk/walk_forward.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Disclosure stating that the Basel traffic-light and VaR backtesting fields
/// are a reference implementation, not regulatory approval or compliance
/// certification.
inline constexpr std::string_view basel_reference_disclosure =
    "Basel traffic-light and VaR backtesting fields are a reference "
    "implementation; they are not regulatory approval or compliance "
    "certification.";
/// Schema version emitted by risk validation reports.
inline constexpr int risk_validation_report_schema_version = 1;

/// One out-of-sample forecast event in a validation report.
struct risk_forecast_event {
    timestamp date;  ///< Forecast timestamp.
    double forecast;  ///< VaR forecast at `date`.
    double realized;  ///< Realized return at `date`.
    bool exception;  ///< Whether `realized` is strictly below `forecast`.
    bool refit;      ///< Whether `date` starts a refit segment.
};

/// Parameters recorded for one refit in a validation report.
struct risk_refit_event {
    timestamp date;  ///< Refit timestamp.
    double forecast;  ///< VaR forecast produced by the refit.
    double mean;      ///< Estimation-window mean.
    double standard_deviation;  ///< Estimation-window sample standard
                                ///< deviation.
    std::size_t n_observations;  ///< Estimation-window length.
};

/// Diagnostic fields describing how a validation report was produced.
struct risk_report_diagnostics {
    std::string method;  ///< Distribution label from the specification.
    std::string quantile_method;  ///< `weibull` for historical and `normal_ppf`
                                  ///< for normal.
    std::size_t window = 0U;  ///< Estimation window length.
    std::size_t refit_cadence = 0U;  ///< Forecast steps per refit.
    std::size_t forecast_count = 0U;  ///< Number of forecast events.
    std::string backtest_status;  ///< Backtest status name (`pass`, `fail`,
                                  ///< `inconclusive`, or `experimental`).
    std::string reason;  ///< Reason for a non-ok status; empty for ok reports.
    std::size_t n_observations = 0U;  ///< Input observation count; populated
                                      ///< for insufficient-data reports.
};

/// Native detached dictionary projection of ``risk_validation_report``.
///
/// This is the C++ equivalent of the source's JSON-compatible ``to_dict``
/// mapping: every native report field is represented exactly once. Python
/// object identity and mapping subclassing are not claimed.
struct risk_validation_report_dict {
    int schema_version = risk_validation_report_schema_version;  ///< Report schema version.
    std::string status;  ///< Status name: `ok`, `insufficient_data`, or
                         ///< `unsupported`.
    std::string inputs_digest;  ///< Lowercase SHA-256 hex digest from the
                                ///< walk-forward result.
    std::optional<std::string> timestamp_index_name;  ///< Optional index label.
    std::optional<std::string> timestamp_timezone;  ///< Optional index timezone
                                                    ///< label; not populated by
                                                    ///< the native builder.
    risk_model_spec specification;  ///< Specification echoed from the result.
    std::vector<risk_forecast_event> forecast_events;  ///< Forecast events;
                                                       ///< empty for non-ok
                                                       ///< reports.
    std::vector<risk_refit_event> refits;  ///< Refit parameters; empty for
                                           ///< non-ok reports.
    risk_report_diagnostics diagnostics;  ///< Report diagnostics.
    std::optional<risk_backtest_result> backtest;  ///< Backtest payload; empty
                                                   ///< when the report has no
                                                   ///< backtest.
    std::string traffic_light_zone;  ///< Basel zone; empty without a backtest.
    std::string disclosure;  ///< Basel reference disclosure text.
};

/// Versioned validation report projected from a walk-forward VaR result.
struct risk_validation_report {
    walk_forward_status status = walk_forward_status::unsupported;  ///< Run status.
    std::string inputs_digest;  ///< Lowercase SHA-256 hex digest from the result.
    risk_model_spec specification;  ///< Specification echoed from the result.
    std::vector<risk_forecast_event> forecast_events;  ///< Forecast events;
                                                       ///< empty for non-ok
                                                       ///< reports.
    std::vector<risk_refit_event> refits;  ///< Refit parameters; empty for
                                           ///< non-ok reports.
    risk_report_diagnostics diagnostics;  ///< Report diagnostics.
    bool has_backtest = false;  ///< Whether `backtest` holds a backtest.
    risk_backtest_result backtest{};  ///< Backtest payload; meaningful only
                                      ///< when `has_backtest` is true.
    std::string traffic_light_zone;  ///< Basel zone; empty without a backtest.
    std::optional<std::string> timestamp_index_name;  ///< Optional index label
                                                      ///< supplied to the
                                                      ///< builder.
    std::optional<std::string> timestamp_timezone;  ///< Optional index timezone
                                                    ///< label; the native builder
                                                    ///< leaves it empty.
    std::string disclosure{basel_reference_disclosure};  ///< Basel reference
                                                         ///< disclosure text.
    int schema_version = risk_validation_report_schema_version;  ///< Report schema version.

    /// Returns a detached native dict representation of the report fields.
    ///
    /// @return Dictionary with one field per native report field.
    /// @throws fincore_error If forecast events, refits, or a present backtest
    ///     contain non-finite values.
    [[nodiscard]] risk_validation_report_dict to_dict() const;

    /// Serializes the report to deterministic JSON text.
    ///
    /// Keys are emitted in sorted order with two-space indentation,
    /// Python-compatible float text, and a trailing newline. Non-finite
    /// numbers and invalid UTF-8 are rejected.
    ///
    /// @return JSON text ending with a newline.
    /// @throws input_error If the specification is invalid, a numeric field is
    ///     not finite, or a text field is not valid UTF-8.
    std::string to_json() const;

    /// Writes the deterministic JSON artifact to ``path`` and returns it.
    ///
    /// Parent directories are not created, matching the source contract.
    ///
    /// @param path Destination path; the file is created or truncated.
    /// @return The same path after a successful write.
    /// @throws fincore_error If report fields violate the serialization
    ///     contract or the file cannot be opened or written.
    [[nodiscard]] std::filesystem::path write_json(
        const std::filesystem::path& path) const;
};

/// Builds a validation report from a walk-forward VaR result.
///
/// The result is revalidated before projection: the forecast, realized and
/// index lengths must agree, refit timestamps must follow the specification
/// cadence, fit parameters must reproduce their forecast segments, the digest
/// must be lowercase SHA-256 hex, and the backtest must match a recomputation
/// over the path. The report owns copies of all projected fields.
///
/// @param result Walk-forward result to project.
/// @param timestamp_index_name Optional index label copied into the report.
/// @return Validation report with specification echo, events, refits,
///     diagnostics, optional backtest and traffic-light zone.
/// @throws input_error If the result violates the validation contract.
risk_validation_report build_risk_validation_report(
    const walk_forward_var_result& result,
    std::optional<std::string> timestamp_index_name = std::nullopt);

}  // namespace fincore
