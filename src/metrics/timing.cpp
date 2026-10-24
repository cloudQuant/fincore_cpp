#include <fincore/calendar.hpp>
#include <fincore/metrics/timing.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr std::size_t coefficient_count = 3;
constexpr std::size_t minimum_observations = 10;
constexpr double relative_pivot_tolerance = 1.0e-12;

using coefficients = std::array<double, coefficient_count>;
using matrix = std::array<coefficients, coefficient_count>;

struct finite_aligned_pairs {
  std::vector<double> returns;
  std::vector<double> benchmark;
};

enum class timing_model { treynor_mazuy, henriksson_merton, cornell };

finite_aligned_pairs align_finite_pairs(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
  auto [aligned_returns, aligned_benchmark] = align_on_common_index(
      returns, returns_column_name, benchmark, benchmark_column_name);
  const auto return_values =
      require_double_column(aligned_returns, returns_column_name);
  const auto benchmark_values =
      require_double_column(aligned_benchmark, benchmark_column_name);

  finite_aligned_pairs pairs;
  pairs.returns.reserve(return_values.size());
  pairs.benchmark.reserve(benchmark_values.size());
  for (std::size_t position = 0; position < return_values.size(); ++position) {
    if (std::isfinite(return_values[position]) &&
        std::isfinite(benchmark_values[position])) {
      pairs.returns.push_back(return_values[position]);
      pairs.benchmark.push_back(benchmark_values[position]);
    }
  }
  return pairs;
}

coefficients design_row(double excess_market, timing_model model) {
  switch (model) {
  case timing_model::treynor_mazuy:
    return {1.0, excess_market, excess_market * excess_market};
  case timing_model::henriksson_merton:
    return {1.0, excess_market, excess_market < 0.0 ? 1.0 : 0.0};
  case timing_model::cornell:
    return {1.0, std::max(0.0, excess_market), std::min(0.0, excess_market)};
  }
  return {nan, nan, nan};
}

bool finite_coefficients(const coefficients& values) {
  return std::all_of(values.begin(), values.end(),
                     [](double value) { return std::isfinite(value); });
}

std::optional<coefficients> solve_normal_equations(matrix normal,
                                                    coefficients response) {
  double scale = 0.0;
  for (const auto& row : normal) {
    for (const double value : row) {
      if (!std::isfinite(value)) {
        return std::nullopt;
      }
      scale = std::max(scale, std::abs(value));
    }
  }
  if (!(scale > 0.0) || !finite_coefficients(response)) {
    return std::nullopt;
  }

  for (std::size_t column = 0; column < coefficient_count; ++column) {
    std::size_t pivot_row = column;
    double pivot_magnitude = std::abs(normal[column][column]);
    for (std::size_t row = column + 1; row < coefficient_count; ++row) {
      const double candidate_magnitude = std::abs(normal[row][column]);
      if (candidate_magnitude > pivot_magnitude) {
        pivot_magnitude = candidate_magnitude;
        pivot_row = row;
      }
    }
    if (!std::isfinite(pivot_magnitude) ||
        pivot_magnitude <= scale * relative_pivot_tolerance) {
      return std::nullopt;
    }
    if (pivot_row != column) {
      std::swap(normal[pivot_row], normal[column]);
      std::swap(response[pivot_row], response[column]);
    }

    const double pivot = normal[column][column];
    for (std::size_t row = column + 1; row < coefficient_count; ++row) {
      const double factor = normal[row][column] / pivot;
      if (!std::isfinite(factor)) {
        return std::nullopt;
      }
      normal[row][column] = 0.0;
      for (std::size_t entry = column + 1; entry < coefficient_count; ++entry) {
        normal[row][entry] -= factor * normal[column][entry];
      }
      response[row] -= factor * response[column];
      if (!finite_coefficients(normal[row]) || !std::isfinite(response[row])) {
        return std::nullopt;
      }
    }
  }

  coefficients result{};
  for (std::size_t offset = 0; offset < coefficient_count; ++offset) {
    const std::size_t row = coefficient_count - 1 - offset;
    const double diagonal = normal[row][row];
    if (!std::isfinite(diagonal) ||
        std::abs(diagonal) <= scale * relative_pivot_tolerance) {
      return std::nullopt;
    }
    double residual = response[row];
    for (std::size_t column = row + 1; column < coefficient_count; ++column) {
      residual -= normal[row][column] * result[column];
    }
    result[row] = residual / diagonal;
    if (!std::isfinite(result[row])) {
      return std::nullopt;
    }
  }
  return result;
}

std::optional<coefficients> fit_timing_model(const finite_aligned_pairs& pairs,
                                              double risk_free,
                                              timing_model model) {
  if (pairs.returns.size() < minimum_observations || !std::isfinite(risk_free)) {
    return std::nullopt;
  }

  matrix normal{};
  coefficients response{};
  for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
    const double excess_return = pairs.returns[position] - risk_free;
    const double excess_market = pairs.benchmark[position] - risk_free;
    const auto row = design_row(excess_market, model);
    if (!std::isfinite(excess_return) || !std::isfinite(excess_market) ||
        !finite_coefficients(row)) {
      return std::nullopt;
    }
    for (std::size_t first = 0; first < coefficient_count; ++first) {
      response[first] += row[first] * excess_return;
      for (std::size_t second = 0; second < coefficient_count; ++second) {
        normal[first][second] += row[first] * row[second];
      }
    }
  }
  return solve_normal_equations(normal, response);
}

