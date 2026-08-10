#include <fincore/metrics/rolling.hpp>

#include <fincore/error.hpp>
#include <fincore/metrics/ratios.hpp>

#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

void validate_window(const std::size_t window) {
    if (window == 0U) {
        throw input_error{"rolling window must be positive"};
    }
}

dataframe materialize_complete_window(const dataframe& returns,
                                      std::span<const double> values,
                                      const std::size_t start,
                                      const std::size_t window,
                                      const std::string_view column) {
    const auto& index = returns.get_index();
    return make_series(
        {index.begin() + static_cast<std::ptrdiff_t>(start),
         index.begin() + static_cast<std::ptrdiff_t>(start + window)},
        column,
        {values.begin() + static_cast<std::ptrdiff_t>(start),
         values.begin() + static_cast<std::ptrdiff_t>(start + window)});
}

template <typename Metric>
dataframe endpoint_output(const dataframe& returns,
                          const std::size_t window,
                          const std::string_view column,
                          Metric&& metric) {
    validate_window(window);
    const auto values = require_double_column(returns, column);
    if (values.size() < window) {
        return make_series({}, column, {});
    }

    const auto& index = returns.get_index();
    std::vector<timestamp> output_index;
    std::vector<double> output_values;
    output_index.reserve(values.size() - window + 1U);
    output_values.reserve(values.size() - window + 1U);
    for (std::size_t start = 0U; start + window <= values.size(); ++start) {
        const auto complete =
            materialize_complete_window(returns, values, start, window, column);
        output_index.push_back(index[start + window - 1U]);
        output_values.push_back(metric(complete));
    }
    return make_series(std::move(output_index), column, std::move(output_values));
}

bool contains_nan(const dataframe& returns, const std::string_view column) {
    for (const double value : require_double_column(returns, column)) {
        if (std::isnan(value)) {
            return true;
        }
    }
    return false;
}

double raw_window_max_drawdown(std::span<const double> values) {
    double wealth = 1.0;
    double running_maximum = 1.0;
    double result = nan;
    for (const double value : values) {
        wealth *= 1.0 + value;
        if (wealth > running_maximum) {
            running_maximum = wealth;
        }
        const double drawdown = (wealth - running_maximum) / running_maximum;
        if (!std::isnan(drawdown) &&
            (std::isnan(result) || drawdown < result)) {
            result = drawdown;
        }
    }
    return result;
}

template <typename Metric>
dataframe full_index_output(const dataframe& returns,
                            const std::size_t window,
                            const std::string_view column,
                            Metric&& metric) {
    validate_window(window);
    const auto values = require_double_column(returns, column);
    std::vector<double> output_values(values.size(), nan);
    if (values.size() < window) {
        return make_series(returns.get_index(), column, std::move(output_values));
    }

    for (std::size_t start = 0U; start + window <= values.size(); ++start) {
        const auto complete =
            materialize_complete_window(returns, values, start, window, column);
        output_values[start + window - 1U] = metric(complete);
    }
    return make_series(returns.get_index(), column, std::move(output_values));
}

}  // namespace

dataframe roll_sharpe_ratio(const dataframe& returns,
                            const std::size_t window,
                            const double risk_free,
                            const frequency period,
                            const std::optional<double> annualization,
                            const std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return endpoint_output(
        returns, window, column,
        [risk_free, factor, column](const dataframe& complete) {
            return sharpe_ratio(complete, risk_free, factor, column);
        });
}

dataframe roll_max_drawdown(const dataframe& returns,
                            const std::size_t window,
                            const std::string_view column) {
    return endpoint_output(
        returns, window, column, [column](const dataframe& complete) {
            return raw_window_max_drawdown(require_double_column(complete, column));
        });
}

dataframe rolling_volatility(const dataframe& returns,
                             const std::size_t window,
                             const frequency period,
                             const std::optional<double> annualization,
                             const std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return full_index_output(
        returns, window, column, [factor, column](const dataframe& complete) {
            return contains_nan(complete, column)
                       ? nan
                       : annual_volatility(complete, factor, column);
        });
}

dataframe rolling_sharpe(const dataframe& returns,
                         const std::size_t window,
                         const frequency period,
                         const std::optional<double> annualization,
                         const std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return full_index_output(
        returns, window, column, [factor, column](const dataframe& complete) {
            return contains_nan(complete, column)
                       ? nan
                       : sharpe_ratio(complete, 0.0, factor, column);
        });
}

}  // namespace fincore
