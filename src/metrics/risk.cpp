#include <fincore/metrics/risk.hpp>

#include <fincore/detail/optimization.hpp>

#include <fincore/detail/numeric.hpp>

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
  return !values.empty() && std::isfinite(cutoff) && cutoff >= 0.0 &&
         cutoff <= 1.0 &&
         std::all_of(values.begin(), values.end(),
                     [](double value) { return std::isfinite(value); });
}

struct paired_returns {
  std::vector<timestamp> index;
  std::vector<double> returns;
  std::vector<double> benchmark;
};

paired_returns aligned_finite_pairs(const dataframe &returns,
                                    const dataframe &benchmark,
                                    std::string_view returns_column_name,
                                    std::string_view benchmark_column_name) {
  auto [aligned_returns, aligned_benchmark] = align_on_common_index(
      returns, returns_column_name, benchmark, benchmark_column_name);
  const auto return_values =
      require_double_column(aligned_returns, returns_column_name);
  const auto benchmark_values =
      require_double_column(aligned_benchmark, benchmark_column_name);

  paired_returns result;
  const auto &aligned_index = aligned_returns.get_index();
  result.index.reserve(return_values.size());
  result.returns.reserve(return_values.size());
  result.benchmark.reserve(benchmark_values.size());
  for (std::size_t position = 0; position < return_values.size(); ++position) {
    if (std::isfinite(return_values[position]) &&
        std::isfinite(benchmark_values[position])) {
      result.index.push_back(aligned_index[position]);
      result.returns.push_back(return_values[position]);
      result.benchmark.push_back(benchmark_values[position]);
    }
  }
  return result;
}

double annualized_sample_deviation(std::span<const double> values,
                                   double annualization) {
  if (values.size() < 2 || !std::isfinite(annualization) ||
      annualization < 0.0) {
    return nan;
  }
  return std::sqrt(detail::sample_variance(values)) * std::sqrt(annualization);
}

} // namespace

double historical_value_at_risk(const dataframe &returns, double cutoff,
                                std::string_view column) {
  const auto values = require_double_column(returns, column);
  if (!valid_risk_input(values, cutoff)) {
    return nan;
  }
  return percentile({values.begin(), values.end()}, cutoff);
}

double trading_value_at_risk(const dataframe& returns, double sigma,
                             std::string_view column) {
  if (column.find('\0') != std::string_view::npos) {
    throw input_error{"column name must not contain an embedded NUL"};
  }
  const auto values = require_double_column(returns, column);
  if (values.size() < 2) {
    return nan;
  }

  const double average = detail::mean(values);
  const double standard_deviation =
      std::sqrt(detail::sample_variance(values));
  return average - sigma * standard_deviation;
}

double var_excess_return(const dataframe& returns, double cutoff,
                         double risk_free, frequency period,
                         std::optional<double> annualization,
                         std::string_view column) {
  if (column.find('\0') != std::string_view::npos) {
    throw input_error{"column name must not contain an embedded NUL"};
  }
  const auto values = require_double_column(returns, column);
  if (values.size() < 2) {
    return nan;
  }

  const double factor = annualization_factor(period, annualization);
  const double annualized_return = annual_return(returns, factor, column);
  const double value_at_risk =
      historical_value_at_risk(returns, cutoff, column);
  if (value_at_risk == 0.0 || std::isnan(value_at_risk) ||
      std::isnan(annualized_return)) {
    return nan;
  }
  return (annualized_return - risk_free) / std::abs(value_at_risk);
}

