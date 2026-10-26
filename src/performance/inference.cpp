#include <fincore/performance/inference.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::vector<double> clean_values(const dataframe& returns,
                                 const std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> clean;
    clean.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            clean.push_back(value);
        }
    }
    return clean;
}

double sample_std(const std::vector<double>& values) {
    double mean = 0.0;
    for (const double value : values) {
        mean += value;
    }
    mean /= static_cast<double>(values.size());
    double total = 0.0;
    for (const double value : values) {
        const double deviation = value - mean;
        total += deviation * deviation;
    }
    return std::sqrt(total / static_cast<double>(values.size() - 1));
}

// A flat series is reported as a zero Sharpe rather than an undefined one, which
// is what lets the standard-error formula below read it as no risk adjustment; the
// estimator itself is the small-sample form with (1 + S^2/2) over the observation
// count as its only divisor.
double sharpe_ratio_of(const std::vector<double>& clean) {
    double mean = 0.0;
    for (const double value : clean) {
        mean += value;
    }
    mean /= static_cast<double>(clean.size());
    const double deviation = sample_std(clean);
    return deviation > 1e-15 ? mean / deviation : 0.0;
}

}  // namespace

double standard_error_of_mean(const dataframe& values,
                              std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto clean = clean_values(values, column);
    if (clean.size() < 2) {
        return nan;
    }
    return sample_std(clean) /
           std::sqrt(static_cast<double>(clean.size()));
}

double sharpe_standard_error(const dataframe& returns,
                             double risk_free,
                             std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    auto clean = clean_values(returns, column);
    for (double& value : clean) {
        value -= risk_free;
    }
    if (clean.size() < 2) {
        return nan;
    }
    const double sharpe = sharpe_ratio_of(clean);
    return std::sqrt(
        (1.0 + 0.5 * sharpe * sharpe) / static_cast<double>(clean.size()));
}

std::pair<double, double> sharpe_confidence_interval(
    const dataframe& returns,
    double risk_free,
    double z,
    std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    auto clean = clean_values(returns, column);
    if (clean.size() < 2) {
        return {nan, nan};
    }
    for (double& value : clean) {
        value -= risk_free;
    }
    const double sharpe = sharpe_ratio_of(clean);
    const double standard_error = std::sqrt(
        (1.0 + 0.5 * sharpe * sharpe) / static_cast<double>(clean.size()));
    return {sharpe - z * standard_error, sharpe + z * standard_error};
}

}  // namespace fincore
