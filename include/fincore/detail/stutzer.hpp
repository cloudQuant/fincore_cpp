#pragma once

#include <cmath>
#include <vector>

namespace fincore::detail {

// Internal test surface for the exponential-tilt objective used by
// stutzer_index. Keep this implementation aligned with the native leaf.
[[nodiscard]] inline double stutzer_objective(
    double theta, const std::vector<double>& excess_returns) {
  double total = 0.0;
  for (const double value : excess_returns) {
    const double tilted = std::exp(theta * value);
    if (std::isinf(tilted) || std::isnan(tilted)) {
      return 0.0;
    }
    total += tilted;
  }
  return std::log(total / static_cast<double>(excess_returns.size()));
}

}  // namespace fincore::detail
