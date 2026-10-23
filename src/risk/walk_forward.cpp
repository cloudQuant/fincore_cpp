#include <fincore/risk/walk_forward.hpp>

#include <fincore/detail/digest.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

double normal_ppf(const double probability) {
    constexpr double low = 0.02425;
    const double high = 1.0 - low;
    double result = 0.0;
    if (probability < low) {
        const double q = std::sqrt(-2.0 * std::log(probability));
        result = (((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                      2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                    4.374664141464968e+00) * q + 2.938163982698783e+00) /
                 ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                    2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    } else if (probability <= high) {
        const double q = probability - 0.5;
        const double r = q * q;
        result = (((((-3.969683028665376e+01 * r + 2.209460984245205e+02) * r -
                      2.759285104469687e+02) * r + 1.383577518672690e+02) * r -
                    3.066479806614716e+01) * r + 2.506628277459239e+00) * q /
                 (((((-5.447609879822406e+01 * r + 1.615858368580409e+02) * r -
                      1.556989798598866e+02) * r + 6.680131188771972e+01) * r -
                    1.328068155288572e+01) * r + 1.0);
    } else {
        const double q = std::sqrt(-2.0 * std::log(1.0 - probability));
        result = -(((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                       2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                     4.374664141464968e+00) * q + 2.938163982698783e+00) /
                  ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                     2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    }
    const double error =
        0.5 * std::erfc(-result / std::sqrt(2.0)) - probability;
    const double correction =
        error * std::sqrt(2.0 * 3.14159265358979323846) *
        std::exp(result * result / 2.0);
    return result - correction / (1.0 + result * correction / 2.0);
}

double weibull_quantile(std::vector<double> values, const double probability) {
    std::sort(values.begin(), values.end());
    const std::size_t count = values.size();
    const double rank =
        (static_cast<double>(count) + 1.0) * probability;
    const auto floor_rank = static_cast<long long>(std::floor(rank));
    const double fraction = rank - static_cast<double>(floor_rank);
    if (floor_rank <= 0) {
        return values.front();
    }
    if (static_cast<std::size_t>(floor_rank) >= count) {
        return values.back();
    }
    const double lower = values[static_cast<std::size_t>(floor_rank) - 1U];
    const double upper = values[static_cast<std::size_t>(floor_rank)];
    return lower + fraction * (upper - lower);
}

double mean_of(const std::vector<double>& values) {
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

double sample_std_of(const std::vector<double>& values) {
    const double mean = mean_of(values);
    double total = 0.0;
    for (const double value : values) {
        total += (value - mean) * (value - mean);
    }
    return std::sqrt(total / static_cast<double>(values.size() - 1U));
}

std::string digest_of(const std::vector<double>& values,
                      const risk_model_spec& spec) {
    std::vector<std::uint8_t> payload;
    payload.reserve(values.size() * 8U + 64U);
    const auto append_double = [&payload](const double value) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(
                static_cast<std::uint8_t>((bits >> shift) & 0xffU));
        }
    };
    const auto append_text = [&payload](const std::string& text) {
        for (const char character : text) {
            payload.push_back(static_cast<std::uint8_t>(character));
        }
        payload.push_back(0U);
    };
    append_double(spec.confidence_level);
    append_double(static_cast<double>(spec.window));
    append_double(static_cast<double>(spec.refit_cadence));
    append_text(spec.distribution);
    append_text(spec.model_version);
    for (const double value : values) {
        append_double(value);
    }
    return detail::sha256_hex(payload);
}

}  // namespace

