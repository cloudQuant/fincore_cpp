#include <fincore/metrics/risk.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

double percentile(std::vector<double> values, double cutoff) {
    std::sort(values.begin(), values.end());
    const double position = cutoff * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    return values[lower] + (position - static_cast<double>(lower)) *
                               (values[upper] - values[lower]);
}

bool valid_risk_input(std::span<const double> values, double cutoff) {
    return !values.empty() && std::isfinite(cutoff) && cutoff >= 0.0 && cutoff <= 1.0 &&
           std::all_of(values.begin(), values.end(),
                       [](double value) { return std::isfinite(value); });
}

}  // namespace

double historical_value_at_risk(const dataframe& returns,
                                double cutoff,
                                std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (!valid_risk_input(values, cutoff)) {
        return nan;
    }
    return percentile({values.begin(), values.end()}, cutoff);
}

double conditional_value_at_risk(const dataframe& returns,
                                 double cutoff,
                                 std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (!valid_risk_input(values, cutoff)) {
        return nan;
    }
    const double value_at_risk = percentile({values.begin(), values.end()}, cutoff);
    double total = 0.0;
    std::size_t count = 0;
    for (const double value : values) {
        if (value <= value_at_risk) {
            total += value;
            ++count;
        }
    }
    return total / static_cast<double>(count);
}

}  // namespace fincore
