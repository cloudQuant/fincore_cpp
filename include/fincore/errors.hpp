#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/// Native structured error payload.
///
/// Python exception inheritance is not reproduced; the source class name is
/// preserved as `category`, with a stable code, message, flat fields and an
/// optional diagnostic report.
struct error_info {
  std::string category;  ///< Source exception class name, such as
                         ///< `ValidationError`.
  std::string code;      ///< Stable dotted error code, such as
                         ///< `fincore.validation`.
  std::string message;   ///< Human-readable error message.
  std::map<std::string, std::string> fields;  ///< Flat string fields such as
                                              ///< `param_name`.
  std::map<std::string, std::string> report;  ///< Diagnostic report entries;
                                              ///< flattened with a `report.`
                                              ///< prefix by to_dict().

  /// Returns the flat dictionary form of this error.
  ///
  /// The result starts from `fields` and adds `error_type`, `message`, `code`
  /// when non-empty, and one `report.`-prefixed key per report entry.
  ///
  /// @return Flat string map suitable for error_info::from_dict().
  std::map<std::string, std::string> to_dict() const;

  /// Returns a compact JSON object with `category`, `code`, `message`,
  /// `fields` and `report`.
  ///
  /// String values are JSON-escaped.
  ///
  /// @return JSON text.
  std::string to_json() const;

  /// Builds an error payload from the flat dictionary form.
  ///
  /// `error_type` is required and becomes `category`. A non-empty `code` entry
  /// overrides the default code formed by prefixing the category with
  /// `fincore.`; a `message` entry becomes the message; `report.`-prefixed
  /// entries become report entries and all other entries become flat fields.
  ///
  /// @param data Flat string map, typically produced by to_dict().
  /// @return Parsed error payload.
  /// @throws std::invalid_argument If `error_type` is missing or empty.
  static error_info from_dict(const std::map<std::string, std::string>& data);
};

/// Exception type carrying a structured error_info payload.
///
/// The `std::runtime_error` message is the error_info message, so what()
/// returns the same text.
class fincore_error : public std::runtime_error {
 public:
  /// Creates an exception from a structured payload.
  ///
  /// @param info Error payload to store.
  explicit fincore_error(error_info info);

  /// Creates an exception with the default `FincoreError` category and
  /// `fincore.error` code.
  ///
  /// @param message Human-readable error message.
  explicit fincore_error(std::string message);

  /// Returns the structured error payload.
  const error_info& info() const noexcept { return info_; }
  /// Returns the source exception class name.
  const std::string& category() const noexcept { return info_.category; }
  /// Returns the stable dotted error code.
  const std::string& code() const noexcept { return info_.code; }
  /// Returns the flat error fields.
  const std::map<std::string, std::string>& fields() const noexcept {
    return info_.fields;
  }
  /// Returns the diagnostic report entries.
  const std::map<std::string, std::string>& report() const noexcept {
    return info_.report;
  }

  /// Creates an exception from the flat dictionary form.
  ///
  /// @param data Flat string map accepted by error_info::from_dict().
  /// @return Exception carrying the parsed payload.
  /// @throws std::invalid_argument If `error_type` is missing or empty.
  static fincore_error from_dict(
      const std::map<std::string, std::string>& data);

 private:
  error_info info_;
};

/// Returns a structured error payload built from the supplied components.
///
/// No validation or normalization is applied; fields and report are stored as
/// given.
///
/// @param category Source exception class name.
/// @param code Stable dotted error code.
/// @param message Human-readable error message.
/// @param fields Flat string fields.
/// @param report Diagnostic report entries.
/// @return Error payload with the supplied values.
error_info make_error(std::string category, std::string code,
                      std::string message,
                      std::map<std::string, std::string> fields = {},
                      std::map<std::string, std::string> report = {});

/// Returns a `ValidationError` payload with code `fincore.validation`.
///
/// `param_name` and `value` are stored in `fields` only when non-empty.
///
/// @param message Human-readable error message.
/// @param param_name Offending parameter name, if known.
/// @param value Rejected value rendered as text, if known.
/// @return Validation error payload.
error_info validation_error(std::string message,
                            std::string param_name = {},
                            std::string value = {});

/// Returns an `InsufficientDataError` payload with code
/// `fincore.insufficient_data`.
///
/// Optional lengths are stored as decimal strings under `required_length` and
/// `actual_length`.
///
/// @param message Human-readable error message.
/// @param required_length Minimum required observation count, if known.
/// @param actual_length Observed count, if known.
/// @return Insufficient-data error payload.
error_info insufficient_data_error(std::string message,
                                   std::optional<std::int64_t> required_length =
                                       std::nullopt,
                                   std::optional<std::int64_t> actual_length =
                                       std::nullopt);

/// Returns an `InvalidPeriodError` payload for `period`.
///
/// The payload uses code `fincore.invalid_period`, lists the accepted periods
/// (`daily`, `weekly`, `monthly`, `quarterly`, `yearly`) and stores `period`
/// and `valid_periods` in `fields`.
///
/// @param period Rejected period label.
/// @return Invalid-period error payload.
error_info invalid_period_error(std::string period);