walk_forward_var_result walk_forward_var(const dataframe& returns,
                                         const risk_model_spec& spec,
                                         const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    validate_risk_model_spec(spec);
    const auto& index = returns.get_index();
    for (std::size_t position = 1; position < index.size(); ++position) {
        if (!(index[position] > index[position - 1U])) {
            throw input_error{"returns index must be sorted in ascending order"};
        }
    }
    const auto values = require_double_column(returns, column);
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw input_error{"returns must contain only finite values"};
        }
    }

    walk_forward_var_result result{};
    result.spec = spec;
    result.n_observations = values.size();
    result.inputs_digest = digest_of(
        std::vector<double>(values.begin(), values.end()), spec);

    std::string unsupported;
    if (spec.forecast_target != risk_forecast_target::var) {
        unsupported = "walk_forward_var currently supports forecast_target='var' only";
    } else if (spec.tail != risk_tail::lower) {
        unsupported = "walk_forward_var currently supports tail='lower' only";
    } else if (spec.sign_convention != "losses_negative") {
        unsupported = "walk_forward_var currently supports sign_convention='losses_negative' only";
    } else if (spec.horizon != 1U) {
        unsupported = "walk_forward_var currently supports horizon=1 only";
    } else if (spec.distribution == "historical") {
        const double alpha = 1.0 - spec.confidence_level;
        const double rank = (static_cast<double>(spec.window) + 1.0) * alpha;
        const double lower_tolerance =
            8.0 * std::numeric_limits<double>::epsilon();
        const double upper_rank = static_cast<double>(spec.window);
        const double upper_tolerance =
            8.0 * std::numeric_limits<double>::epsilon() * upper_rank;
        if (rank < 1.0 - lower_tolerance || rank > upper_rank + upper_tolerance) {
            unsupported =
                "historical Weibull quantile requires 1 / (window + 1) <= 1 - confidence_level <= window / (window + 1)";
        }
    }
    if (!unsupported.empty()) {
        result.status = walk_forward_status::unsupported;
        result.unsupported_reason = unsupported;
        result.has_backtest = false;
        return result;
    }
    if (values.size() <= spec.window) {
        result.status = walk_forward_status::insufficient_data;
        result.unsupported_reason =
            "at least one realized observation after the estimation window is required";
        result.has_backtest = false;
        return result;
    }

    const double alpha = 1.0 - spec.confidence_level;
    double current_forecast = nan;
    for (std::size_t position = spec.window; position < values.size();
         ++position) {
        const bool is_refit =
            !std::isfinite(current_forecast) ||
            (position - spec.window) % spec.refit_cadence == 0U;
        if (is_refit) {
            std::vector<double> window_values(
                values.begin() + static_cast<std::ptrdiff_t>(position - spec.window),
                values.begin() + static_cast<std::ptrdiff_t>(position));
            double forecast_value = nan;
            double mean = nan;
            double standard_deviation = nan;
            if (spec.distribution == "historical") {
                forecast_value = weibull_quantile(window_values, alpha);
                mean = mean_of(window_values);
                standard_deviation = sample_std_of(window_values);
            } else {
                mean = mean_of(window_values);
                standard_deviation = sample_std_of(window_values);
                forecast_value =
                    standard_deviation == 0.0
                        ? mean
                        : mean + standard_deviation * normal_ppf(alpha);
            }
            current_forecast = forecast_value;
            result.refit_timestamps.push_back(index[position]);
            result.fits.push_back({index[position], forecast_value, mean,
                                   standard_deviation, window_values.size()});
        }
        result.index.push_back(index[position]);
        result.forecast.push_back(current_forecast);
        result.realized.push_back(values[position]);
    }

    auto forecast_frame =
        make_series(result.index, "var", result.forecast);
    auto realized_frame =
        make_series(result.index, "realized", result.realized);
    result.backtest = backtest_var(forecast_frame, realized_frame,
                                   spec.confidence_level, 0.05, "var",
                                   "realized");
    result.status = walk_forward_status::ok;
    result.has_backtest = true;
    return result;
}

std::string_view walk_forward_status_name(const walk_forward_status status) {
    switch (status) {
        case walk_forward_status::ok:
            return "ok";
        case walk_forward_status::insufficient_data:
            return "insufficient_data";
        case walk_forward_status::unsupported:
            return "unsupported";
    }
    return "unsupported";
}

}  // namespace fincore
