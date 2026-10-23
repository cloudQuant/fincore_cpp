#include <fincore/attribution/fama_french.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <typeindex>
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
