#include <fincore/simulation/parameters.hpp>

#include <fincore/error.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace fincore {
namespace {

struct running_moments {
    std::size_t count{0U};
    double mean{0.0};
    double sum_of_squared_deviations{0.0};
};

std::string checked_column_name(const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"parameter estimation column name must not contain an embedded NUL"};
    }
    return std::string{column};
}

void require_finite(const double value, const char* const message) {
    if (!std::isfinite(value)) {
        throw input_error{message};
    }
}

void add_observation(running_moments& moments, const double value) {
    if (moments.count == std::numeric_limits<std::size_t>::max()) {
        throw input_error{"too many returns for parameter estimation"};
    }

    const std::size_t next_count = moments.count + 1U;
    const double count_as_double = static_cast<double>(next_count);
    require_finite(count_as_double,
                   "parameter estimation observation count is not finite");

    const double delta = value - moments.mean;
    require_finite(delta, "parameter estimation mean calculation overflowed");

    const double next_mean = moments.mean + delta / count_as_double;
    require_finite(next_mean,
                   "parameter estimation mean calculation overflowed");

    const double next_delta = value - next_mean;
    require_finite(next_delta,
                   "parameter estimation variance calculation overflowed");
    const double square_increment = delta * next_delta;
    require_finite(square_increment,
                   "parameter estimation variance calculation overflowed");

    const double next_sum_of_squared_deviations =
        moments.sum_of_squared_deviations + square_increment;
    require_finite(next_sum_of_squared_deviations,
                   "parameter estimation variance calculation overflowed");

    moments = {next_count, next_mean, next_sum_of_squared_deviations};
}

running_moments finite_moments(const std::span<const double> values) {
    running_moments moments;
    for (const double value : values) {
        if (std::isnan(value)) {
            continue;
        }
        if (!std::isfinite(value)) {
            throw input_error{"parameter estimation returns must be finite or NaN"};
        }
        add_observation(moments, value);
    }
    if (moments.count < 2U) {
        throw input_error{"parameter estimation requires at least two returns"};
    }
    return moments;
}

}  // namespace

simulation_parameters estimate_simulation_parameters(
    const dataframe& returns,
    const std::size_t periods_per_year,
    const std::string_view column) {
    if (periods_per_year == 0U) {
        throw input_error{"periods per year must be positive"};
    }

    const auto column_name = checked_column_name(column);
    const auto moments =
        finite_moments(require_double_column(returns, column_name));
    const double periods_as_double = static_cast<double>(periods_per_year);
    require_finite(periods_as_double,
                   "parameter estimation periods per year is not finite");

    const double denominator = static_cast<double>(moments.count - 1U);
    require_finite(denominator,
                   "parameter estimation sample denominator is not finite");
    const double sample_variance = moments.sum_of_squared_deviations / denominator;
    if (!std::isfinite(sample_variance) || sample_variance < 0.0) {
        throw input_error{"parameter estimation variance is not finite"};
    }

    const double sample_deviation = std::sqrt(sample_variance);
    require_finite(sample_deviation,
                   "parameter estimation sample deviation is not finite");
    const double annualization_scale = std::sqrt(periods_as_double);
    require_finite(annualization_scale,
                   "parameter estimation annualization scale is not finite");

    const double drift = moments.mean * periods_as_double;
    const double volatility = sample_deviation * annualization_scale;
    require_finite(drift, "parameter estimation drift is not finite");
    require_finite(volatility, "parameter estimation volatility is not finite");
    return {drift, volatility};
}

}  // namespace fincore
