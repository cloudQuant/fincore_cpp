#include <fincore/metrics/risk.hpp>

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

} // namespace fincore
