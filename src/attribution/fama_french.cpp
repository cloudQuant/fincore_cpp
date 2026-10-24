#include <fincore/attribution/fama_french.hpp>

#include <fincore/attribution/factor_regression.hpp>
#include <fincore/error.hpp>
#include <fincore/errors.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <vector>

namespace fincore {
namespace {

std::vector<std::string> double_columns(const dataframe& frame) {
    std::vector<std::string> names;
    for (const auto& info : frame.get_columns_info<double>()) {
        if (std::get<2>(info) == typeid(double)) {
            names.emplace_back(std::get<0>(info).c_str());
        }
    }
    return names;
}

std::vector<double> solve(std::vector<std::vector<double>> matrix,
                          std::vector<double> rhs) {
    const std::size_t size = rhs.size();
    for (std::size_t column = 0; column < size; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1U; row < size; ++row) {
            if (std::abs(matrix[row][column]) >
                std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (std::abs(matrix[pivot][column]) < 1e-14) {
            throw input_error{"market beta regression is singular"};
        }
        std::swap(matrix[column], matrix[pivot]);
        std::swap(rhs[column], rhs[pivot]);
        const double divisor = matrix[column][column];
        for (std::size_t entry = column; entry < size; ++entry) {
            matrix[column][entry] /= divisor;
        }
        rhs[column] /= divisor;
        for (std::size_t row = 0; row < size; ++row) {
            if (row == column) {
                continue;
            }
            const double factor = matrix[row][column];
            if (factor == 0.0) {
                continue;
            }
            for (std::size_t entry = column; entry < size; ++entry) {
                matrix[row][entry] -= factor * matrix[column][entry];
            }
            rhs[row] -= factor * rhs[column];
        }
    }
    return rhs;
}

double population_variance(const std::vector<double>& values,
                           const double shift) {
    if (values.empty()) {
        return std::nan("");
    }
    double mean = 0.0;
    for (const double value : values) {
        mean += value - shift;
    }
    mean /= static_cast<double>(values.size());
    double variance = 0.0;
    for (const double value : values) {
        const double delta = (value - shift) - mean;
        variance += delta * delta;
    }
    return variance / static_cast<double>(values.size());
}

double fit_market_beta(const dataframe& returns, const dataframe& factors,
                       const std::string& first_asset,
                       const double risk_free) {
    static const std::vector<std::string> factor_names{"MKT", "SMB", "HML",
                                                       "RMW", "CMA"};
    const auto factor_columns = double_columns(factors);
    for (const auto& name : factor_names) {
        if (std::find(factor_columns.begin(), factor_columns.end(), name) ==
            factor_columns.end()) {
            throw input_error{
                "factor_data must contain the MKT, SMB, HML, RMW and CMA "
                "columns to fit the market beta"};
        }
    }
    const auto& y_values = returns.get_column<double>(first_asset.c_str());
    const std::size_t rows = y_values.size();
    if (rows < factor_names.size() + 1U) {
        throw input_error{"too few rows to fit the market beta"};
    }
    std::vector<std::vector<double>> design(
        rows, std::vector<double>(factor_names.size() + 1U, 1.0));
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < factor_names.size(); ++column) {
            const auto& values =
                factors.get_column<double>(factor_names[column].c_str());
            design[row][column + 1U] = values[row];
        }
    }
    const std::size_t width = design.front().size();
    std::vector<std::vector<double>> normal(width,
                                           std::vector<double>(width, 0.0));
    std::vector<double> rhs(width, 0.0);
    for (std::size_t row = 0; row < rows; ++row) {
        const double target = y_values[row] - risk_free;
        for (std::size_t left = 0; left < width; ++left) {
            rhs[left] += design[row][left] * target;
            for (std::size_t right = 0; right < width; ++right) {
                normal[left][right] +=
                    design[row][left] * design[row][right];
            }
        }
    }
    return solve(std::move(normal), std::move(rhs))[1U];
}

}  // namespace