double conditional_value_at_risk(const dataframe &returns, double cutoff,
                                 std::string_view column) {
  const auto values = require_double_column(returns, column);
  if (!valid_risk_input(values, cutoff)) {
    return nan;
  }
  const double value_at_risk =
      percentile({values.begin(), values.end()}, cutoff);
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

double downside_risk(const dataframe &returns, double required_return,
                     double annualization, std::string_view column) {
  const auto values =
      detail::finite_values(require_double_column(returns, column));
  if (values.empty() || !std::isfinite(required_return) ||
      !std::isfinite(annualization) || annualization < 0.0) {
    return nan;
  }

  double squared_downside = 0.0;
  for (const double value : values) {
    const double excess = value - required_return;
    if (excess < 0.0) {
      squared_downside += excess * excess;
    }
  }
  return std::sqrt(squared_downside / static_cast<double>(values.size())) *
         std::sqrt(annualization);
}

double tail_ratio(const dataframe &returns, std::string_view column) {
  const auto values =
      detail::finite_values(require_double_column(returns, column));
  if (values.empty()) {
    return nan;
  }

  const double lower_tail = std::abs(detail::linear_percentile(values, 0.05));
  if (lower_tail == 0.0) {
    return nan;
  }
  return std::abs(detail::linear_percentile(values, 0.95)) / lower_tail;
}

double tracking_error(const dataframe &returns, const dataframe &benchmark,
                      double annualization,
                      std::string_view returns_column_name,
                      std::string_view benchmark_column_name) {
  const auto pairs = aligned_finite_pairs(
      returns, benchmark, returns_column_name, benchmark_column_name);
  std::vector<double> active_returns;
  active_returns.reserve(pairs.returns.size());
  for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
    active_returns.push_back(pairs.returns[position] -
                             pairs.benchmark[position]);
  }
  return annualized_sample_deviation(active_returns, annualization);
}

double residual_risk(const dataframe &returns, const dataframe &benchmark,
                     double risk_free, double annualization,
                     std::string_view returns_column_name,
                     std::string_view benchmark_column_name) {
  const auto pairs = aligned_finite_pairs(
      returns, benchmark, returns_column_name, benchmark_column_name);
  if (!std::isfinite(risk_free)) {
    return nan;
  }

  const auto finite_returns =
      make_series(pairs.index, returns_column_name, pairs.returns);
  const auto finite_benchmark =
      make_series(pairs.index, benchmark_column_name, pairs.benchmark);
  const double beta_value = beta(finite_returns, finite_benchmark, risk_free,
                                 returns_column_name, benchmark_column_name);
  if (!std::isfinite(beta_value)) {
    return nan;
  }

  std::vector<double> residuals;
  residuals.reserve(pairs.returns.size());
  for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
    residuals.push_back((pairs.returns[position] - risk_free) -
                        beta_value * (pairs.benchmark[position] - risk_free));
  }
  return annualized_sample_deviation(residuals, annualization);
}

double var_cov_var_normal(double portfolio_value, double confidence,
                          double mean, double standard_deviation) {
  if (!std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0 ||
      !std::isfinite(standard_deviation) || standard_deviation <= 0.0) {
    return nan;
  }
  return portfolio_value *
         (detail::inverse_standard_normal_cdf(confidence) * standard_deviation -
          mean);
}


namespace {

double gpd_scale_and_shape_loglikelihood(const double scale,
                                         const double shape,
                                         const std::vector<double>& tail) {
    const double count = static_cast<double>(tail.size());
    double result = -std::numeric_limits<double>::max();
    if (scale != 0.0) {
        const double param_factor = shape / scale;
        if (shape != 0.0 && param_factor >= 0.0 && scale >= 0.0) {
            double total = 0.0;
            for (const double value : tail) {
                total += std::log(shape / scale * value + 1.0);
            }
            result = -count * std::log(scale) -
                     (1.0 / shape + 1.0) * total;
        }
    }
    return result;
}

double gpd_scale_only_loglikelihood(const double scale,
                                    const std::vector<double>& tail) {
    const double count = static_cast<double>(tail.size());
    double result = -std::numeric_limits<double>::max();
    if (scale >= 0.0) {
        double total = 0.0;
        for (const double value : tail) {
            total += value;
        }
        result = -count * std::log(scale) - total / scale;
    }
    return result;
}

double gpd_negative_loglikelihood(const std::vector<double>& params,
                                  const std::vector<double>& tail) {
    const double scale = params[0];
    const double shape = params[1];
    if (shape != 0.0) {
        return -gpd_scale_and_shape_loglikelihood(scale, shape, tail);
    }
    return -gpd_scale_only_loglikelihood(scale, tail);
}

double gpd_var_calculator(const double threshold, const double scale,
                          const double shape, const double probability,
                          const std::size_t total_n,
                          const std::size_t exceedance_n) {
    if (exceedance_n > 0U && shape > 0.0) {
        const double param_ratio = scale / shape;
        const double prob_ratio =
            static_cast<double>(total_n) /
            static_cast<double>(exceedance_n) * probability;
        return threshold +
               param_ratio * (std::pow(prob_ratio, -shape) - 1.0);
    }
    return 0.0;
}

double gpd_es_calculator(const double var_estimate, const double threshold,
                         const double scale, const double shape) {
    if (1.0 - shape != 0.0) {
        const double var_ratio = var_estimate / (1.0 - shape);
        const double param_ratio =
            (scale - shape * threshold) / (1.0 - shape);
        return var_ratio + param_ratio;
    }
    return 0.0;
}

}  // namespace

