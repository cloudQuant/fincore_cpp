#include <fincore/metrics/rolling.hpp>

#include <fincore/error.hpp>
#include <fincore/metrics/ratios.hpp>

#include <cmath>
#include <limits>
#include <span>
#include <string>
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

template <typename MetricFactory>
dataframe paired_endpoint_output(
    const dataframe& returns,
    const dataframe& benchmark,
    const std::size_t window,
    const std::string_view returns_column_name,
    const std::string_view benchmark_column_name,
    MetricFactory&& metric_factory) {
    validate_window(window);
    auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const auto returns_values =
        require_double_column(aligned_returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(aligned_benchmark, benchmark_column_name);
    const auto metric = metric_factory();
    if (returns_values.size() < window) {
        return make_series({}, returns_column_name, {});
    }

    const auto& index = aligned_returns.get_index();
    std::vector<timestamp> output_index;
    std::vector<double> output_values;
    output_index.reserve(returns_values.size() - window + 1U);
    output_values.reserve(returns_values.size() - window + 1U);
    for (std::size_t start = 0U; start + window <= returns_values.size(); ++start) {
        const auto returns_window = materialize_complete_window(
            aligned_returns, returns_values, start, window, returns_column_name);
        const auto benchmark_window = materialize_complete_window(
            aligned_benchmark, benchmark_values, start, window, benchmark_column_name);
        output_index.push_back(index[start + window - 1U]);
        output_values.push_back(metric(returns_window, benchmark_window));
    }
    return make_series(std::move(output_index), returns_column_name,
                       std::move(output_values));
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
    double result = 0.0;
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

dataframe two_column_output(std::vector<timestamp> index,
                            std::vector<double> alpha_values,
                            std::vector<double> beta_values) {
    dataframe result;
    result.load_index(std::move(index));
    result.load_column<double>("alpha", std::move(alpha_values),
                               hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>("beta", std::move(beta_values),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

void validate_factor_columns(
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names) {
    if (factor_column_names.empty()) {
        throw input_error{"factor column names must not be empty"};
    }
    for (std::size_t position = 0U; position < factor_column_names.size(); ++position) {
        const auto name = factor_column_names[position];
        if (name.empty()) {
            throw input_error{"factor column names must not be empty"};
        }
        for (std::size_t prior = 0U; prior < position; ++prior) {
            if (name == factor_column_names[prior]) {
                throw input_error{"factor column names must be unique"};
            }
        }
        static_cast<void>(require_double_column(factor_returns, name));
    }
}

struct fixed_window_regression {
    double alpha{nan};
    double beta{nan};
};

fixed_window_regression fit_finite_window(const std::span<const double> returns,
                                          const std::span<const double> factors) {
    if (returns.size() != factors.size() || returns.size() < 2U) {
        return {};
    }

    double returns_total = 0.0;
    double factors_total = 0.0;
    for (std::size_t position = 0U; position < returns.size(); ++position) {
        if (!std::isfinite(returns[position]) || !std::isfinite(factors[position])) {
            return {};
        }
        returns_total += returns[position];
        factors_total += factors[position];
    }
    const double size = static_cast<double>(returns.size());
    const double returns_mean = returns_total / size;
    const double factors_mean = factors_total / size;
    double covariance = 0.0;
    double factor_variance = 0.0;
    for (std::size_t position = 0U; position < returns.size(); ++position) {
        const double factor_residual = factors[position] - factors_mean;
        covariance += (returns[position] - returns_mean) * factor_residual;
        factor_variance += factor_residual * factor_residual;
    }
    if (factor_variance == 0.0) {
        return {};
    }

    const double beta_value = covariance / factor_variance;
    const double alpha_value = returns_mean - beta_value * factors_mean;
    if (!std::isfinite(beta_value) || !std::isfinite(alpha_value)) {
        return {};
    }
    return {alpha_value, beta_value};
}

std::vector<double> full_index_beta_values(
    const std::span<const double> returns,
    const std::span<const double> factors,
    const std::size_t rolling_window) {
    std::vector<double> result(returns.size(), nan);
    for (std::size_t start = 0U; start + rolling_window <= returns.size(); ++start) {
        const auto fit = fit_finite_window(
            returns.subspan(start, rolling_window), factors.subspan(start, rolling_window));
        result[start + rolling_window - 1U] = fit.beta;
    }
    return result;
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

dataframe roll_alpha(const dataframe& returns,
                     const dataframe& benchmark,
                     const std::size_t window,
                     const double risk_free,
                     const frequency period,
                     const std::optional<double> annualization,
                     const std::string_view returns_column_name,
                     const std::string_view benchmark_column_name) {
    return paired_endpoint_output(
        returns, benchmark, window, returns_column_name, benchmark_column_name,
        [risk_free, period, annualization, returns_column_name,
         benchmark_column_name] {
            const double factor = annualization_factor(period, annualization);
            return [risk_free, factor, returns_column_name,
                    benchmark_column_name](const dataframe& complete_returns,
                                           const dataframe& complete_benchmark) {
                return alpha(complete_returns, complete_benchmark, risk_free, factor,
                             returns_column_name, benchmark_column_name);
            };
        });
}

dataframe roll_beta(const dataframe& returns,
                    const dataframe& benchmark,
                    const std::size_t window,
                    const double risk_free,
                    const frequency period,
                    const std::optional<double> annualization,
                    const std::string_view returns_column_name,
                    const std::string_view benchmark_column_name) {
    static_cast<void>(period);
    static_cast<void>(annualization);
    return paired_endpoint_output(
        returns, benchmark, window, returns_column_name, benchmark_column_name,
        [risk_free, returns_column_name, benchmark_column_name] {
            return [risk_free, returns_column_name,
                    benchmark_column_name](const dataframe& complete_returns,
                                           const dataframe& complete_benchmark) {
                return beta(complete_returns, complete_benchmark, risk_free,
                            returns_column_name, benchmark_column_name);
            };
        });
}

dataframe roll_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    const std::size_t window,
    const double risk_free,
    const frequency period,
    const std::optional<double> annualization,
    const std::string_view returns_column_name,
    const std::string_view benchmark_column_name) {
    validate_window(window);
    auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const double factor = annualization_factor(period, annualization);
    const auto returns_values =
        require_double_column(aligned_returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(aligned_benchmark, benchmark_column_name);
    if (returns_values.size() < window) {
        return two_column_output({}, {}, {});
    }

    const auto& index = aligned_returns.get_index();
    std::vector<timestamp> output_index;
    std::vector<double> alpha_values;
    std::vector<double> beta_values;
    output_index.reserve(returns_values.size() - window + 1U);
    alpha_values.reserve(returns_values.size() - window + 1U);
    beta_values.reserve(returns_values.size() - window + 1U);
    for (std::size_t start = 0U; start + window <= returns_values.size(); ++start) {
        const auto complete_returns = materialize_complete_window(
            aligned_returns, returns_values, start, window, returns_column_name);
        const auto complete_benchmark = materialize_complete_window(
            aligned_benchmark, benchmark_values, start, window, benchmark_column_name);
        const auto [alpha_value, beta_value] = alpha_beta(
            complete_returns, complete_benchmark, risk_free, factor,
            returns_column_name, benchmark_column_name);
        output_index.push_back(index[start + window - 1U]);
        alpha_values.push_back(alpha_value);
        beta_values.push_back(beta_value);
    }
    return two_column_output(std::move(output_index), std::move(alpha_values),
                             std::move(beta_values));
}

dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::size_t rolling_window,
    const std::string_view returns_column_name,
    const std::string_view factor_column_name) {
    validate_window(rolling_window);
    auto [aligned_returns, aligned_factors] = align_on_common_index(
        returns, returns_column_name, factor_returns, factor_column_name);
    const auto returns_values =
        require_double_column(aligned_returns, returns_column_name);
    const auto factor_values =
        require_double_column(aligned_factors, factor_column_name);
    return make_series(aligned_returns.get_index(), returns_column_name,
                       full_index_beta_values(returns_values, factor_values,
                                              rolling_window));
}

dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names,
    const std::size_t rolling_window,
    const std::string_view returns_column_name) {
    validate_window(rolling_window);
    static_cast<void>(require_double_column(returns, returns_column_name));
    validate_factor_columns(factor_returns, factor_column_names);

    dataframe result;
    bool has_index = false;
    for (const auto factor_column_name : factor_column_names) {
        auto [aligned_returns, aligned_factors] = align_on_common_index(
            returns, returns_column_name, factor_returns, factor_column_name);
        const auto returns_values =
            require_double_column(aligned_returns, returns_column_name);
        const auto factor_values =
            require_double_column(aligned_factors, factor_column_name);
        if (!has_index) {
            result.load_index(std::vector<timestamp>{aligned_returns.get_index()});
            has_index = true;
        }
        result.load_column<double>(std::string{factor_column_name}.c_str(),
                                   full_index_beta_values(returns_values, factor_values,
                                                          rolling_window),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

dataframe rolling_regression(
    const dataframe& returns,
    const dataframe& benchmark,
    const std::size_t rolling_window,
    const std::string_view returns_column_name,
    const std::string_view benchmark_column_name) {
    validate_window(rolling_window);
    auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const auto returns_values =
        require_double_column(aligned_returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(aligned_benchmark, benchmark_column_name);

    std::vector<timestamp> output_index;
    std::vector<double> alpha_values;
    std::vector<double> beta_values;
    for (std::size_t start = 0U;
         start + rolling_window <= returns_values.size(); ++start) {
        const auto fit = fit_finite_window(
            returns_values.subspan(start, rolling_window),
            benchmark_values.subspan(start, rolling_window));
        if (std::isnan(fit.alpha) || std::isnan(fit.beta)) {
            continue;
        }
        output_index.push_back(
            aligned_returns.get_index()[start + rolling_window - 1U]);
        alpha_values.push_back(fit.alpha);
        beta_values.push_back(fit.beta);
    }
    return two_column_output(std::move(output_index), std::move(alpha_values),
                             std::move(beta_values));
}

dataframe roll_up_capture(const dataframe& returns,
                          const dataframe& benchmark,
                          const std::size_t window,
                          const frequency period,
                          const std::string_view returns_column_name,
                          const std::string_view benchmark_column_name) {
    return paired_endpoint_output(
        returns, benchmark, window, returns_column_name, benchmark_column_name,
        [period, returns_column_name, benchmark_column_name] {
            const double factor = annualization_factor(period, std::nullopt);
            return [factor, returns_column_name,
                    benchmark_column_name](const dataframe& complete_returns,
                                           const dataframe& complete_benchmark) {
                return up_capture(complete_returns, complete_benchmark, factor,
                                  returns_column_name, benchmark_column_name);
            };
        });
}

dataframe roll_down_capture(const dataframe& returns,
                            const dataframe& benchmark,
                            const std::size_t window,
                            const frequency period,
                            const std::string_view returns_column_name,
                            const std::string_view benchmark_column_name) {
    return paired_endpoint_output(
        returns, benchmark, window, returns_column_name, benchmark_column_name,
        [period, returns_column_name, benchmark_column_name] {
            const double factor = annualization_factor(period, std::nullopt);
            return [factor, returns_column_name,
                    benchmark_column_name](const dataframe& complete_returns,
                                           const dataframe& complete_benchmark) {
                return down_capture(complete_returns, complete_benchmark, factor,
                                    returns_column_name, benchmark_column_name);
            };
        });
}

dataframe roll_up_down_capture(const dataframe& returns,
                               const dataframe& benchmark,
                               const std::size_t window,
                               const frequency period,
                               const std::string_view returns_column_name,
                               const std::string_view benchmark_column_name) {
    return paired_endpoint_output(
        returns, benchmark, window, returns_column_name, benchmark_column_name,
        [period, returns_column_name, benchmark_column_name] {
            const double factor = annualization_factor(period, std::nullopt);
            return [factor, returns_column_name,
                    benchmark_column_name](const dataframe& complete_returns,
                                           const dataframe& complete_benchmark) {
                return up_down_capture(complete_returns, complete_benchmark, factor,
                                       returns_column_name, benchmark_column_name);
            };
        });
}

dataframe roll_annual_volatility(const dataframe& returns,
                                 const std::size_t window,
                                 const frequency period,
                                 const std::optional<double> annualization,
                                 const std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return endpoint_output(
        returns, window, column, [factor, column](const dataframe& complete) {
            return annual_volatility(complete, factor, column);
        });
}

dataframe roll_sortino_ratio(const dataframe& returns,
                             const std::size_t window,
                             const double required_return,
                             const frequency period,
                             const std::optional<double> annualization,
                             const std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return endpoint_output(
        returns, window, column,
        [factor, required_return, column](const dataframe& complete) {
            return sortino_ratio(complete, required_return, factor, column);
        });
}

}  // namespace fincore