idiosyncratic_risk_result calculate_idiosyncratic_risk(
    const dataframe& returns, const dataframe& factor_data,
    const std::optional<double> market_beta, const double risk_free) {
    if (!std::isfinite(risk_free)) {
        throw input_error{"risk_free must be finite"};
    }
    if (returns.get_index().size() != factor_data.get_index().size()) {
        throw input_error{
            "returns and factor_data must contain the same number of rows"};
    }
    if (returns.get_index().empty()) {
        throw input_error{"returns must contain at least one row"};
    }
    const auto assets = double_columns(returns);
    if (assets.empty()) {
        throw input_error{"returns must contain at least one asset column"};
    }
    const auto factor_columns = double_columns(factor_data);
    if (std::find(factor_columns.begin(), factor_columns.end(), "MKT") ==
        factor_columns.end()) {
        throw input_error{"factor_data must contain a double MKT column"};
    }
    const double beta = market_beta.has_value()
                            ? *market_beta
                            : fit_market_beta(returns, factor_data,
                                              assets.front(), risk_free);
    if (!std::isfinite(beta)) {
        throw input_error{"market beta must be finite"};
    }
    const auto& market =
        factor_data.get_column<double>("MKT");
    const double market_variance =
        population_variance(std::vector<double>(market.begin(),
                                                market.end()),
                            risk_free);
    idiosyncratic_risk_result result;
    result.market_beta = beta;
    result.assets = assets;
    for (const auto& asset : assets) {
        const auto& values = returns.get_column<double>(asset.c_str());
        const double total_variance = population_variance(
            std::vector<double>(values.begin(), values.end()), risk_free);
        const double idiosyncratic_variance =
            total_variance - beta * beta * market_variance;
        result.specific_volatility.push_back(
            idiosyncratic_variance > 0.0 ? std::sqrt(idiosyncratic_variance)
                                         : 0.0);
    }
    return result;
}

}  // namespace fincore

