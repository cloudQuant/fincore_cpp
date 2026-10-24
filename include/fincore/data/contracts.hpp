#pragma once

#include <cstdint>

namespace fincore {

enum class transport_error_kind {
  none,
  connection,
  timeout,
  operating_system,
  validation,
  other,
};

// Bounded, deterministic retry policy for external data access. Mirrors the
// pinned `fincore.data.contracts.RequestPolicy` predicates without importing a
// network stack.
struct request_policy {
  double connect_timeout = 10.0;
  double read_timeout = 30.0;
  double total_timeout = 60.0;
  int max_attempts = 3;

  request_policy() = default;
  request_policy(double connect_timeout, double read_timeout,
                 double total_timeout, int max_attempts);

  void validate() const;
  bool should_retry(transport_error_kind kind) const noexcept;
  int remaining_attempts(int attempt) const noexcept;
};

}  // namespace fincore
