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

// Native structured error contract. Python exception inheritance is not
// reproduced; the source class name is preserved as `category`, with a stable
// code, message, flat fields and an optional diagnostic report.
struct error_info {
  std::string category;
  std::string code;
  std::string message;
  std::map<std::string, std::string> fields;
  std::map<std::string, std::string> report;

  std::map<std::string, std::string> to_dict() const;
  std::string to_json() const;
  static error_info from_dict(const std::map<std::string, std::string>& data);
};

class fincore_error : public std::runtime_error {
 public:
  explicit fincore_error(error_info info);
  explicit fincore_error(std::string message);

  const error_info& info() const noexcept { return info_; }
  const std::string& category() const noexcept { return info_.category; }
  const std::string& code() const noexcept { return info_.code; }
  const std::map<std::string, std::string>& fields() const noexcept {
    return info_.fields;
  }
  const std::map<std::string, std::string>& report() const noexcept {
    return info_.report;
  }

  static fincore_error from_dict(
      const std::map<std::string, std::string>& data);

 private:
  error_info info_;
};

error_info make_error(std::string category, std::string code,
                      std::string message,
                      std::map<std::string, std::string> fields = {},
                      std::map<std::string, std::string> report = {});

error_info validation_error(std::string message,
                            std::string param_name = {},
                            std::string value = {});
error_info insufficient_data_error(std::string message,
                                   std::optional<std::int64_t> required_length =
                                       std::nullopt,
                                   std::optional<std::int64_t> actual_length =
                                       std::nullopt);
error_info invalid_period_error(std::string period);
error_info data_alignment_error(
    std::string message,
    std::optional<std::int64_t> returns_length = std::nullopt,
    std::optional<std::int64_t> factor_length = std::nullopt);
error_info numerical_error(std::string message, std::string operation = {});
error_info missing_data_error(std::string message,
                              std::string missing_field = {});
error_info unsupported_format_error(std::string message,
                                    std::string format = {});
error_info dependency_error(std::string message, std::string dependency = {},
                            std::string extra = {});
error_info runtime_diagnostic_error(
    std::string message, std::string runtime_category,
    std::string operation_id = {}, std::string capability_id = {},
    std::map<std::string, std::string> details = {},
    std::string remediation = {});
error_info make_operation_resolution_error(std::string operation_id);
error_info contextual_error(std::string category, std::string message,
                            std::string operation_id = {},
                            std::string parameter = {}, std::string path = {},
                            std::string profile = {});
error_info input_contract_error(std::string message, std::string operation_id = {},
                                std::string parameter = {}, std::string path = {},
                                std::string profile = {});
error_info alignment_error(std::string message, std::string operation_id = {},
                           std::string parameter = {}, std::string path = {},
                           std::string profile = {});
error_info numerical_convergence_error(std::string message,
                                       std::string operation_id = {},
                                       std::string parameter = {},
                                       std::string path = {},
                                       std::string profile = {});
error_info result_contract_error(std::string message,
                                 std::string operation_id = {},
                                 std::string parameter = {},
                                 std::string path = {},
                                 std::string profile = {});
error_info resource_lifecycle_error(std::string message,
                                    std::string operation_id = {},
                                    std::string parameter = {},
                                    std::string path = {},
                                    std::string profile = {});

struct dependency_status {
  std::string name;
  bool available = false;
  std::string extra;
};

void check_dependencies(std::span<const dependency_status> dependencies);

double ensure_not_nan(double value, std::string_view param_name,
                      std::optional<double> replace_with = std::nullopt);
double safe_divide(double numerator, double denominator,
                   double default_value = std::numeric_limits<double>::quiet_NaN());
double safe_sqrt(double value,
                 double default_value = std::numeric_limits<double>::quiet_NaN());

// Wrap a numeric kernel: native domain/overflow/invalid-argument failures are
// rethrown as NumericalError while structured fincore errors pass through.
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
