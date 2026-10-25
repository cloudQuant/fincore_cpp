#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace fincore {

/// Tail of the return distribution used by an EVT model.
enum class evt_tail {
    lower,  ///< Lower tail (losses).
    upper   ///< Upper tail (gains).
};

/// Estimation method for a generalized Pareto fit.
enum class gpd_estimation_method {
    mle,  ///< Maximum-likelihood fit of the GPD.
    pwm   ///< Probability-weighted-moment fit using L-moments.
};

/// Extreme-value model used for a tail estimate.
enum class evt_model {
    gpd,  ///< Peaks-over-threshold generalized Pareto model.
    gev   ///< Block-maxima generalized extreme-value model.
};

/// Hill tail-index estimate and the exceedances used.
struct hill_estimate {
    double xi;  ///< Hill tail-index estimate: mean log exceedance ratio.
    std::vector<double> tail_observations;  ///< Strict exceedances in
                                            ///< magnitude units, in input
                                            ///< order.
};

/// Fitted generalized Pareto distribution for threshold exceedances.
struct gpd_parameters {
    double xi;         ///< Shape parameter.
    double beta;       ///< Scale parameter in magnitude units.
    double threshold;  ///< Positive magnitude threshold used for the fit.
    std::size_t n_exceed;  ///< Number of strict exceedances used.
};

/// Fitted generalized extreme-value distribution of block extremes.
struct gev_parameters {
    double xi;     ///< Shape parameter.
    double mu;     ///< Location parameter in tail-magnitude units.
    double sigma;  ///< Scale parameter in tail-magnitude units.
    std::size_t n_blocks;  ///< Number of complete blocks used.
};

/// Estimates the Hill tail index from the selected tail magnitudes.
///
/// Magnitudes are reflected losses for `lower` and gains for `upper`; the
/// default threshold is the 90th linear percentile of those magnitudes. Only
/// strict exceedances are used, and at least ten are required.
///
/// @param data Frame containing the return series.
/// @param threshold Positive magnitude threshold; empty selects the 90th
///     percentile of the tail magnitudes.
/// @param tail Tail to estimate.
/// @param column Column holding decimal simple returns.
/// @return Hill tail index and the strict exceedances used.
/// @throws input_error If the threshold is not finite and positive, fewer than
///     ten exceedances remain, or the column is missing or contains an
///     embedded NUL.
hill_estimate hill_estimator(const dataframe& data,
                             std::optional<double> threshold = std::nullopt,
                             evt_tail tail = evt_tail::upper,
                             std::string_view column = returns_column);

/// Fits a generalized Pareto distribution to tail exceedances.
///
/// Magnitudes are reflected losses for `lower` and gains for `upper`; the
/// default threshold is the 90th linear percentile of those magnitudes. At
/// least ten strict exceedances are required. The maximum-likelihood path
/// reproduces the pinned source objective and bounds, which can drive `xi` to
/// its upper bound. The PWM path uses L-moment formulas.
///
/// @param data Frame containing the return series.
/// @param threshold Positive magnitude threshold; empty selects the 90th
///     percentile of the tail magnitudes.
/// @param method Estimation method.
/// @param tail Tail to fit.
/// @param column Column holding decimal simple returns.
/// @return Fitted shape, scale, threshold and exceedance count.
/// @throws input_error If the selected tail has no observations, the threshold
///     is not finite and positive, fewer than ten exceedances remain, a PWM
///     moment is invalid, or the column is missing or contains an embedded
///     NUL.
gpd_parameters gpd_fit(const dataframe& data,
                       std::optional<double> threshold = std::nullopt,
                       gpd_estimation_method method = gpd_estimation_method::mle,
                       evt_tail tail = evt_tail::lower,
                       std::string_view column = returns_column);

/// Fits a generalized extreme-value distribution to block extremes.
///
/// NaN values are dropped; each complete block contributes its maximum for
/// `upper` or the negated minimum for `lower`, so the fitted location and scale
/// are in tail-magnitude units. The default block size is the integer square
/// root of the remaining observation count.
///
/// @param data Frame containing the return series.
/// @param block_size Positive number of consecutive observations per block;
///     empty selects `floor(sqrt(n))`.
/// @param tail Tail whose block extremes are fitted.
/// @param column Column holding decimal simple returns.
/// @return Fitted shape, location, scale and block count.
/// @throws input_error If no observation remains, `block_size` is zero or
///     leaves no complete block, the block extremes have no positive finite
///     variation, the fit does not reach a finite likelihood, a fitted
///     parameter is non-finite or the scale is not positive, or the column is
///     missing or contains an embedded NUL.
gev_parameters gev_fit(const dataframe& data,
                       std::optional<std::size_t> block_size = std::nullopt,
                       evt_tail tail = evt_tail::lower,
                       std::string_view column = returns_column);

