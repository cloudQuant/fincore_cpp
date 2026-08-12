#include <fincore/simulation/paths.hpp>

#include <fincore/error.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <numeric>
#include <numbers>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct output_shape {
    std::size_t step_count;
    std::size_t row_count;
};

struct propagation_terms {
    double drift_per_step;
    double diffusion_scale;
};

void require_finite(const double value, const char* const message) {
    if (!std::isfinite(value)) {
        throw input_error{message};
    }
}

std::string checked_return_column_name(const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"GBM returns column name must not contain an embedded NUL"};
    }
    return std::string{column};
}

void validate_model_inputs(const double initial_value,
                           const double annualized_drift,
                           const double annualized_volatility,
                           const double total_years,
                           const double time_step,
                           const std::size_t path_count) {
    require_finite(initial_value, "GBM initial value must be finite");
    require_finite(annualized_drift, "GBM annualized drift must be finite");
    require_finite(annualized_volatility,
                   "GBM annualized volatility must be finite");
    require_finite(total_years, "GBM total years must be finite");
    require_finite(time_step, "GBM time step must be finite");
    if (time_step <= 0.0) {
        throw input_error{"GBM time step must be positive"};
    }
    if (total_years < 0.0) {
        throw input_error{"GBM total years must be nonnegative"};
    }
    if (path_count == 0U) {
        throw input_error{"GBM path count must be positive"};
    }
}

std::size_t checked_step_count(const double total_years, const double time_step) {
    const double raw_step_count = total_years / time_step;
    require_finite(raw_step_count, "GBM step count is not representable");

    const double truncated_step_count = std::trunc(raw_step_count);
    if (!std::isfinite(truncated_step_count) || truncated_step_count < 0.0) {
        throw input_error{"GBM step count is not representable"};
    }

    // Converting either integral bound to double can round upward on common
    // targets, so rejecting either boundary keeps the cast below defined for
    // every supported build.
    const double timestamp_limit =
        static_cast<double>(std::numeric_limits<timestamp>::max());
    const double size_limit =
        static_cast<double>(std::numeric_limits<std::size_t>::max());
    if (truncated_step_count >= timestamp_limit ||
        truncated_step_count >= size_limit) {
        throw input_error{"GBM step index is not representable"};
    }
    return static_cast<std::size_t>(truncated_step_count);
}

void validate_path_name_capacity(const std::size_t path_count) {
    const std::string largest_suffix = std::to_string(path_count - 1U);
    if (gbm_path_column_prefix.size() >
        std::string{}.max_size() - largest_suffix.size()) {
        throw input_error{"GBM path name is not representable"};
    }
}

output_shape validate_output_shape(const std::size_t step_count,
                                   const std::size_t path_count) {
    if (step_count == std::numeric_limits<std::size_t>::max()) {
        throw input_error{"GBM output row count is not representable"};
    }
    const std::size_t row_count = step_count + 1U;

    if (row_count > std::vector<timestamp>{}.max_size() ||
        row_count > std::vector<double>{}.max_size() ||
        path_count > std::vector<std::string>{}.max_size()) {
        throw input_error{"GBM output allocation is not representable"};
    }
    if (path_count > std::numeric_limits<std::size_t>::max() / row_count) {
        throw input_error{"GBM output cell count is not representable"};
    }
    const std::size_t cell_count = row_count * path_count;
    if (cell_count > std::vector<double>{}.max_size()) {
        throw input_error{"GBM output allocation is not representable"};
    }

    validate_path_name_capacity(path_count);
    return {step_count, row_count};
}

std::vector<timestamp> make_ordinal_index(const std::size_t row_count) {
    std::vector<timestamp> index;
    index.reserve(row_count);
    for (std::size_t row = 0U; row < row_count; ++row) {
        index.push_back(static_cast<timestamp>(row));
    }
    return index;
}

std::string path_name(const std::size_t path_number) {
    return std::string{gbm_path_column_prefix} + std::to_string(path_number);
}

