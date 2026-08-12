#include <fincore/metrics/bayesian.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <new>
#include <span>
#include <stdexcept>
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

}  // namespace fincore