namespace fincore {
namespace {

void validate_provider_frame(const dataframe& frame,
                             const std::string_view context) {
    if (frame.get_index().empty()) {
        throw fincore_error{validation_error(
            std::string(context) + " must return a non-empty dataframe")};
    }
    const auto columns = frame.get_columns_info<double, int, std::string,
                                                bool>();
    if (columns.empty()) {
        throw fincore_error{validation_error(
            std::string(context) + " must return at least one column")};
    }
}

std::string first_double_column(const dataframe& frame,
                                const std::string_view context) {
    const auto columns = frame.get_columns_info<double>();
    if (columns.empty()) {
        throw fincore_error{validation_error(
            std::string(context) + " must contain a double column")};
    }
    return std::string{std::get<0>(columns.front()).c_str()};
}

dataframe slice_frame(const dataframe& frame, const std::size_t begin,
                      const std::size_t end,
                      const std::span<const std::string_view> columns) {
    const auto& index = frame.get_index();
    dataframe sliced;
    sliced.load_index(
        std::vector<timestamp>{index.begin() + static_cast<std::ptrdiff_t>(begin),
                               index.begin() + static_cast<std::ptrdiff_t>(end)});
    for (const std::string_view name : columns) {
        const auto values = frame.get_column<double>(std::string{name}.c_str());
        sliced.load_column<double>(
            std::string{name}.c_str(),
            std::vector<double>{values.begin() + static_cast<std::ptrdiff_t>(begin),
                                values.begin() + static_cast<std::ptrdiff_t>(end)},
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return sliced;
}

double column_mean(const dataframe& frame, const std::string_view column) {
    const auto values = frame.get_column<double>(std::string{column}.c_str());
    if (values.empty()) {
        throw fincore_error{validation_error("factor data must not be empty")};
    }
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

}  // namespace

dataframe fetch_ff_factors(const std::string_view start,
                           const std::string_view end,
                           const std::string_view library,
                           const fama_french_provider& provider,
                           const bool copy) {
    if (!provider) {
        throw fincore_error{validation_error(
            "Fama-French provider must be callable", "provider")};
    }
    if (start.empty() || end.empty()) {
        throw fincore_error{validation_error(
            "Fama-French start and end dates must not be empty", "start")};
    }
    if (library.empty()) {
        throw fincore_error{validation_error(
            "Fama-French library must not be empty", "library")};
    }
    dataframe frame = provider(start, end, library);
    validate_provider_frame(frame, "Fama-French provider");
    // Native results always own their buffers. ``copy=false`` cannot expose
    // provider-owned storage, so it still yields a detached value copy.
    (void)copy;
    return frame;
}

dataframe fetch_ff_factors(const std::string_view start,
                           const std::string_view end,
                           const fama_french_provider& provider,
                           const std::string_view library, const bool copy) {
    return fetch_ff_factors(start, end, library, provider, copy);
}

FamaFrenchModel::FamaFrenchModel(std::string model_type,
                                 const double risk_free_rate)
    : model_type_(std::move(model_type)), risk_free_rate_(risk_free_rate) {
    if (!std::isfinite(risk_free_rate_)) {
        throw fincore_error{validation_error(
            "risk_free_rate must be finite", "risk_free_rate")};
    }
    if (model_type_ == "3factor") {
        factors_.assign(FF3_FACTORS.begin(), FF3_FACTORS.end());
    } else if (model_type_ == "5factor") {
        factors_.assign(FF5_FACTORS.begin(), FF5_FACTORS.end());
    } else if (model_type_ == "4factor_mom") {
        factors_.assign(FF4MOM_FACTORS.begin(), FF4MOM_FACTORS.end());
    } else {
        throw fincore_error{validation_error(
            "Unknown model_type: " + model_type_ +
                ". Use: '3factor', '5factor', '4factor_mom'",
            "model_type", model_type_)};
    }
}

const std::string& FamaFrenchModel::model_type() const noexcept {
    return model_type_;
}

double FamaFrenchModel::risk_free_rate() const noexcept {
    return risk_free_rate_;
}

std::span<const std::string_view> FamaFrenchModel::factors() const noexcept {
    return factors_;
}

const std::map<std::string, double>& FamaFrenchModel::betas() const noexcept {
    return betas_;
}

std::optional<double> FamaFrenchModel::alpha() const noexcept { return alpha_; }

std::string FamaFrenchModel::resolve_returns_column(
    const dataframe& returns, const std::string_view returns_column) const {
    if (!returns_column.empty()) {
        return std::string{returns_column};
    }
    // Source semantics: a multi-column DataFrame uses its first column as the
    // dependent variable for a single-asset regression.
    return first_double_column(returns, "Fama-French returns");
}

fama_french_fit_result FamaFrenchModel::fit(const dataframe& returns,
                                            const dataframe& factor_data,
                                            const std::string_view returns_column) {
    const std::string column = resolve_returns_column(returns, returns_column);
    const auto regression = fit_factor_regression(returns, factor_data, factors_,
                                                  column);
    const auto& coefficient_values = regression.coefficients.get_column<double>(
        factor_regression_coefficient_column.data());

    fama_french_fit_result result;
    result.alpha = regression.alpha;
    result.r_squared = regression.r_squared;
    result.fitted_returns = regression.fitted_returns;
    result.residuals = regression.residuals;
    for (std::size_t position = 0U; position < factors_.size(); ++position) {
        result.betas.emplace(std::string{factors_[position]},
                             coefficient_values[position + 1U]);
    }
    alpha_ = result.alpha;
    betas_ = result.betas;
    return result;
}

dataframe FamaFrenchModel::predict(const dataframe& factor_data) const {
    if (!alpha_.has_value()) {
        throw fincore_error{make_error(
            "RuntimeError", "fincore.model_not_fit",
            "Model must be fit before prediction")};
    }
    const auto& index = factor_data.get_index();
    std::vector<double> values(index.size(), *alpha_);
    for (const std::string_view factor : factors_) {
        const auto column =
            factor_data.get_column<double>(std::string{factor}.c_str());
        if (column.size() != index.size()) {
            throw fincore_error{validation_error(
                "factor data column length must match the index", "factor_data")};
        }
        const double beta = betas_.at(std::string{factor});
        for (std::size_t row = 0U; row < index.size(); ++row) {
            values[row] += beta * column[row];
        }
    }
    return make_series(std::vector<timestamp>{index.begin(), index.end()},
                       factor_regression_fitted_column, std::move(values));
}

dataframe FamaFrenchModel::get_factor_exposures(
    const dataframe& returns, const dataframe& factor_data,
    const std::optional<std::size_t> rolling_window,
    const std::string_view returns_column) const {
    if (returns.get_index() != factor_data.get_index()) {
        throw fincore_error{validation_error(
            "returns and factor_data must share the same index", "returns")};
    }
    const std::string column = resolve_returns_column(returns, returns_column);
    std::vector<std::string_view> return_columns{column};
    std::vector<double> alpha_values;
    std::vector<std::vector<double>> beta_values(factors_.size());
    const auto& index = returns.get_index();
    alpha_values.reserve(index.size());
    for (auto& values : beta_values) {
        values.reserve(index.size());
    }

    for (std::size_t position = 0U; position < index.size(); ++position) {
        const std::size_t start =
            rolling_window.has_value() && *rolling_window > 0U
                ? (position + 1U > *rolling_window
                       ? position + 1U - *rolling_window
                       : 0U)
                : 0U;
        const std::size_t end = position + 1U;
        if (end > start && end - start > factors_.size()) {
            const auto window_returns =
                slice_frame(returns, start, end, return_columns);
            const auto window_factors =
                slice_frame(factor_data, start, end, factors_);
            const auto regression = fit_factor_regression(
                window_returns, window_factors, factors_, column);
            const auto& coefficients = regression.coefficients.get_column<double>(
                factor_regression_coefficient_column.data());
            alpha_values.push_back(coefficients.front());
            for (std::size_t factor = 0U; factor < factors_.size(); ++factor) {
                beta_values[factor].push_back(coefficients[factor + 1U]);
            }
        } else {
            alpha_values.push_back(std::nan(""));
            for (auto& values : beta_values) {
                values.push_back(std::nan(""));
            }
        }
    }

    dataframe exposures;
    exposures.load_index(std::vector<timestamp>{index.begin(), index.end()});
    exposures.load_column<double>(factor_regression_alpha_name.data(),
                                  std::move(alpha_values),
                                  hmdf::nan_policy::dont_pad_with_nans);
    for (std::size_t factor = 0U; factor < factors_.size(); ++factor) {
        exposures.load_column<double>(std::string{factors_[factor]}.c_str(),
                                      std::move(beta_values[factor]),
                                      hmdf::nan_policy::dont_pad_with_nans);
    }
    return exposures;
}

std::map<std::string, double> FamaFrenchModel::attribution_decomposition(
    const dataframe& returns, const dataframe& factor_data,
    const std::string_view returns_column) {
    const std::string column = resolve_returns_column(returns, returns_column);
    const auto regression = fit(returns, factor_data, column);
    const double total_return = column_mean(returns, column) - risk_free_rate_;

    std::map<std::string, double> result;
    result.emplace("alpha", regression.alpha);
    double specific = 0.0;
    for (const auto& [factor, beta] : regression.betas) {
        const double contribution = beta * column_mean(factor_data, factor);
        specific += contribution;
        result.emplace(factor + "_attribution", contribution);
    }
    result.emplace("specific_return", specific);
    result.emplace("common_return", total_return);
    result.emplace("unexplained", total_return - regression.alpha - specific);
    return result;
}

}  // namespace fincore
