#include <fincore/performance/returns.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <span>
#include <vector>

namespace fincore {

double twr(const dataframe& returns, std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    double product = 1.0;
    for (const double value : values) {
        if (!std::isnan(value)) {
            product *= 1.0 + value;
        }
    }
    return product - 1.0;
}

namespace {

constexpr long long nanoseconds_per_day = 86'400'000'000'000LL;

// Floor division, not truncation: timestamps before the epoch must land in
// their own UTC day, and C++ integer division truncates toward zero, which
// would advance any pre-epoch timestamp carrying a time-of-day onto the
// following day.
long long utc_day(const timestamp value) {
    long long quotient = value / nanoseconds_per_day;
    if (value % nanoseconds_per_day != 0 && value < 0) {
        --quotient;
    }
    return quotient;
}

// The bracket is expressed in log-gross space, so these are the two ends of
// the rate range that still survives a logarithm: the largest representable
// rate strictly above -1, and the largest finite rate. The singular rate of
// exactly -1 is deliberately outside the interval rather than a boundary the
// solver may evaluate.
double log_gross_lower_bound() {
    return std::log1p(std::nextafter(-1.0, 0.0));
}

double log_gross_upper_bound() {
    return std::log(std::numeric_limits<double>::max());
}

// Each term is discounted in log space and re-exponentiated relative to the
// largest magnitude in the series, so no single present value can overflow.
// Both outputs carry that shared factor, which means only their signs and
// their ratio are meaningful here, never an absolute net present value.
bool scaled_npv(const std::vector<double>& cashflows,
                const std::vector<double>& periods,
                const double log_gross_rate,
                double& value,
                double& scale) {
    double anchor = -std::numeric_limits<double>::infinity();
    std::vector<double> log_magnitudes(cashflows.size());
    for (std::size_t position = 0; position < cashflows.size(); ++position) {
        const double magnitude =
            std::log(std::fabs(cashflows[position])) -
            periods[position] * log_gross_rate;
        if (!std::isfinite(magnitude)) {
            return false;
        }
        log_magnitudes[position] = magnitude;
        anchor = std::max(anchor, magnitude);
    }
    double total = 0.0;
    double absolute_total = 0.0;
    for (std::size_t position = 0; position < cashflows.size(); ++position) {
        const double term = std::exp(log_magnitudes[position] - anchor);
        total += std::copysign(term, cashflows[position]);
        absolute_total += term;
    }
    value = total;
    scale = absolute_total;
    return std::isfinite(total) && std::isfinite(absolute_total) &&
           absolute_total > 0.0;
}

// Exactly one transition, read from the sign bit: [-, +, -] has two and is
// rejected as multi-root rather than resolved to one of its roots, and since a
// plain zero reads as positive, [-, 0, -] also counts two instead of masking an
// unknown sign.
bool has_single_sign_change(const std::vector<double>& cashflows) {
    if (cashflows.size() < 2) {
        return false;
    }
    std::size_t transitions = 0;
    for (std::size_t position = 1; position < cashflows.size(); ++position) {
        if (std::signbit(cashflows[position]) !=
            std::signbit(cashflows[position - 1])) {
            ++transitions;
        }
    }
    return transitions == 1;
}

double rate_from_log_gross(const std::vector<double>& cashflows,
                           const std::vector<double>& periods,
                           const double log_gross_rate) {
    const double rate = std::expm1(log_gross_rate);
    if (!std::isfinite(rate) || !(rate > -1.0)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Check the rate that callers actually receive. Rounding log-gross space
    // back to a binary64 rate can make roots close to -1 unrepresentable even
    // when the internal bracket is narrow.
    const double rounded_log_gross = std::log1p(rate);
    double residual = 0.0;
    double scale = 0.0;
    constexpr double residual_budget = 1e-10;
    if (!std::isfinite(rounded_log_gross) ||
        !scaled_npv(cashflows, periods, rounded_log_gross, residual, scale) ||
        std::fabs(residual) / scale > residual_budget) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return rate;
}

double solve_conventional_irr(const std::vector<double>& cashflows,
                              const std::vector<double>& periods) {
    double lower_value = 0.0;
    double upper_value = 0.0;
    double lower_scale = 0.0;
    double upper_scale = 0.0;
    const double lower_bound = log_gross_lower_bound();
    const double upper_bound = log_gross_upper_bound();
    if (!scaled_npv(cashflows, periods, lower_bound, lower_value,
                    lower_scale) ||
        !scaled_npv(cashflows, periods, upper_bound, upper_value,
                    upper_scale)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (lower_value == 0.0) {
        return rate_from_log_gross(cashflows, periods, lower_bound);
    }
    if (upper_value == 0.0) {
        return rate_from_log_gross(cashflows, periods, upper_bound);
    }
    if (std::signbit(lower_value) == std::signbit(upper_value)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double lower = lower_bound;
    double upper = upper_bound;
    // The bracket contracts in log-gross space, the coordinate in which the
    // discount exponent is linear, and the stop test is relative to the
    // midpoint so a root far from zero still gets the same effective digits.
    for (int iteration = 0; iteration < 200; ++iteration) {
        const double middle = lower + (upper - lower) / 2.0;
        double middle_value = 0.0;
        double middle_scale = 0.0;
        if (!scaled_npv(cashflows, periods, middle, middle_value,
                        middle_scale)) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        if (std::signbit(middle_value) == std::signbit(lower_value)) {
            lower = middle;
            lower_value = middle_value;
        } else {
            upper = middle;
            upper_value = middle_value;
        }
        if (upper - lower <= 1e-15 * std::max(1.0, std::fabs(middle))) {
            break;
        }
    }
    const double root = lower + (upper - lower) / 2.0;
    return rate_from_log_gross(cashflows, periods, root);
}

double irr_from_flows(const std::vector<double>& cashflows,
                      const std::vector<double>& periods) {
    if (cashflows.size() < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (!has_single_sign_change(cashflows)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return solve_conventional_irr(cashflows, periods);
}

}  // namespace

double xirr(const dataframe& cashflows, std::string_view amount_column) {
    if (amount_column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto amounts = require_double_column(cashflows, amount_column);
    std::map<long long, double> daily_totals;
    for (std::size_t position = 0; position < amounts.size(); ++position) {
        if (!std::isfinite(amounts[position])) {
            throw input_error{"cashflows must be finite"};
        }
        daily_totals[utc_day(cashflows.get_index()[position])] +=
            amounts[position];
    }
    if (daily_totals.size() < 2) {
        throw input_error{"at least two distinct cashflow dates are required"};
    }

    std::vector<double> aggregated;
    std::vector<double> periods;
    const long long first_day = daily_totals.begin()->first;
    // Days netting to zero are dropped rather than carried along, since
    // whether a zero registers a sign transition depends on its neighbours and
    // keeping one could make an otherwise conventional series look
    // multi-rooted. Elapsed time is then counted in 365-day years from the
    // first surviving day.
    for (const auto& entry : daily_totals) {
        if (entry.second != 0.0) {
            aggregated.push_back(entry.second);
            periods.push_back(
                static_cast<double>(entry.first - first_day) / 365.0);
        }
    }
    return irr_from_flows(aggregated, periods);
}

double mwr(const dataframe& cashflows,
           std::optional<double> period,
           std::string_view amount_column) {
    if (amount_column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto amounts = require_double_column(cashflows, amount_column);
    if (amounts.size() < 2) {
        throw input_error{"at least two cashflows are required"};
    }
    for (const double amount : amounts) {
        if (!std::isfinite(amount)) {
            throw input_error{"cashflows must be finite"};
        }
    }

    double interval = 1.0;
    if (period.has_value()) {
        interval = *period;
        if (!std::isfinite(interval) || interval <= 0.0) {
            throw input_error{"periods must be a positive finite interval"};
        }
    }

    std::vector<double> flows;
    std::vector<double> periods;
    flows.reserve(amounts.size());
    periods.reserve(amounts.size());
    for (std::size_t position = 0; position < amounts.size(); ++position) {
        if (amounts[position] == 0.0) {
            continue;
        }
        flows.push_back(amounts[position]);
        periods.push_back(static_cast<double>(position) * interval);
    }
    return irr_from_flows(flows, periods);
}

}  // namespace fincore