void validate_latin_hypercube_shape(const std::size_t sample_count,
                                    const std::size_t dimension_count) {
    if (sample_count != 0U &&
        sample_count - 1U >
            static_cast<std::size_t>(std::numeric_limits<timestamp>::max())) {
        throw input_error{"Latin-hypercube sample index is not representable"};
    }
    if (sample_count > std::vector<timestamp>{}.max_size()) {
        throw input_error{"Latin-hypercube sample index allocation is not representable"};
    }

    if (dimension_count != 0U) {
        const std::string largest_suffix = std::to_string(dimension_count - 1U);
        const std::size_t string_capacity = std::string{}.max_size();
        if (largest_suffix.size() > string_capacity ||
            latin_hypercube_dimension_column_prefix.size() >
                string_capacity - largest_suffix.size()) {
            throw input_error{"Latin-hypercube dimension name is not representable"};
        }
    }
    if (dimension_count > std::vector<std::string>{}.max_size()) {
        throw input_error{"Latin-hypercube dimension allocation is not representable"};
    }

    if (sample_count == 0U || dimension_count == 0U) {
        return;
    }
    if (sample_count > std::vector<std::size_t>{}.max_size() ||
        sample_count > std::vector<double>{}.max_size()) {
        throw input_error{"Latin-hypercube sample allocation is not representable"};
    }
    if (dimension_count > std::numeric_limits<std::size_t>::max() / sample_count) {
        throw input_error{"Latin-hypercube output cell count is not representable"};
    }
    const std::size_t cell_count = sample_count * dimension_count;
    if (cell_count > std::vector<double>{}.max_size()) {
        throw input_error{"Latin-hypercube output allocation is not representable"};
    }
}

std::string latin_hypercube_dimension_name(const std::size_t dimension) {
    return std::string{latin_hypercube_dimension_column_prefix} +
           std::to_string(dimension);
}

std::mt19937_64 make_engine(const std::optional<std::uint64_t> seed) {
    if (seed.has_value()) {
        return std::mt19937_64{*seed};
    }

    std::random_device device;
    return std::mt19937_64{static_cast<std::uint64_t>(device())};
}

double open_unit_uniform(std::mt19937_64& engine) {
    // Retain 52 engine bits and use the midpoint of each bin. This avoids
    // zero and one without invoking a distribution whose endpoint contract
    // would make log(0) possible.
    constexpr unsigned discarded_bits = 12U;
    constexpr int retained_bits = 52;
    const std::uint64_t mantissa =
        static_cast<std::uint64_t>(engine()) >> discarded_bits;
    const double uniform = std::ldexp(static_cast<double>(mantissa), -retained_bits) +
                           std::ldexp(1.0, -(retained_bits + 1));
    if (!std::isfinite(uniform) || uniform <= 0.0 || uniform >= 1.0) {
        throw input_error{"GBM random uniform is not finite"};
    }
    return uniform;
}

std::size_t rejection_mapped_index(std::mt19937_64& engine,
                                   const std::size_t upper_bound) {
    if (upper_bound == 0U) {
        throw input_error{"Latin-hypercube bounded index must be positive"};
    }
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
        if (upper_bound > static_cast<std::size_t>(
                              std::numeric_limits<std::uint64_t>::max())) {
            throw input_error{"Latin-hypercube bounded index is not representable"};
        }
    }

    const std::uint64_t bound = static_cast<std::uint64_t>(upper_bound);
    const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
    for (;;) {
        const std::uint64_t candidate = engine();
        if (candidate >= threshold) {
            return static_cast<std::size_t>(candidate % bound);
        }
    }
}

std::vector<std::size_t> latin_hypercube_permutation(
    const std::size_t sample_count,
    std::mt19937_64& engine) {
    std::vector<std::size_t> permutation(sample_count);
    std::iota(permutation.begin(), permutation.end(), std::size_t{0});
    for (std::size_t remaining = sample_count; remaining > 1U; --remaining) {
        std::swap(permutation[remaining - 1U],
                  permutation[rejection_mapped_index(engine, remaining)]);
    }
    return permutation;
}

double standard_normal(std::mt19937_64& engine) {
    const double radial_argument = -2.0 * std::log(open_unit_uniform(engine));
    require_finite(radial_argument, "GBM Box-Muller radial argument is not finite");
    const double radius = std::sqrt(radial_argument);
    require_finite(radius, "GBM Box-Muller radius is not finite");

    const double angle = 2.0 * std::numbers::pi_v<double> *
                         open_unit_uniform(engine);
    require_finite(angle, "GBM Box-Muller angle is not finite");
    const double normal = radius * std::cos(angle);
    require_finite(normal, "GBM Box-Muller normal is not finite");
    return normal;
}