/// Returns a `DataAlignmentError` payload with code
/// `fincore.data_alignment`.
///
/// Optional lengths are stored as decimal strings under `returns_length` and
/// `factor_length`.
///
/// @param message Human-readable error message.
/// @param returns_length Number of return observations, if known.
/// @param factor_length Number of factor observations, if known.
/// @return Data-alignment error payload.
error_info data_alignment_error(
    std::string message,
    std::optional<std::int64_t> returns_length = std::nullopt,
    std::optional<std::int64_t> factor_length = std::nullopt);

/// Returns a `NumericalError` payload with code `fincore.numerical`.
///
/// `operation` is stored in `fields` only when non-empty.
///
/// @param message Human-readable error message.
/// @param operation Numeric operation label, if known.
/// @return Numerical error payload.
error_info numerical_error(std::string message, std::string operation = {});

/// Returns a `MissingDataError` payload with code `fincore.missing_data`.
///
/// `missing_field` is stored in `fields` only when non-empty.
///
/// @param message Human-readable error message.
/// @param missing_field Name of the missing field, if known.
/// @return Missing-data error payload.
error_info missing_data_error(std::string message,
                              std::string missing_field = {});

/// Returns an `UnsupportedFormatError` payload with code
/// `fincore.unsupported_format`.
///
/// `format` is stored in `fields` only when non-empty.
///
/// @param message Human-readable error message.
/// @param format Rejected format label, if known.
/// @return Unsupported-format error payload.
error_info unsupported_format_error(std::string message,
                                    std::string format = {});

/// Returns a `DependencyError` payload with code `fincore.dependency`.
///
/// `dependency` and `extra` are stored in `fields` only when non-empty.
///
/// @param message Human-readable error message.
/// @param dependency Missing dependency name, if known.
/// @param extra Extra or feature label associated with the dependency.
/// @return Dependency error payload.
error_info dependency_error(std::string message, std::string dependency = {},
                            std::string extra = {});

/// Returns a `RuntimeDiagnosticError` payload with code
/// `fincore.runtime.diagnostic`.
///
/// `runtime_category` is always stored in `fields`; `operation_id`,
/// `capability_id` and `remediation` are stored only when non-empty. `details`
/// becomes the diagnostic report.
///
/// @param message Human-readable error message.
/// @param runtime_category Runtime classification, such as
///     `resolution_failure`.
/// @param operation_id Operation identifier, if known.
/// @param capability_id Capability identifier, if known.
/// @param details Diagnostic report entries.
/// @param remediation Suggested remediation text, if known.
/// @return Runtime diagnostic payload.
error_info runtime_diagnostic_error(
    std::string message, std::string runtime_category,
    std::string operation_id = {}, std::string capability_id = {},
    std::map<std::string, std::string> details = {},
    std::string remediation = {});

/// Returns a runtime diagnostic error for an unknown operation identifier.
///
/// The payload uses category `RuntimeDiagnosticError`, code
/// `fincore.runtime.operation_resolution`, runtime category
/// `resolution_failure`, an `unknown operation_id:` message followed by the
/// identifier, and a remediation that points at the operation catalog.
///
/// @param operation_id Unknown operation identifier.
/// @return Runtime diagnostic payload for the resolution failure.
error_info make_operation_resolution_error(std::string operation_id);

/// Returns an error payload with operation context and code
/// `fincore.contract`.
///
/// `category` is used verbatim. Optional context values are stored in `fields`
/// under `operation_id`, `parameter`, `path` and `profile` only when non-empty.
///
/// @param category Source exception class name.
/// @param message Human-readable error message.
/// @param operation_id Operation identifier, if known.
/// @param parameter Parameter name, if known.
/// @param path Artifact path, if known.
/// @param profile Profile name, if known.
/// @return Contextual error payload.
error_info contextual_error(std::string category, std::string message,
                            std::string operation_id = {},
                            std::string parameter = {}, std::string path = {},
                            std::string profile = {});

/// Returns an `InputContractError` payload via contextual_error().
///
/// @param message Human-readable error message.
/// @param operation_id Operation identifier, if known.
/// @param parameter Parameter name, if known.
/// @param path Artifact path, if known.
/// @param profile Profile name, if known.
/// @return Input-contract error payload with code `fincore.contract`.
error_info input_contract_error(std::string message, std::string operation_id = {},
                                std::string parameter = {}, std::string path = {},
                                std::string profile = {});

/// Returns an `AlignmentError` payload via contextual_error().
///
/// @param message Human-readable error message.
/// @param operation_id Operation identifier, if known.
/// @param parameter Parameter name, if known.
/// @param path Artifact path, if known.
/// @param profile Profile name, if known.
/// @return Alignment error payload with code `fincore.contract`.
error_info alignment_error(std::string message, std::string operation_id = {},
                           std::string parameter = {}, std::string path = {},
                           std::string profile = {});

