#include <fincore/metrics/bayesian.hpp>

#include <fincore/detail/mt19937.hpp>
#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr std::array<double, 7U> bayesian_cone_percentiles{
    .01, .05, .25, .50, .75, .95, .99};

constexpr std::array<std::string_view, 7U> bayesian_cone_column_names{
    bayesian_cone_percentile_1_column,
    bayesian_cone_percentile_5_column,
    bayesian_cone_percentile_25_column,
    bayesian_cone_percentile_50_column,
    bayesian_cone_percentile_75_column,
    bayesian_cone_percentile_95_column,
    bayesian_cone_percentile_99_column};

void require_finite(const double value, const char* const message) {
    if (!std::isfinite(value)) {
        throw input_error{message};
    }
}

double checked_percentile(const std::span<const double> wealths,
                          const double percentile) {
    const double value = detail::linear_percentile(wealths, percentile);
    require_finite(value, "bayesian cone percentile is not finite");
    return value;
}

// Each row's percentiles are taken across the paths at that point in time, over a
// wealth vector updated in place, so a column of the result is a distribution of
// outcomes at one step rather than one path's history.
dataframe compute_bayes_cone_impl(const dataframe& predicted_returns,
                                  const double starting_value) {
    require_finite(starting_value, "bayesian cone starting value must be finite");

    const auto& index = predicted_returns.get_index();
    const auto shape = predicted_returns.shape();
    const auto double_columns = predicted_returns.get_columns_info<double>();
    if (shape.second == 0U || double_columns.size() != shape.second) {
        throw input_error{"bayesian cone paths must contain only double columns"};
    }

    std::vector<std::span<const double>> paths;
    paths.reserve(double_columns.size());
    for (const auto& info : double_columns) {
        const auto& values =
            predicted_returns.get_column<double>(std::get<0>(info).c_str());
        if (values.size() != index.size()) {
            throw input_error{
                "bayesian cone path columns must match the frame index length"};
        }
        for (const double value : values) {
            require_finite(value, "bayesian cone returns must be finite");
            require_finite(1.0 + value,
                           "bayesian cone return multiplier must be finite");
        }
        paths.emplace_back(values.data(), values.size());
    }

    std::vector<double> wealths(paths.size(), starting_value);
    std::array<std::vector<double>, 7U> percentile_values;
    for (auto& values : percentile_values) {
        values.reserve(index.size());
    }

    for (std::size_t row = 0U; row < index.size(); ++row) {
        for (std::size_t path = 0U; path < paths.size(); ++path) {
            wealths[path] *= 1.0 + paths[path][row];
            require_finite(wealths[path],
                           "bayesian cone cumulative wealth must be finite");
        }
        const std::span<const double> current_wealths{wealths.data(), wealths.size()};
        for (std::size_t percentile = 0U;
             percentile < bayesian_cone_percentiles.size(); ++percentile) {
            percentile_values[percentile].push_back(
                checked_percentile(current_wealths,
                                   bayesian_cone_percentiles[percentile]));
        }
    }

    dataframe result;
    result.load_index(std::vector<timestamp>{index.begin(), index.end()});
    for (std::size_t percentile = 0U;
         percentile < bayesian_cone_column_names.size(); ++percentile) {
        result.load_column<double>(bayesian_cone_column_names[percentile].data(),
                                   std::move(percentile_values[percentile]),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

double compound_return(const double wealth,
                       const double simple_return,
                       const bool nan_as_zero,
                       const char* const message) {
    double value = simple_return;
    if (std::isnan(value)) {
        if (!nan_as_zero) {
            throw input_error{message};
        }
        value = 0.0;
    }
    if (!std::isfinite(value) || !std::isfinite(1.0 + value)) {
        throw input_error{message};
    }

    const double next = wealth * (1.0 + value);
    if (!std::isfinite(next)) {
        throw input_error{message};
    }
    return next;
}

// A starting value of zero has no multiplicative reading — every path would scale
// to zero — so the result is expressed as the excess over one instead, which makes
// that cone report returns where the others report levels.
double scale_wealth(const double compounded,
                    const double starting_value) {
    const double scaled = starting_value == 0.0
                              ? compounded - 1.0
                              : compounded * starting_value;
    if (!std::isfinite(scaled)) {
        throw input_error{"bayesian cone cumulative wealth must be finite"};
    }
    return scaled;
}

std::vector<timestamp> positional_index(const std::size_t count) {
    std::vector<timestamp> index;
    index.reserve(count);
    for (std::size_t position = 0U; position < count; ++position) {
        index.push_back(static_cast<timestamp>(position));
    }
    return index;
}

std::size_t random_index(detail::numpy_mt19937& engine,
                         const std::size_t bound) {
    if (bound == 0U) {
        throw input_error{
            "simulate_paths requires at least one in-sample return"};
    }
    if (bound > std::numeric_limits<std::uint32_t>::max()) {
        throw input_error{
            "simulate_paths population is too large for a 32-bit draw"};
    }
    return static_cast<std::size_t>(
        engine.randint(static_cast<std::uint32_t>(bound)));
}

// The formatted number becomes a column name, so a zero written with a minus sign
// would give the same bound two identities; the rewrite makes negative zero print
// exactly as positive zero does.
std::string shortest_number(const double value) {
    const double normalized = value == 0.0 ? 0.0 : value;
    std::array<char, 64U> buffer{};
    const auto result = std::to_chars(buffer.data(),
                                      buffer.data() + buffer.size(),
                                      normalized);
    if (result.ec != std::errc{}) {
        throw input_error{"cone bound width is not representable"};
    }
    return std::string{buffer.data(), result.ptr};
}

}  // namespace

dataframe compute_bayes_cone(const dataframe& predicted_returns,
                             const double starting_value) {
    try {
        return compute_bayes_cone_impl(predicted_returns, starting_value);
    } catch (const std::bad_alloc&) {
        throw input_error{"bayesian cone allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"bayesian cone allocation is not representable"};
    }
}

std::vector<double> compute_consistency_score(
    const dataframe& returns_test,
    const matrix& predicted_returns,
    const std::string_view column) {
    const auto returns = require_double_column(returns_test, column);
    if (returns.size() != predicted_returns.columns()) {
        throw input_error{
            "consistency score requires one realized return per predicted day"};
    }

    std::vector<double> realized(returns.size(), detail::nan);
    double wealth = 1.0;
    for (std::size_t day = 0U; day < returns.size(); ++day) {
        wealth = compound_return(
            wealth, returns[day], true,
            "consistency score realized returns must be finite or NaN");
        realized[day] = wealth;
    }

    std::vector<std::size_t> below(returns.size(), 0U);
    for (std::size_t path = 0U; path < predicted_returns.rows(); ++path) {
        double path_wealth = 1.0;
        for (std::size_t day = 0U; day < returns.size(); ++day) {
            path_wealth = compound_return(
                path_wealth, predicted_returns.at(path, day), false,
                "consistency score predicted returns must be finite");
            if (path_wealth < realized[day]) {
                ++below[day];
            }
        }
    }

    std::vector<double> scores(returns.size(), detail::nan);
    if (predicted_returns.rows() == 0U) {
        return scores;
    }
    for (std::size_t day = 0U; day < scores.size(); ++day) {
        scores[day] = static_cast<double>(below[day]) /
                      static_cast<double>(predicted_returns.rows());
    }
    return scores;
}

matrix simulate_paths(const std::span<const double> in_sample_returns,
                      const std::size_t num_days,
                      const double starting_value,
                      const std::size_t num_samples,
                      const std::uint32_t random_seed) {
    // Source `_starting_value` is unused by simulate_paths.
    (void)starting_value;

    if (num_days != 0U &&
        num_samples > std::vector<double>{}.max_size() / num_days) {
        throw input_error{"simulated path shape is not representable"};
    }
    for (const double value : in_sample_returns) {
        if (!std::isfinite(value) && !std::isnan(value)) {
            throw input_error{
                "simulate_paths in-sample returns must be finite or NaN"};
        }
    }

    const std::size_t draw_count = num_samples * num_days;
    if (draw_count != 0U && in_sample_returns.empty()) {
        throw input_error{
            "simulate_paths requires at least one in-sample return"};
    }

    std::vector<double> draws;
    draws.reserve(draw_count);
    detail::numpy_mt19937 engine{random_seed};
    for (std::size_t draw = 0U; draw < draw_count; ++draw) {
        draws.push_back(
            in_sample_returns[random_index(engine, in_sample_returns.size())]);
    }
    return matrix{num_samples, num_days, std::move(draws)};
}

matrix simulate_paths(const dataframe& in_sample_returns,
                      const std::size_t num_days,
                      const double starting_value,
                      const std::size_t num_samples,
                      const std::uint32_t random_seed,
                      const std::string_view column) {
    return simulate_paths(require_double_column(in_sample_returns, column),
                          num_days, starting_value, num_samples,
                          random_seed);
}

std::string cone_bounds_column_name(const double num_std) {
    require_finite(num_std, "cone bound width must be finite");
    return std::string{"cone_"} + shortest_number(num_std);
}

dataframe summarize_paths(const matrix& samples,
                          const std::span<const double> cone_std,
                          const double starting_value) {
    require_finite(starting_value, "cone starting value must be finite");

    std::vector<double> widths;
    widths.reserve(cone_std.size());
    for (const double width : cone_std) {
        require_finite(width, "cone bound width must be finite");
        widths.push_back(width);
    }
    if (widths.empty()) {
        return dataframe{};
    }

    const std::size_t path_count = samples.rows();
    const std::size_t day_count = samples.columns();
    if (day_count != 0U &&
        path_count > std::vector<double>{}.max_size() / day_count) {
        throw input_error{"bayesian cone shape is not representable"};
    }

    std::vector<double> cumulative_values(path_count * day_count);
    matrix cumulative{path_count, day_count, std::move(cumulative_values)};

    for (std::size_t path = 0U; path < path_count; ++path) {
        double wealth = 1.0;
        for (std::size_t day = 0U; day < day_count; ++day) {
            wealth = compound_return(
                wealth, samples.at(path, day), true,
                "bayesian cone returns must be finite or NaN");
            cumulative.at(path, day) = scale_wealth(wealth, starting_value);
        }
    }

    std::vector<double> means(day_count, detail::nan);
    std::vector<double> standard_deviations(day_count, detail::nan);
    std::vector<double> column_values(path_count, 0.0);
    for (std::size_t day = 0U; day < day_count; ++day) {
        for (std::size_t path = 0U; path < path_count; ++path) {
            column_values[path] = cumulative.at(path, day);
        }
        const std::span<const double> values{column_values.data(),
                                             column_values.size()};
        means[day] = detail::mean(values);
        standard_deviations[day] = std::sqrt(detail::sample_variance(values));
    }

    std::vector<std::string> names;
    names.reserve(widths.size() * 2U);
    for (const double width : widths) {
        names.push_back(cone_bounds_column_name(width));
        names.push_back(cone_bounds_column_name(-width));
    }
    for (std::size_t position = 0U; position < names.size(); ++position) {
        if (std::find(names.begin(), names.begin() +
                                     static_cast<std::ptrdiff_t>(position),
                      names[position]) !=
            names.begin() + static_cast<std::ptrdiff_t>(position)) {
            throw input_error{"cone standard deviations must be unique"};
        }
    }

    dataframe result;
    result.load_index(positional_index(day_count));
    for (std::size_t width_index = 0U; width_index < widths.size();
         ++width_index) {
        std::vector<double> upper(day_count, detail::nan);
        std::vector<double> lower(day_count, detail::nan);
        for (std::size_t day = 0U; day < day_count; ++day) {
            upper[day] =
                means[day] + standard_deviations[day] * widths[width_index];
            lower[day] =
                means[day] - standard_deviations[day] * widths[width_index];
        }
        result.load_column<double>(
            names[2U * width_index].c_str(), std::move(upper),
            hmdf::nan_policy::dont_pad_with_nans);
        result.load_column<double>(
            names[2U * width_index + 1U].c_str(), std::move(lower),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

dataframe summarize_paths(const matrix& samples,
                          const double cone_std,
                          const double starting_value) {
    const std::array<double, 1U> widths{cone_std};
    return summarize_paths(samples,
                           std::span<const double>{widths.data(), widths.size()},
                           starting_value);
}

dataframe forecast_cone_bootstrap(
    const dataframe& in_sample_returns,
    const std::size_t num_days,
    const std::span<const double> cone_std,
    const double starting_value,
    const std::size_t num_samples,
    const std::uint32_t random_seed,
    const std::string_view column) {
    const auto samples = simulate_paths(in_sample_returns, num_days,
                                        starting_value, num_samples,
                                        random_seed, column);
    return summarize_paths(samples, cone_std, starting_value);
}

dataframe forecast_cone_bootstrap(
    const dataframe& in_sample_returns,
    const std::size_t num_days,
    const double cone_std,
    const double starting_value,
    const std::size_t num_samples,
    const std::uint32_t random_seed,
    const std::string_view column) {
    const std::array<double, 1U> widths{cone_std};
    return forecast_cone_bootstrap(
        in_sample_returns, num_days,
        std::span<const double>{widths.data(), widths.size()}, starting_value,
        num_samples, random_seed, column);
}

}  // namespace fincore