/// Estimates the value at risk with an extreme-value model.
///
/// `alpha` is the tail probability, not the confidence level: for example
/// `0.05` targets the 95% tail. The GPD path fits a peaks-over-threshold
/// model; when `threshold` is empty, a threshold is chosen to leave enough
/// strict exceedances to cover `alpha`, starting from the 90th percentile of
/// the selected tail. The GEV path uses block maxima and ignores `threshold`.
/// The result is signed by `tail`, so lower-tail estimates are negative.
///
/// @param data Frame containing the return series.
/// @param alpha Tail probability in (0, 1).
/// @param model EVT model.
/// @param tail Tail to estimate.
/// @param threshold Positive GPD magnitude threshold; empty selects the
///     automatic threshold.
/// @param block_size GEV block size in observations; empty selects the
///     automatic block size.
/// @param column Column holding decimal simple returns.
/// @return Signed VaR estimate.
/// @throws input_error If `alpha` is not finite in (0, 1), a GPD threshold
///     leaves an exceedance probability below `alpha`, a fit or VaR is not
///     finite, or the column is missing or contains an embedded NUL.
double evt_var(const dataframe& data, double alpha = 0.05,
               evt_model model = evt_model::gpd,
               evt_tail tail = evt_tail::lower,
               std::optional<double> threshold = std::nullopt,
               std::optional<std::size_t> block_size = std::nullopt,
               std::string_view column = returns_column);

/// Estimates the conditional tail expectation with an extreme-value model.
///
/// Uses the same fitting and threshold policy as `evt_var`, then reports the
/// mean of the tail beyond VaR (CVaR/ES). The estimate is signed by `tail` and
/// lies further into the selected tail than the corresponding VaR.
///
/// @param data Frame containing the return series.
/// @param alpha Tail probability in (0, 1).
/// @param model EVT model.
/// @param tail Tail to estimate.
/// @param threshold Positive GPD magnitude threshold; empty selects the
///     automatic threshold.
/// @param block_size GEV block size in observations; empty selects the
///     automatic block size.
/// @param column Column holding decimal simple returns.
/// @return Signed CVaR estimate.
/// @throws input_error If `alpha` is not finite in (0, 1), a GPD threshold
///     leaves an exceedance probability below `alpha`, a fit is not finite,
///     `xi` is at least 1 (infinite CVaR), or the column is missing or
///     contains an embedded NUL.
double evt_cvar(const dataframe& data, double alpha = 0.05,
                evt_model model = evt_model::gpd,
                evt_tail tail = evt_tail::lower,
                std::optional<double> threshold = std::nullopt,
                std::optional<std::size_t> block_size = std::nullopt,
                std::string_view column = returns_column);

/// Combined value-at-risk and expected-shortfall estimate from an EVT model.
struct extreme_risk_result {
    double var;   ///< Value-at-risk estimate, signed by the tail.
    double cvar;  ///< Conditional tail expectation, signed by the tail.
    double tail_index;  ///< Fitted GPD or GEV shape parameter.
    double threshold;   ///< GPD magnitude threshold; NaN for GEV fits.
    std::size_t n_exceedances;  ///< GPD exceedance count; zero for GEV fits.
    double alpha;               ///< Tail probability used for the estimates.
    std::optional<double> location;  ///< GEV location; empty for GPD fits.
    std::optional<double> scale;     ///< GEV scale; empty for GPD fits.
    std::optional<std::size_t> n_blocks;  ///< GEV block count; empty for GPD.
};

/// Estimates VaR, CVaR and fitted tail parameters with an extreme-value model.
///
/// Runs the same fit and threshold policy as `evt_var` and `evt_cvar` and
/// returns both estimates. For GPD, `threshold` and `n_exceedances` describe
/// the fit and the location, scale and block fields are empty. For GEV, the
/// location, scale and block fields are set, while `threshold` is NaN and
/// `n_exceedances` is zero.
///
/// @param returns Frame containing the return series.
/// @param alpha Tail probability in (0, 1).
/// @param tail Tail to estimate.
/// @param model EVT model.
/// @param threshold Positive GPD magnitude threshold; ignored for GEV.
/// @param block_size GEV block size in observations; empty selects the
///     automatic block size.
/// @param column Column holding decimal simple returns.
/// @return Signed VaR and CVaR with fitted tail parameters.
/// @throws input_error If `alpha` is not finite in (0, 1), a fit is not
///     finite, `xi` is at least 1 (infinite CVaR), or the column is missing or
///     contains an embedded NUL.
extreme_risk_result extreme_risk(const dataframe& returns, double alpha = 0.05,
                                 evt_tail tail = evt_tail::lower,
                                 evt_model model = evt_model::gpd,
                                 std::optional<double> threshold = std::nullopt,
                                 std::optional<std::size_t> block_size = std::nullopt,
                                 std::string_view column = returns_column);

}  // namespace fincore
