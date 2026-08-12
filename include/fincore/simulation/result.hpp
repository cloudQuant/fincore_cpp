#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <vector>

namespace fincore {

struct simulation_terminal_percentiles {
  double percentile_1;
  double percentile_5;
  double percentile_25;
  double percentile_50;
  double percentile_75;
  double percentile_95;
  double percentile_99;
};

struct simulation_terminal_statistics {
  double mean;
  double sample_standard_deviation;
  double median;
  simulation_terminal_percentiles percentiles;
};

class simulation_result {
public:
  simulation_result(const simulation_result &) = default;
  simulation_result(simulation_result &&) = default;
  simulation_result &operator=(const simulation_result &) = default;
  simulation_result &operator=(simulation_result &&) = default;
  ~simulation_result() = default;

  [[nodiscard]] const dataframe &paths() const noexcept;
  [[nodiscard]] const simulation_terminal_statistics &
  statistics() const noexcept;
  [[nodiscard]] std::size_t path_count() const noexcept;
  [[nodiscard]] std::size_t observation_count() const noexcept;
  [[nodiscard]] double terminal_value_at_risk(double alpha = 0.05) const;
  [[nodiscard]] double
  terminal_conditional_value_at_risk(double alpha = 0.05) const;

private:
  dataframe paths_;
  simulation_terminal_statistics statistics_;
  std::vector<double> terminal_values_;

  simulation_result(dataframe paths, simulation_terminal_statistics statistics,
                    std::vector<double> terminal_values);

  friend simulation_result analyze_simulation_paths(const dataframe &paths);
};

[[nodiscard]] simulation_result
analyze_simulation_paths(const dataframe &paths);

} // namespace fincore
