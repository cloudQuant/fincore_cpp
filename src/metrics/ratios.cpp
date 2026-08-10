#include <fincore/metrics/ratios.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::vector<double> valid_values(std::span<const double> values) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            result.push_back(value);
        }
    }
    return result;
}

double mean(const std::vector<double>& values) {
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

double annual_return_from_valid(const std::vector<double>& values,
                                double annualization) {
    double wealth = 1.0;
    for (const double value : values) {
        wealth *= 1.0 + value;
    }
    if (wealth <= 0.0) {
        return -1.0;
    }
    return std::pow(wealth, annualization / static_cast<double>(values.size())) - 1.0;
}

double finite_quotient(double numerator, double denominator) {
    if (denominator == 0.0) {
        return nan;
    }
    const double result = numerator / denominator;
    return std::isfinite(result) ? result : nan;
}

}  // namespace

double sharpe_ratio(const dataframe& returns,
                    double risk_free,
                    double annualization,
                    std::string_view column) {
    const auto values = valid_values(require_double_column(returns, column));
    if (values.size() < 2) {
        return nan;
    }

    std::vector<double> excess;
    excess.reserve(values.size());
    for (const double value : values) {
        excess.push_back(value - risk_free);
    }
    const double average = mean(excess);
    double squared_deviations = 0.0;
    for (const double value : excess) {
        const double deviation = value - average;
        squared_deviations += deviation * deviation;
    }
    const double sample_standard_deviation =
        std::sqrt(squared_deviations / static_cast<double>(excess.size() - 1));
    return finite_quotient(average * std::sqrt(annualization),
                           sample_standard_deviation);
}

double sortino_ratio(const dataframe& returns,
                     double required_return,
                     double annualization,
                     std::string_view column) {
    const auto values = valid_values(require_double_column(returns, column));
    if (values.size() < 2) {
        return nan;
    }

    std::vector<double> excess;
    excess.reserve(values.size());
    double squared_downside = 0.0;
    for (const double value : values) {
        const double excess_value = value - required_return;
        excess.push_back(excess_value);
        if (excess_value < 0.0) {
            squared_downside += excess_value * excess_value;
        }
    }
    const double annualized_mean = mean(excess) * annualization;
    const double annualized_downside = std::sqrt(
        squared_downside / static_cast<double>(excess.size())) * std::sqrt(annualization);
    return finite_quotient(annualized_mean, annualized_downside);
}

double calmar_ratio(const dataframe& returns,
                    double risk_free,
                    double annualization,
                    std::string_view column) {
    const auto values = valid_values(require_double_column(returns, column));
    if (values.size() < 2) {
        return nan;
    }

    double wealth = 100.0;
    double peak = wealth;
    double maximum_drawdown = 0.0;
    for (const double value : values) {
        wealth *= 1.0 + value;
        peak = std::fmax(peak, wealth);
        maximum_drawdown = std::fmin(maximum_drawdown, (wealth - peak) / peak);
    }
    if (!(maximum_drawdown < 0.0)) {
        return nan;
    }
    return finite_quotient(annual_return_from_valid(values, annualization) - risk_free,
                           std::fabs(maximum_drawdown));
}

}  // namespace fincore