propagation_terms checked_propagation_terms(
    const double annualized_drift,
    const double annualized_volatility,
    const double time_step) {
    const double squared_volatility =
        annualized_volatility * annualized_volatility;
    require_finite(squared_volatility, "GBM volatility square is not finite");
    const double half_squared_volatility = 0.5 * squared_volatility;
    require_finite(half_squared_volatility,
                   "GBM volatility adjustment is not finite");
    const double adjusted_drift = annualized_drift - half_squared_volatility;
    require_finite(adjusted_drift, "GBM adjusted drift is not finite");
    const double drift_per_step = adjusted_drift * time_step;
    require_finite(drift_per_step, "GBM drift per step is not finite");

    const double square_root_step = std::sqrt(time_step);
    require_finite(square_root_step, "GBM time-step square root is not finite");
    const double diffusion_scale = annualized_volatility * square_root_step;
    require_finite(diffusion_scale, "GBM diffusion scale is not finite");
    return {drift_per_step, diffusion_scale};
}

std::vector<double> generate_path(const double initial_value,
                                  const output_shape shape,
                                  const propagation_terms terms,
                                  std::mt19937_64& engine) {
    std::vector<double> values(shape.row_count);
    values[0] = initial_value;
    for (std::size_t step = 1U; step < shape.row_count; ++step) {
        const double shock = terms.diffusion_scale * standard_normal(engine);
        require_finite(shock, "GBM diffusion shock is not finite");
        const double exponent = terms.drift_per_step + shock;
        require_finite(exponent, "GBM path exponent is not finite");
        const double growth = std::exp(exponent);
        require_finite(growth, "GBM path growth is not finite");
        const double next_value = values[step - 1U] * growth;
        require_finite(next_value, "GBM path value is not finite");
        values[step] = next_value;
    }
    return values;
}

}  // namespace

