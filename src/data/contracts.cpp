#include <fincore/data/contracts.hpp>

#include <fincore/errors.hpp>

namespace fincore {

// Validation runs here and on an explicit validate() call only: the four fields
// are public and default-initialized, so a policy adjusted in place can hold a
// non-positive timeout or a zero attempt count until somebody calls validate().
request_policy::request_policy(double connect_timeout_value,
                               double read_timeout_value,
                               double total_timeout_value, int max_attempts_value)
    : connect_timeout(connect_timeout_value),
      read_timeout(read_timeout_value),
      total_timeout(total_timeout_value),
      max_attempts(max_attempts_value) {
  validate();
}

void request_policy::validate() const {
  if (connect_timeout <= 0.0 || read_timeout <= 0.0 || total_timeout <= 0.0) {
    throw fincore_error{validation_error("timeouts must be positive")};
  }
  if (max_attempts < 1) {
    throw fincore_error{
        validation_error("max_attempts must be at least 1")};
  }
}

bool request_policy::should_retry(transport_error_kind kind) const noexcept {
  return kind == transport_error_kind::connection ||
         kind == transport_error_kind::timeout ||
         kind == transport_error_kind::operating_system;
}

// Clamped at zero, so a caller that has already overrun its budget sees "none
// left" rather than a negative count it would have to interpret.
int request_policy::remaining_attempts(int attempt) const noexcept {
  const int remaining = max_attempts - attempt;
  return remaining > 0 ? remaining : 0;
}

}  // namespace fincore
