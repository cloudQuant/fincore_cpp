#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace fincore {

/// Parameters of a GARCH(1,1) model.
struct garch11_parameters {
    double mu;     ///< Constant mean; zero when the fit uses a zero mean.
    double omega;  ///< Variance intercept in return-variance units; positive.
    double alpha;  ///< ARCH coefficient on the previous squared shock.
    double beta;   ///< GARCH coefficient on the previous variance.
};

/// Result of a GARCH(1,1) fit.
struct garch11_result {
    garch11_parameters params;  ///< Fitted parameters.
    std::vector<double> conditional_variances;  ///< Variance path in return
                                                ///< units squared, one value
                                                ///< per cleaned observation.
    std::vector<double> residuals;  ///< Standardized residuals aligned with
                                    ///< `conditional_variances`.
    double log_likelihood;  ///< Gaussian log-likelihood, excluding the first
                            ///< observation.
    bool converged;  ///< Whether the optimizer and parameter, variance and
                     ///< stationarity checks passed.
};

/// Returns the Gaussian GARCH(1,1) log-likelihood for fixed parameters.
///
/// The first conditional variance is the population variance of `eps` and the
/// likelihood sums over the remaining observations, so the first observation
/// is a one-step burn-in.
///
/// @param eps Zero-mean return shocks.
/// @param omega Variance intercept.
/// @param alpha ARCH coefficient.
/// @param beta GARCH coefficient.
/// @return Log-likelihood; NaN when fewer than two shocks are supplied, or
///     negative infinity when a conditional variance is not positive and
///     finite.
double garch11_log_likelihood(std::span<const double> eps,
                              double omega,
                              double alpha,
                              double beta);

/// Fits a GARCH(1,1) model by Gaussian maximum likelihood.
///
/// NaN observations are dropped; at least eleven non-NaN observations with
/// finite values and positive variance are required. A zero mean is imposed
/// unless `constant_mean` is true.
///
/// @param returns Frame containing decimal simple returns.
/// @param constant_mean Estimate a constant mean instead of fixing it at zero.
/// @param column Column holding the return series.
/// @return Fitted parameters, conditional variances, residuals, likelihood and
///     convergence flag. A failed convergence is reported through `converged`,
///     not by throwing.
/// @throws input_error If fewer than eleven non-NaN observations remain, a
///     non-finite value remains, the series has zero variance, or the column
///     is missing or contains an embedded NUL.
garch11_result fit_garch11(const dataframe& returns,
                           bool constant_mean = false,
                           std::string_view column = returns_column);

/// Forecasts the conditional variance path of a GARCH(1,1) fit.
///
/// The first step uses the last conditional variance and residual; later steps
/// use the fitted persistence `alpha + beta`. A result without conditional
/// variances yields a path of NaNs.
///
/// @param result Fitted GARCH(1,1) result.
/// @param horizon Number of variance steps; must be at least 1.
/// @return Variance forecasts in return units squared, one per step.
/// @throws input_error If `horizon` is zero.
std::vector<double> forecast_garch11(const garch11_result& result,
                                     std::size_t horizon = 1U);

/// Parameters of an EGARCH(1,1) model.
struct egarch_parameters {
    double omega;  ///< Log-variance intercept.
    double alpha;  ///< Magnitude coefficient on the absolute standardized
                   ///< shock.
    double gamma;  ///< Asymmetry (leverage) coefficient on the standardized
                   ///< shock.
    double beta;   ///< Persistence coefficient on the previous log-variance.
};

/// Result of an EGARCH(1,1) fit.
struct egarch_result {
    egarch_parameters params;  ///< Fitted parameters.
    std::vector<double> conditional_variances;  ///< Variance path in return
                                                ///< units squared, one value
                                                ///< per cleaned observation.
    std::vector<double> residuals;  ///< Standardized residuals aligned with
                                    ///< `conditional_variances`.
    double log_likelihood;  ///< Gaussian log-likelihood, excluding the first
                            ///< observation.
    bool converged;  ///< Whether the optimizer and parameter, variance and
                     ///< stationarity checks passed.
};

/// Parameters of a GJR-GARCH(1,1) model.
struct gjr_garch_parameters {
    double omega;  ///< Variance intercept in return-variance units; positive.
    double alpha;  ///< ARCH coefficient on the previous squared shock.
    double gamma;  ///< Additional coefficient on squared negative shocks.
    double beta;   ///< GARCH coefficient on the previous variance.
};

/// Result of a GJR-GARCH(1,1) fit.
struct gjr_garch_result {
    gjr_garch_parameters params;  ///< Fitted parameters.
    std::vector<double> conditional_variances;  ///< Variance path in return
                                                ///< units squared, one value
                                                ///< per cleaned observation.
    std::vector<double> residuals;  ///< Standardized residuals aligned with
                                    ///< `conditional_variances`.
    double log_likelihood;  ///< Gaussian log-likelihood, excluding the first
                            ///< observation.
    bool converged;  ///< Whether the optimizer and parameter, variance and
                     ///< stationarity checks passed.
};

/// GARCH-family model selected by the convenience functions.
enum class garch_model {
    garch,      ///< GARCH(1,1) with a zero mean.
    egarch,     ///< EGARCH(1,1).
    gjr_garch   ///< GJR-GARCH(1,1) with an asymmetric shock term.
};

