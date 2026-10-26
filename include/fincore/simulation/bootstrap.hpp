#pragma once

#include <fincore/simulation/parameters.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace fincore {

/// Statistic computed for each bootstrap resample.
enum class bootstrap_statistic {
    mean,                      ///< Arithmetic mean of the drawn sample.
    sample_standard_deviation, ///< Sample deviation (`n - 1`); NaN for one value.
    median,                    ///< Median, averaging the two middle values.
    sharpe_ratio,              ///< Mean / sample deviation, scaled by `sqrt(252)`.
    sortino_ratio,             ///< Mean / downside deviation, scaled by `sqrt(252)`.
    minimum,                   ///< Smallest drawn value.
    maximum,                   ///< Largest drawn value.
};

/// Name of the generated bootstrap statistic column.
inline constexpr std::string_view bootstrap_statistic_column{"bootstrap"};

/// Lower and upper bounds of a percentile bootstrap interval.
struct bootstrap_confidence_interval_result {
    double lower; ///< Lower percentile bound.
    double upper; ///< Upper percentile bound.
};

/// Summary of one bootstrapped statistic.
struct bootstrap_summary_statistic {
    double value; ///< Statistic observed on the NaN-cleaned input sample.
    double standard_error; ///< Sample deviation of the bootstrap draws.
    /// Percentile interval of the bootstrap draws.
    bootstrap_confidence_interval_result confidence_interval;
};

/// Bootstrap summaries for the mean, deviation and ratio statistics.
struct bootstrap_summary_result {
    bootstrap_summary_statistic mean; ///< Summary of the mean statistic.
    /// Summary of the sample deviation statistic.
    bootstrap_summary_statistic sample_standard_deviation;
    bootstrap_summary_statistic sharpe_ratio; ///< Summary of the Sharpe-like ratio.
    bootstrap_summary_statistic sortino_ratio; ///< Summary of the Sortino-like ratio.
};

/// Generates a bootstrap distribution of one statistic.
///
/// NaN returns are removed before resampling and infinities are rejected. Each
/// of the ``resample_count`` draws samples the retained population with
/// replacement at its original size. A supplied ``seed`` makes the native
/// 64-bit Mersenne Twister stream repeatable; omitting it seeds from
/// ``std::random_device``. Native draws are not guaranteed to match any other
/// RNG implementation.
///
/// @param returns Frame containing the selected return column.
/// @param resample_count Number of bootstrap draws; 0 yields an empty frame
///     after input validation.
/// @param statistic Statistic computed for every draw.
/// @param seed Optional deterministic seed.
/// @param column Double return column to resample.
/// @return A frame with an ordinal index and one ``bootstrap`` double column.
/// @throws input_error If the column or statistic is invalid, no non-NaN
///     return remains, a retained value is infinite, or the output size is
///     unrepresentable.
dataframe bootstrap_distribution(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

/// Generates a bootstrap distribution from explicit draw indices.
///
/// Draws address the NaN-cleaned positional input population. Every draw must
/// contain exactly that population's size and only in-range indices. No random
/// engine is used. Empty ``draw_indices`` yields an empty distribution after
/// the input and statistic are validated.
///
/// @param returns Frame containing the selected return column.
/// @param draw_indices Resampling index sets, one per output row.
/// @param statistic Statistic computed for every draw.
/// @param column Double return column to resample.
/// @return A frame with an ordinal index and one ``bootstrap`` double column.
/// @throws input_error If the column or statistic is invalid, no non-NaN
///     return remains, a retained value is infinite, a draw length or index is
///     invalid, or the output size is unrepresentable.
dataframe bootstrap_distribution(
    const dataframe& returns,
    std::span<const std::vector<std::size_t>> draw_indices,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::string_view column = returns_column);

/// Computes a percentile bootstrap confidence interval.
///
/// The interval spans the ``alpha / 2`` and ``1 - alpha / 2`` linearly
/// interpolated percentiles of the generated statistic distribution.
/// Validation of ``alpha`` happens before any resampling.
///
/// @param returns Frame containing the selected return column.
/// @param resample_count Number of bootstrap draws.
/// @param alpha Significance level in ``(0, 1)``; defaults to 0.05.
/// @param statistic Statistic computed for every draw.
/// @param seed Optional deterministic seed.
/// @param column Double return column to resample.
/// @return The lower and upper percentile bounds.
/// @throws input_error If ``alpha`` is not finite or outside ``(0, 1)``, the
///     distribution rejects the input, or generated statistics are not finite.
bootstrap_confidence_interval_result bootstrap_confidence_interval(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

/// Computes bootstrap summaries for four statistics.
///
/// Each statistic is summarized in a separate native run with the supplied
/// seed and reports its observed value, the sample deviation of its draws, and
/// the percentile interval at ``alpha / 2`` and ``1 - alpha / 2``. Validation
/// of ``alpha`` and the resample count happens before input access.
///
/// @param returns Frame containing the selected return column.
/// @param resample_count Number of draws per statistic; must be at least 2.
/// @param alpha Significance level in ``(0, 1)``; defaults to 0.05.
/// @param seed Optional deterministic seed.
/// @param column Double return column to resample.
/// @return Summaries for the mean, sample deviation, Sharpe and Sortino
///     statistics.
/// @throws input_error If ``alpha`` is invalid, ``resample_count`` is less
///     than 2, the input is malformed, or an observed or generated summary
///     value is not finite.
bootstrap_summary_result bootstrap_summary(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

}  // namespace fincore