double coefficient_for_model(const dataframe& returns,
                             const dataframe& benchmark,
                             double risk_free,
                             std::string_view returns_column_name,
                             std::string_view benchmark_column_name,
                             timing_model model,
                             std::size_t coefficient_index) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  const auto fitted = fit_timing_model(pairs, risk_free, model);
  if (!fitted.has_value()) {
    return nan;
  }
  const double result = (*fitted)[coefficient_index];
  return std::isfinite(result) ? result : nan;
}

double mean_squared_excess_market(const finite_aligned_pairs& pairs,
                                  double risk_free) {
  double total = 0.0;
  for (const double benchmark : pairs.benchmark) {
    const double excess_market = benchmark - risk_free;
    if (!std::isfinite(excess_market)) {
      return nan;
    }
    total += excess_market * excess_market;
    if (!std::isfinite(total)) {
      return nan;
    }
  }
  const double result = total / static_cast<double>(pairs.benchmark.size());
  return std::isfinite(result) ? result : nan;
}

}  // namespace

double treynor_mazuy_timing(const dataframe& returns,
                            const dataframe& benchmark,
                            double risk_free,
                            std::string_view returns_column_name,
                            std::string_view benchmark_column_name) {
  return coefficient_for_model(returns, benchmark, risk_free, returns_column_name,
                               benchmark_column_name, timing_model::treynor_mazuy, 2);
}

double henriksson_merton_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
  return coefficient_for_model(returns, benchmark, risk_free, returns_column_name,
                               benchmark_column_name,
                               timing_model::henriksson_merton, 2);
}

double market_timing_return(const dataframe& returns,
                            const dataframe& benchmark,
                            double risk_free,
                            std::string_view returns_column_name,
                            std::string_view benchmark_column_name) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  const auto fitted = fit_timing_model(pairs, risk_free, timing_model::treynor_mazuy);
  if (!fitted.has_value()) {
    return nan;
  }
  const double mean_squared_market = mean_squared_excess_market(pairs, risk_free);
  const double result = (*fitted)[2] * mean_squared_market;
  return std::isfinite(result) ? result : nan;
}

double cornell_timing(const dataframe& returns,
                      const dataframe& benchmark,
                      double risk_free,
                      std::string_view returns_column_name,
                      std::string_view benchmark_column_name) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  const auto fitted = fit_timing_model(pairs, risk_free, timing_model::cornell);
  if (!fitted.has_value()) {
    return nan;
  }
  const double result = (*fitted)[1] - (*fitted)[2];
  return std::isfinite(result) ? result : nan;
}

namespace {

struct crisis_period {
    const char* name;
    timestamp start;
    timestamp end;
};

const std::vector<crisis_period>& crisis_periods() {
    static const std::vector<crisis_period> periods{
        {"Dotcom", utc_timestamp(2000, 3, 10), utc_timestamp(2000, 9, 10)},
        {"Lehman", utc_timestamp(2008, 8, 1), utc_timestamp(2008, 10, 1)},
        {"9/11", utc_timestamp(2001, 9, 11), utc_timestamp(2001, 10, 11)},
        {"US downgrade/European Debt Crisis", utc_timestamp(2011, 8, 5),
         utc_timestamp(2011, 9, 5)},
        {"Fukushima", utc_timestamp(2011, 3, 16), utc_timestamp(2011, 4, 16)},
        {"US Housing", utc_timestamp(2003, 1, 8), utc_timestamp(2003, 2, 8)},
        {"EZB IR Event", utc_timestamp(2012, 9, 10),
         utc_timestamp(2012, 10, 10)},
        {"Aug07", utc_timestamp(2007, 8, 1), utc_timestamp(2007, 9, 1)},
        {"Mar08", utc_timestamp(2008, 3, 1), utc_timestamp(2008, 4, 1)},
        {"Sept08", utc_timestamp(2008, 9, 1), utc_timestamp(2008, 10, 1)},
        {"2009Q1", utc_timestamp(2009, 1, 1), utc_timestamp(2009, 3, 1)},
        {"2009Q2", utc_timestamp(2009, 3, 1), utc_timestamp(2009, 6, 1)},
        {"Flash Crash", utc_timestamp(2010, 5, 5), utc_timestamp(2010, 5, 10)},
        {"Apr14", utc_timestamp(2014, 4, 1), utc_timestamp(2014, 5, 1)},
        {"Oct14", utc_timestamp(2014, 10, 1), utc_timestamp(2014, 11, 1)},
        {"Fall2015", utc_timestamp(2015, 8, 15), utc_timestamp(2015, 9, 30)},
        {"Low Volatility Bull Market", utc_timestamp(2005, 1, 1),
         utc_timestamp(2007, 8, 1)},
        {"GFC Crash", utc_timestamp(2007, 8, 1), utc_timestamp(2009, 4, 1)},
        {"Recovery", utc_timestamp(2009, 4, 1), utc_timestamp(2013, 1, 1)},
        {"New Normal", utc_timestamp(2013, 1, 1), utc_timestamp(2025, 12, 31)},
    };
    return periods;
}

}  // namespace

std::vector<named_returns> extract_interesting_date_ranges(
    const dataframe& returns, std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    const auto& index = returns.get_index();
    std::vector<named_returns> result;
    for (const auto& period : crisis_periods()) {
        std::vector<timestamp> selected_index;
        std::vector<double> selected_values;
        for (std::size_t position = 0; position < values.size(); ++position) {
            if (index[position] >= period.start &&
                index[position] <= period.end) {
                selected_index.push_back(index[position]);
                selected_values.push_back(values[position]);
            }
        }
        if (selected_index.empty()) {
            continue;
        }
        result.push_back(
            {period.name,
             make_series(std::move(selected_index), column,
                         std::move(selected_values))});
    }
    return result;
}

}  // namespace fincore