dataframe latin_hypercube_sampling(
    const std::size_t sample_count,
    const std::size_t dimension_count,
    const std::optional<std::uint64_t> seed) {
    try {
        validate_latin_hypercube_shape(sample_count, dimension_count);
        const double denominator = static_cast<double>(sample_count);
        if (sample_count != 0U && dimension_count != 0U &&
            (!std::isfinite(denominator) || denominator <= 0.0)) {
            throw input_error{"Latin-hypercube denominator is not finite"};
        }

        dataframe result;
        result.load_index(make_ordinal_index(sample_count));
        if (sample_count == 0U) {
            for (std::size_t dimension = 0U; dimension < dimension_count;
                 ++dimension) {
                const std::string name = latin_hypercube_dimension_name(dimension);
                result.load_column<double>(name.c_str(), std::vector<double>{},
                                           hmdf::nan_policy::dont_pad_with_nans);
            }
            return result;
        }
        if (dimension_count == 0U) {
            return result;
        }

        auto engine = make_engine(seed);
        for (std::size_t dimension = 0U; dimension < dimension_count;
             ++dimension) {
            const std::vector<std::size_t> permutation =
                latin_hypercube_permutation(sample_count, engine);
            std::vector<double> values;
            values.reserve(sample_count);
            for (const std::size_t stratum : permutation) {
                const double numerator = static_cast<double>(stratum) +
                                         open_unit_uniform(engine);
                const double value = numerator / denominator;
                if (!std::isfinite(value) || value < 0.0 || value >= 1.0) {
                    throw input_error{"Latin-hypercube sample is outside [0, 1)"};
                }
                values.push_back(value);
            }

            const std::string name = latin_hypercube_dimension_name(dimension);
            result.load_column<double>(name.c_str(), std::move(values),
                                       hmdf::nan_policy::dont_pad_with_nans);
        }
        return result;
    } catch (const std::bad_alloc&) {
        throw input_error{"Latin-hypercube output allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"Latin-hypercube output allocation is not representable"};
    }
}

dataframe geometric_brownian_motion(
    const double initial_value,
    const double annualized_drift,
    const double annualized_volatility,
    const double total_years,
    const double time_step,
    const std::size_t path_count,
    const std::optional<std::uint64_t> seed) {
    validate_model_inputs(initial_value, annualized_drift, annualized_volatility,
                          total_years, time_step, path_count);
    const output_shape shape =
        validate_output_shape(checked_step_count(total_years, time_step), path_count);

    try {
        dataframe result;
        result.load_index(make_ordinal_index(shape.row_count));

        if (shape.step_count == 0U) {
            for (std::size_t path = 0U; path < path_count; ++path) {
                result.load_column<double>(path_name(path).c_str(), {initial_value},
                                           hmdf::nan_policy::dont_pad_with_nans);
            }
            return result;
        }

        const propagation_terms terms = checked_propagation_terms(
            annualized_drift, annualized_volatility, time_step);
        auto engine = make_engine(seed);
        for (std::size_t path = 0U; path < path_count; ++path) {
            result.load_column<double>(path_name(path).c_str(),
                                       generate_path(initial_value, shape, terms, engine),
                                       hmdf::nan_policy::dont_pad_with_nans);
        }
        return result;
    } catch (const std::bad_alloc&) {
        throw input_error{"GBM output allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"GBM output allocation is not representable"};
    }
}

dataframe gbm_from_returns(
    const dataframe& returns,
    const std::size_t horizon,
    const std::size_t path_count,
    const std::size_t periods_per_year,
    const std::optional<std::uint64_t> seed,
    const std::string_view column) {
    try {
        const std::string column_name = checked_return_column_name(column);
        const simulation_parameters parameters = estimate_simulation_parameters(
            returns, periods_per_year, column_name);

        const double periods_as_double = static_cast<double>(periods_per_year);
        require_finite(periods_as_double,
                       "GBM returns periods per year is not finite");
        const double square_root_periods = std::sqrt(periods_as_double);
        require_finite(square_root_periods,
                       "GBM returns periods-per-year square root is not finite");
        const double daily_drift = parameters.drift / periods_as_double;
        require_finite(daily_drift, "GBM returns daily drift is not finite");
        const double daily_volatility = parameters.volatility / square_root_periods;
        require_finite(daily_volatility,
                       "GBM returns daily volatility is not finite");
        const double horizon_as_double = static_cast<double>(horizon);
        require_finite(horizon_as_double, "GBM returns horizon is not finite");
        const double duration = horizon_as_double / periods_as_double;
        require_finite(duration, "GBM returns duration is not finite");
        const double step = 1.0 / periods_as_double;
        require_finite(step, "GBM returns time step is not finite");

        const dataframe prices = geometric_brownian_motion(
            1.0, daily_drift, daily_volatility, duration, step, path_count, seed);
        const auto& price_index = prices.get_index();
        if (price_index.empty()) {
            throw input_error{"GBM returns price paths must include an initial row"};
        }

        const std::size_t retained_count = price_index.size() - 1U;
        std::vector<timestamp> index;
        index.reserve(retained_count);
        for (std::size_t row = 1U; row < price_index.size(); ++row) {
            index.push_back(price_index[row]);
        }

        dataframe result;
        result.load_index(std::move(index));
        for (std::size_t path = 0U; path < path_count; ++path) {
            const std::string name = path_name(path);
            const auto& prices_for_path = prices.get_column<double>(name.c_str());
            if (prices_for_path.size() != price_index.size()) {
                throw input_error{"GBM returns price path length is invalid"};
            }

            std::vector<double> values;
            values.reserve(retained_count);
            for (std::size_t row = 1U; row < prices_for_path.size(); ++row) {
                const double cumulative_return = prices_for_path[row] - 1.0;
                require_finite(cumulative_return,
                               "GBM returns cumulative return is not finite");
                values.push_back(cumulative_return);
            }
            result.load_column<double>(name.c_str(), std::move(values),
                                       hmdf::nan_policy::dont_pad_with_nans);
        }
        return result;
    } catch (const std::bad_alloc&) {
        throw input_error{"GBM returns output allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"GBM returns output allocation is not representable"};
    }
}

}  // namespace fincore