gpd_risk_estimates_result gpd_risk_estimates(
    const dataframe& returns, const double var_p,
    const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    gpd_risk_estimates_result result{0.0, 0.0, 0.0, 0.0, 0.0};
    if (values.size() < 3U) {
        return result;
    }
    std::vector<double> losses;
    losses.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value) && -value > 0.0) {
            losses.push_back(-value);
        }
    }
    const std::size_t total_losses = losses.size();

    double threshold = 0.2;
    constexpr double minimum_threshold = 1e-9;
    bool finished = false;
    double scale_param = 0.0;
    double shape_param = 0.0;
    double var_estimate = 0.0;
    double used_threshold = 0.0;
    while (!finished && threshold > minimum_threshold) {
        std::vector<double> tail;
        for (const double loss : losses) {
            if (loss >= threshold) {
                tail.push_back(loss);
            }
        }
        if (!tail.empty()) {
            detail::nelder_mead_options options;
            options.function_tolerance = 1e-4;
            options.simplex_tolerance = 1e-4;
            options.max_iterations = 400U;
            const auto optimized = detail::minimize_nelder_mead(
                [&tail](const std::vector<double>& params) {
                    return gpd_negative_loglikelihood(params, tail);
                },
                {1.0, 1.0}, options);
            if (optimized.converged) {
                const double candidate_scale = optimized.parameters[0];
                const double candidate_shape = optimized.parameters[1];
                const double candidate_var = gpd_var_calculator(
                    threshold, candidate_scale, candidate_shape, var_p,
                    total_losses, tail.size());
                if (candidate_shape > 0.0 && candidate_var > 0.0) {
                    scale_param = candidate_scale;
                    shape_param = candidate_shape;
                    var_estimate = candidate_var;
                    used_threshold = threshold;
                    finished = true;
                }
            }
        }
        if (!finished) {
            threshold /= 2.0;
        }
    }
    if (finished) {
        result.threshold = used_threshold;
        result.scale = scale_param;
        result.shape = shape_param;
        result.var = var_estimate;
        result.es = gpd_es_calculator(var_estimate, used_threshold,
                                      scale_param, shape_param);
    }
    return result;
}

double beta_fragility_heuristic(
    const dataframe& returns, const dataframe& benchmark,
    const std::string_view returns_column_name,
    const std::string_view benchmark_column_name) {
    if (returns_column_name.find('\0') != std::string_view::npos ||
        benchmark_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto aligned = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const auto return_values =
        require_double_column(aligned.first, returns_column_name);
    const auto factor_values =
        require_double_column(aligned.second, benchmark_column_name);
    if (return_values.size() < 3U || factor_values.size() < 3U) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    struct pair_value {
        double returns;
        double factor;
    };
    std::vector<pair_value> pairs;
    pairs.reserve(return_values.size());
    for (std::size_t position = 0; position < return_values.size();
         ++position) {
        if (!std::isnan(return_values[position]) &&
            !std::isnan(factor_values[position])) {
            pairs.push_back({return_values[position], factor_values[position]});
        }
    }
    if (pairs.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::stable_sort(pairs.begin(), pairs.end(),
                     [](const pair_value& left, const pair_value& right) {
                         return left.factor < right.factor;
                     });
    const std::size_t end_index = pairs.size() - 1U;
    const auto mid_index = static_cast<std::size_t>(
        std::nearbyint(static_cast<double>(pairs.size()) / 2.0));
    const auto& start = pairs.front();
    const auto& middle = pairs[mid_index];
    const auto& end = pairs.back();
    const double factor_range = end.factor - start.factor;
    double start_weight = 0.5;
    double end_weight = 0.5;
    if (factor_range != 0.0) {
        start_weight = (middle.factor - start.factor) / factor_range;
        end_weight = (end.factor - middle.factor) / factor_range;
    }
    static_cast<void>(end_index);
    return start_weight * start.returns + end_weight * end.returns -
           middle.returns;
}

} // namespace fincore