/// Returns a `NumericalConvergenceError` payload via contextual_error().
///
/// @param message Human-readable error message.
/// @param operation_id Operation identifier, if known.
/// @param parameter Parameter name, if known.
/// @param path Artifact path, if known.
/// @param profile Profile name, if known.
/// @return Numerical-convergence error payload with code `fincore.contract`.
error_info numerical_convergence_error(std::string message,
                                       std::string operation_id = {},
                                       std::string parameter = {},
                                       std::string path = {},
                                       std::string profile = {});

/// Returns a `ResultContractError` payload via contextual_error().
///
/// @param message Human-readable error message.
/// @param operation_id Operation identifier, if known.
/// @param parameter Parameter name, if known.
/// @param path Artifact path, if known.
/// @param profile Profile name, if known.
/// @return Result-contract error payload with code `fincore.contract`.
error_info result_contract_error(std::string message,
                                 std::string operation_id = {},
                                 std::string parameter = {},
                                 std::string path = {},
                                 std::string profile = {});

/// Returns a `ResourceLifecycleError` payload via contextual_error().
///
/// @param message Human-readable error message.
/// @param operation_id Operation identifier, if known.
/// @param parameter Parameter name, if known.
/// @param path Artifact path, if known.
/// @param profile Profile name, if known.
/// @return Resource-lifecycle error payload with code `fincore.contract`.
error_info resource_lifecycle_error(std::string message,
                                    std::string operation_id = {},
                                    std::string parameter = {},
                                    std::string path = {},
                                    std::string profile = {});

/// Availability record for one dependency.
struct dependency_status {
  std::string name;         ///< Dependency name used in error messages.
  bool available = false;   ///< Whether the dependency is available.
  std::string extra;        ///< Extra or feature label for the dependency.
};

/// Throws when any listed dependency is unavailable.
///
/// The error is a `DependencyError` whose `dependency` field lists every
/// unavailable name in input order, joined with `", "`. The `extra` field is
/// the first non-empty extra label among the unavailable dependencies and is
/// omitted when none supplies one. Its message is
/// `Missing required dependencies:` followed by the names.
///
/// @param dependencies Dependency records to check.
/// @throws fincore_error With category `DependencyError` when at least one
///     entry is unavailable.
void check_dependencies(std::span<const dependency_status> dependencies);

/// Returns `value`, or `replace_with` when `value` is NaN.
///
/// @param value Value to check.
/// @param param_name Parameter name used in the error message and field.
/// @param replace_with Replacement used instead of throwing, if supplied.
/// @return `value` when it is not NaN, otherwise the supplied replacement.
/// @throws fincore_error When `value` is NaN and `replace_with` is absent; the
///     payload is a `ValidationError` for `param_name`.
double ensure_not_nan(double value, std::string_view param_name,
                      std::optional<double> replace_with = std::nullopt);

/// Returns `numerator / denominator`, or `default_value` when the denominator
/// is zero.
///
/// @param numerator Dividend.
/// @param denominator Divisor; zero selects `default_value`.
/// @param default_value Value returned for a zero denominator; defaults to NaN.
/// @return Quotient, or `default_value` for a zero denominator.
double safe_divide(double numerator, double denominator,
                   double default_value = std::numeric_limits<double>::quiet_NaN());

/// Returns the square root of `value`, or `default_value` when `value` is
/// negative.
///
/// @param value Input value.
/// @param default_value Value returned for a negative input; defaults to NaN.
/// @return Square root, or `default_value` when `value < 0`.
double safe_sqrt(double value,
                 double default_value = std::numeric_limits<double>::quiet_NaN());

/// Runs a numeric kernel and converts native numeric exceptions to
/// `NumericalError`.
///
/// Existing fincore_error exceptions pass through unchanged.
/// `std::domain_error`, `std::overflow_error`, `std::underflow_error`,
/// `std::invalid_argument` and other `std::runtime_error` exceptions are
/// rethrown as fincore_error payloads from numerical_error() with the supplied
/// operation label. The kernel result is returned unchanged.
///
/// @tparam Function Callable type invoked with no arguments.
/// @param operation Numeric operation label stored in the error payload.
/// @param function Kernel to invoke.
/// @return Whatever `function()` returns.
/// @throws fincore_error For the converted native exceptions.
template <typename Function>
decltype(auto) handle_numerical_error(std::string_view operation,
                                      Function&& function) {
  try {
    return std::forward<Function>(function)();
  } catch (const fincore_error&) {
    throw;
  } catch (const std::domain_error& error) {
    throw fincore_error{
        numerical_error(error.what(), std::string(operation))};
  } catch (const std::overflow_error& error) {
    throw fincore_error{
        numerical_error(error.what(), std::string(operation))};
  } catch (const std::underflow_error& error) {
    throw fincore_error{
        numerical_error(error.what(), std::string(operation))};
  } catch (const std::invalid_argument& error) {
    throw fincore_error{
        numerical_error(error.what(), std::string(operation))};
  } catch (const std::runtime_error& error) {
    throw fincore_error{
        numerical_error(error.what(), std::string(operation))};
  }
}

}  // namespace fincore