/// Fits an EGARCH(1,1) model by Gaussian maximum likelihood.
///
/// NaN observations are dropped; at least ten non-NaN observations with finite
/// values and positive variance are required. The fit uses multi-start
/// Nelder-Mead optimization.
///
/// @param returns Frame containing decimal simple returns.
/// @param column Column holding the return series.
/// @return Fitted parameters, conditional variances, residuals, likelihood and
///     convergence flag.
/// @throws input_error If fewer than ten non-NaN observations remain, a
///     non-finite value remains, the series has zero variance, or the column
///     is missing or contains an embedded NUL.
egarch_result fit_egarch(const dataframe& returns,
                         std::string_view column = returns_column);

/// Forecasts the conditional variance path of an EGARCH(1,1) fit.
///
/// The first step uses the last log-variance and standardized residual; later
/// steps use a constant drift and the fitted `beta` persistence. A result
/// without conditional variances yields a path of NaNs.
///
/// @param result Fitted EGARCH(1,1) result.
/// @param horizon Number of variance steps; must be at least 1.
/// @return Variance forecasts in return units squared, one per step.
/// @throws input_error If `horizon` is zero.
std::vector<double> forecast_egarch(const egarch_result& result,
                                    std::size_t horizon = 1U);

/// Fits a GJR-GARCH(1,1) model by Gaussian maximum likelihood.
///
/// NaN observations are dropped; at least ten non-NaN observations with finite
/// values and positive variance are required. The fit uses multi-start
/// Nelder-Mead optimization.
///
/// @param returns Frame containing decimal simple returns.
/// @param column Column holding the return series.
/// @return Fitted parameters, conditional variances, residuals, likelihood and
///     convergence flag.
/// @throws input_error If fewer than ten non-NaN observations remain, a
///     non-finite value remains, the series has zero variance, or the column
///     is missing or contains an embedded NUL.
gjr_garch_result fit_gjr_garch(const dataframe& returns,
                               std::string_view column = returns_column);

/// Forecasts the conditional variance path of a GJR-GARCH(1,1) fit.
///
/// The first step uses the last conditional variance and signed residual;
/// later steps use the fitted persistence `alpha + 0.5 * gamma + beta`. A
/// result without conditional variances yields a path of NaNs.
///
/// @param result Fitted GJR-GARCH(1,1) result.
/// @param horizon Number of variance steps; must be at least 1.
/// @return Variance forecasts in return units squared, one per step.
/// @throws input_error If `horizon` is zero.
std::vector<double> forecast_gjr_garch(const gjr_garch_result& result,
                                       std::size_t horizon = 1U);

/// Forecast and fit diagnostics returned by `forecast_volatility`.
struct garch_forecast_result {
    std::vector<double> forecasts;  ///< Variance forecasts in return units
                                    ///< squared.
    double log_likelihood;  ///< Log-likelihood of the fitted model.
    bool converged;         ///< Convergence flag of the fitted model.
};

/// Fits a GARCH-family model and forecasts its variance path.
///
/// @param returns Frame containing decimal simple returns.
/// @param model GARCH family to fit.
/// @param horizon Number of variance steps; must be at least 1.
/// @param column Column holding the return series.
/// @return Variance forecasts with the fitted likelihood and convergence flag.
/// @throws input_error If `horizon` is zero or the selected fit rejects its
///     input.
garch_forecast_result forecast_volatility(
    const dataframe& returns, garch_model model, std::size_t horizon = 1U,
    std::string_view column = returns_column);

/// GARCH-family VaR or ES estimate and the variance path used.
struct garch_conditional_estimate {
    double value;  ///< Signed risk estimate; negative for losses at lower-tail
                   ///< probabilities.
    std::vector<double> conditional_variances;  ///< Variance forecast path in
                                                ///< return units squared from
                                                ///< `forecast_volatility`, one
                                                ///< value per horizon step.
    bool converged;  ///< Convergence flag of the fit that produced the path.
};

/// Computes a conditional normal VaR from a GARCH-family variance forecast.
///
/// The fit is performed by `forecast_volatility`; the value is
/// `z(alpha) * sqrt(sum(forecast variances))` over the requested horizon.
///
/// @param returns Frame containing decimal simple returns.
/// @param model GARCH family to fit.
/// @param alpha Tail probability in (0, 1); values outside yield NaN.
/// @param horizon Number of variance steps; must be at least 1.
/// @param column Column holding the return series.
/// @return Signed VaR estimate, the variance forecast path and the fit
///     convergence flag.
/// @throws input_error If `horizon` is zero or the selected fit rejects its
///     input.
garch_conditional_estimate conditional_var(
    const dataframe& returns, garch_model model, double alpha = 0.05,
    std::size_t horizon = 1U, std::string_view column = returns_column);

/// Computes a conditional normal expected shortfall from a GARCH-family
/// variance forecast.
///
/// The fit is performed by `forecast_volatility`; the value is
/// `-sqrt(sum(forecast variances)) * pdf(z(alpha)) / alpha` over the requested
/// horizon.
///
/// @param returns Frame containing decimal simple returns.
/// @param model GARCH family to fit.
/// @param alpha Tail probability in (0, 1); values outside yield NaN.
/// @param horizon Number of variance steps; must be at least 1.
/// @param column Column holding the return series.
/// @return Signed expected-shortfall estimate, the variance forecast path and
///     the fit convergence flag.
/// @throws input_error If `horizon` is zero or the selected fit rejects its
///     input.
garch_conditional_estimate conditional_es(
    const dataframe& returns, garch_model model, double alpha = 0.05,
    std::size_t horizon = 1U, std::string_view column = returns_column);

}  // namespace fincore
