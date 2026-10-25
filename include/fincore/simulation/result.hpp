#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <vector>

namespace fincore {

/// Terminal-value percentiles using linear interpolation.
struct simulation_terminal_percentiles {
  double percentile_1;  ///< 1st percentile.
  double percentile_5;  ///< 5th percentile.
  double percentile_25; ///< 25th percentile.
  double percentile_50; ///< 50th percentile (the median).
  double percentile_75; ///< 75th percentile.
  double percentile_95; ///< 95th percentile.
  double percentile_99; ///< 99th percentile.
};

/// Summary statistics of the terminal path values.
struct simulation_terminal_statistics {
  double mean;                      ///< Arithmetic mean of terminal values.
  double sample_standard_deviation; ///< Sample deviation (`n - 1`); NaN for one path.
  double median;                    ///< Median terminal value.
  simulation_terminal_percentiles percentiles; ///< Terminal-value percentiles.
};

/// Owning simulation result with paths, terminal statistics and risk measures.
///
/// Instances are created by ``analyze_simulation_paths``; the constructor that
/// accepts ready-made state is private. Copy and move operations copy or
/// transfer the owned path frame, statistics and terminal distribution.
class simulation_result {
public:
  /// Copies the path frame, statistics and terminal distribution.
  ///
  /// @param other Result to copy.
  simulation_result(const simulation_result &) = default;
  /// Moves the path frame, statistics and terminal distribution.
  ///
  /// @param other Result to move from.
  simulation_result(simulation_result &&) = default;
  /// Assigns by copying the path frame, statistics and terminal distribution.
  ///
  /// @param other Result to copy.
  /// @return This result after assignment.
  simulation_result &operator=(const simulation_result &) = default;
  /// Assigns by moving the path frame, statistics and terminal distribution.
  ///
  /// @param other Result to move from.
  /// @return This result after assignment.
  simulation_result &operator=(simulation_result &&) = default;
  /// Destroys the owned path frame, statistics and terminal distribution.
  ~simulation_result() = default;

  /// Returns the owned path frame.
  ///
  /// @return Const reference valid for the lifetime of this result.
  [[nodiscard]] const dataframe &paths() const noexcept;

  /// Returns the cached terminal statistics.
  ///
  /// @return Const reference to the statistics computed at construction.
  [[nodiscard]] const simulation_terminal_statistics &
  statistics() const noexcept;

  /// Returns the number of simulated paths.
  ///
  /// @return Number of path columns and terminal values.
  [[nodiscard]] std::size_t path_count() const noexcept;

  /// Returns the number of observations per path.
  ///
  /// @return Number of rows in the path frame index.
  [[nodiscard]] std::size_t observation_count() const noexcept;

  /// Computes the terminal value at risk at ``alpha``.
  ///
  /// The result is the linearly interpolated ``alpha`` percentile of the
  /// terminal values.
  ///
  /// @param alpha Tail probability in ``(0, 1)``; defaults to 0.05.
  /// @return The percentile, or NaN when ``alpha`` is not finite or outside
  ///     ``(0, 1)``.
  [[nodiscard]] double terminal_value_at_risk(double alpha = 0.05) const;

  /// Computes the terminal conditional value at risk at ``alpha``.
  ///
  /// The result is the arithmetic mean of the terminal values at or below the
  /// corresponding value at risk.
  ///
  /// @param alpha Tail probability in ``(0, 1)``; defaults to 0.05.
  /// @return The tail mean, or NaN when ``alpha`` is not finite or outside
  ///     ``(0, 1)``.
  /// @throws input_error If no terminal value lies in the tail or the mean is
  ///     not finite.
  [[nodiscard]] double
  terminal_conditional_value_at_risk(double alpha = 0.05) const;

private:
  dataframe paths_;                       ///< Owned path frame.
  simulation_terminal_statistics statistics_; ///< Statistics of terminal values.
  /// Final value of each path, in column order.
  std::vector<double> terminal_values_;

  /// Constructs a result from already validated state.
  ///
  /// @param paths Path frame to own.
  /// @param statistics Precomputed terminal statistics.
  /// @param terminal_values One terminal value per path.
  simulation_result(dataframe paths, simulation_terminal_statistics statistics,
                    std::vector<double> terminal_values);

  friend simulation_result analyze_simulation_paths(const dataframe &paths);
};

/// Analyzes a path frame into a simulation result.
///
/// The frame must have a non-empty index and only double columns, with each
/// column matching the index length. Each column's last value is its terminal
/// value and must be finite. The result owns a copy of the paths plus the
/// terminal statistics and distribution.
///
/// @param paths Path frame whose columns are simulation paths.
/// @return A result owning the copied paths, terminal statistics and terminal
///     distribution.
/// @throws input_error If the frame is structurally invalid, a terminal value
///     is not finite, or allocation fails.
[[nodiscard]] simulation_result
analyze_simulation_paths(const dataframe &paths);

} // namespace fincore
