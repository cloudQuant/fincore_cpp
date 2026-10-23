#include <fincore/risk/models.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace fincore {
namespace {

struct cleaned_series {
    std::vector<timestamp> index;
    std::vector<double> values;
};

cleaned_series clean_returns(const dataframe& returns,
                             const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto& index = returns.get_index();
    for (std::size_t position = 1U; position < index.size(); ++position) {
        if (index[position] == index[position - 1U]) {
            throw input_error{"returns index must not contain duplicates"};
        }
        if (index[position] < index[position - 1U]) {
            throw input_error{
                "returns index must be sorted in ascending order"};
        }
    }
    const auto values = require_double_column(returns, column);
    cleaned_series cleaned;
    cleaned.index.reserve(index.size());
    cleaned.values.reserve(index.size());
    for (std::size_t position = 0U; position < index.size(); ++position) {
        if (!std::isnan(values[position])) {
            cleaned.index.push_back(index[position]);
            cleaned.values.push_back(values[position]);
        }
    }
    if (cleaned.values.empty()) {
        throw input_error{"returns must contain at least one observation"};
    }
    return cleaned;
}

std::string digest_of(const cleaned_series& series) {
    std::vector<std::uint8_t> payload;
    payload.reserve(series.values.size() * 16U);
    const auto append_u64 = [&payload](const std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(
                static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    };
    for (std::size_t position = 0; position < series.values.size();
         ++position) {
        append_u64(static_cast<std::uint64_t>(series.index[position]));
        std::uint64_t bits = 0;
        std::memcpy(&bits, &series.values[position], sizeof(bits));
        append_u64(bits);
    }
    return detail::sha256_hex(payload);
}

void validate_config(const risk_forecast_config& config) {
    if (!std::isfinite(config.confidence_level) ||
        !(config.confidence_level > 0.0 &&
          config.confidence_level < 1.0)) {
        throw input_error{"confidence_level must be in (0, 1)"};
    }
    if (config.horizon < 1U) {
        throw input_error{"horizon must be at least 1"};
    }
    switch (config.method) {
        case risk_forecast_method::historical:
        case risk_forecast_method::evt:
        case risk_forecast_method::garch:
            return;
    }
    throw input_error{"unknown method"};
}

risk_estimate forecast_impl(const dataframe& returns,
                            const risk_forecast_config& config,
                            const std::string_view column,
                            const bool expected_shortfall) {
    validate_config(config);
    const auto cleaned = clean_returns(returns, column);
    const double alpha = 1.0 - config.confidence_level;

    risk_estimate result;
    result.method = config.method;
    result.confidence_level = config.confidence_level;
    result.horizon = config.horizon;
    result.sign_convention = std::string(losses_negative_sign_convention);
    result.status = risk_estimate_status::ok;
    bool converged = true;

    switch (config.method) {
        case risk_forecast_method::historical: {
            const double threshold =
                detail::linear_percentile(cleaned.values, alpha);
            if (expected_shortfall) {
                double total = 0.0;
                std::size_t count = 0U;
                for (const double value : cleaned.values) {
                    if (value <= threshold) {
                        total += value;
                        ++count;
                    }
                }
                result.estimate =
                    count == 0U ? threshold
                                : total / static_cast<double>(count);
                result.diagnostics["n_tail_observations"] =
                    static_cast<double>(count);
            } else {
                result.estimate = threshold;
                result.diagnostics["n_observations"] =
                    static_cast<double>(cleaned.values.size());
            }
            break;
        }
        case risk_forecast_method::evt: {
            const auto frame = make_series(
                cleaned.index, column, cleaned.values);
            if (expected_shortfall) {
                result.estimate = evt_cvar(
                    frame, alpha, config.evt_kernel, config.tail,
                    config.threshold, config.block_size, column);
                result.kernel = "evt_cvar";
            } else {
                result.estimate = evt_var(
                    frame, alpha, config.evt_kernel, config.tail,
                    config.threshold, config.block_size, column);
                result.kernel = "evt_var";
            }
            result.diagnostics["alpha"] = alpha;
            break;
        }
        case risk_forecast_method::garch: {
            const auto frame = make_series(
                cleaned.index, column, cleaned.values);
            const auto estimate =
                expected_shortfall
                    ? conditional_es(frame, config.garch_kernel, alpha,
                                     config.horizon, column)
                    : conditional_var(frame, config.garch_kernel, alpha,
                                      config.horizon, column);
            result.estimate = estimate.value;
            converged = estimate.converged;
            result.kernel = expected_shortfall ? "conditional_es"
                                               : "conditional_var";
            result.diagnostics["alpha"] = alpha;
            result.diagnostics["horizon"] =
                static_cast<double>(config.horizon);
            if (!converged) {
                result.note =
                    "optimizer did not converge or stationarity checks "
                    "failed";
            }
            break;
        }
    }

    result.forecast_timestamp = cleaned.index.back();
    result.has_forecast_timestamp = true;
    result.inputs_digest = digest_of(cleaned);
    if (cleaned.values.size() < 2U) {
        result.status = risk_estimate_status::insufficient_data;
    } else if (!converged) {
        result.status = risk_estimate_status::failed;
    }
    return result;
}

}  // namespace

risk_estimate forecast_var(const dataframe& returns,
                           const risk_forecast_config& config,
                           const std::string_view column) {
    return forecast_impl(returns, config, column, false);
}

risk_estimate forecast_es(const dataframe& returns,
                          const risk_forecast_config& config,
                          const std::string_view column) {
    return forecast_impl(returns, config, column, true);
}

}  // namespace fincore
