#pragma once

#include <cstdint>

namespace fincore {

/// Classification of a transport failure for retry decisions.
enum class transport_error_kind {
  none,              ///< No transport error.
  connection,        ///< Connection-level failure; retryable.
  timeout,           ///< Request timeout; retryable.
  operating_system,  ///< Operating-system transport failure; retryable.
  validation,        ///< Request validation failure; not retried.
  other,             ///< Any other failure; not retried.
};

/// Bounded, deterministic retry policy for external data access.
///
/// Mirrors the pinned `fincore.data.contracts.RequestPolicy` predicates without
/// importing a network stack.
struct request_policy {
  double connect_timeout = 10.0;  ///< Connect timeout; defaults to 10.0.
  double read_timeout = 30.0;     ///< Read timeout; defaults to 30.0.
  double total_timeout = 60.0;    ///< Total timeout; defaults to 60.0.
  int max_attempts = 3;           ///< Maximum attempt count; defaults to 3.

  /// Creates a policy with the default values.
  request_policy() = default;

  /// Creates a policy and validates its values.
  ///
  /// @param connect_timeout Connect timeout value.
  /// @param read_timeout Read timeout value.
  /// @param total_timeout Total timeout value.
  /// @param max_attempts Maximum attempt count.
  /// @throws fincore_error With category `ValidationError` when a timeout is
  ///     not positive or `max_attempts` is less than 1.
  request_policy(double connect_timeout, double read_timeout,
                 double total_timeout, int max_attempts);

  /// Validates the timeout and attempt values.
  ///
  /// @throws fincore_error With category `ValidationError` when a timeout is
  ///     not positive or `max_attempts` is less than 1.
  void validate() const;

  /// Returns whether failures of `kind` are retried.
  ///
  /// Connection, timeout and operating-system failures are retryable;
  /// validation, `none` and `other` are not.
  ///
  /// @param kind Transport failure classification.
  /// @return True for retryable kinds.
  bool should_retry(transport_error_kind kind) const noexcept;

  /// Returns the attempts left after `attempt`.
  ///
  /// The result is `max_attempts - attempt`, clamped to zero.
  ///
  /// @param attempt One-based attempt number.
  /// @return Remaining attempts, never negative.
  int remaining_attempts(int attempt) const noexcept;
};

}  // namespace fincore
