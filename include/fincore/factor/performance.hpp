#pragma once

#include <fincore/factor/data.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace fincore {

/// Spearman rank information coefficient per date and forward-return label.
///
/// `values` is a rectangular cube: one row per entry of `dates` and one column
/// per entry of `labels`; a cell with fewer than two usable observations stays
/// NaN.
struct factor_ic_table {
    std::vector<timestamp> dates; ///< Row dates; repeats per observed group.
    std::vector<std::string> labels; ///< Forward-return labels.
    std::vector<double> values; ///< (date[, group]) rows x labels, NaN missing.
    /// Empty for aggregate output; otherwise parallel to dates, where dates
    /// repeats once per observed (date, group) row in date/group order.
    std::vector<std::string> groups;
};

/// Mean information coefficient per label or per group.
struct factor_ic_summary {
    std::vector<std::string> labels; ///< Forward-return labels.
    std::vector<double> means; ///< Labels or groups x labels.
    /// Empty for aggregate output; otherwise one entry per group row in means.
    std::vector<std::string> groups;
};

/// One asset weight on one date.
struct factor_weight_row {
    timestamp date; ///< Observation date.
    std::string asset; ///< Asset identifier.
    double weight; ///< Gross-normalized weight; NaN for a zero gross bucket.
};

/// Portfolio returns per date and forward-return label.
struct factor_return_table {
    std::vector<timestamp> dates; ///< Sorted unique dates.
    std::vector<std::string> labels; ///< Forward-return labels.
    std::vector<double> values; ///< Dates x labels; zero when no pair is usable.
};

/// Annualized alpha and market beta for one forward-return label.
struct factor_alpha_beta_row {
    std::string label; ///< Forward-return label.
    double annual_alpha; ///< Annualized intercept; NaN when undefined.
    double beta; ///< Slope on the universe mean; NaN when undefined.
};

/// Quantile mean returns and standard errors.
struct quantile_return_table {
    std::vector<int> quantiles; ///< Row keys; repeats when multiple groups exist.
    std::vector<std::string> labels; ///< Forward-return labels.
    std::vector<double> means; ///< (quantile[, group]) rows x labels.
    std::vector<double> standard_errors; ///< (quantile[, group]) rows x labels.
    /// Empty for aggregate output; otherwise parallel to quantiles.
    std::vector<std::string> groups;
};

/// Per-date quantile mean returns and standard errors.
struct quantile_date_return_table {
    std::vector<timestamp> dates; ///< Row keys; repeats for each observed group.
    std::vector<int> quantiles; ///< Quantile keys.
    std::vector<std::string> labels; ///< Forward-return labels.
    std::vector<double> means; ///< (date[, group]) x quantiles x labels.
    std::vector<double> standard_errors; ///< (date[, group]) x quantiles x labels.
    /// Empty for aggregate output; otherwise parallel to dates.
    std::vector<std::string> groups;
};

/// One finite event-window value for an offset, event date and asset.
struct event_window_cell {
    int offset = 0; ///< Row offset from the event date; negative is before.
    timestamp event_date = 0; ///< Event date.
    std::string asset; ///< Asset label; empty for date-mean output.
    double value = 0.0; ///< Window value, compounded or raw as requested.
};

/// Event-aligned window extraction.
struct event_window_result {
    int before = 0; ///< Requested rows before each event date.
    int after = 0; ///< Requested rows after each event date.
    std::vector<int> offsets; ///< Observed offsets, ascending and clipped.
    std::vector<timestamp> event_dates; ///< Event dates found in the ledger.
    std::vector<std::string> asset_labels; ///< Assets seen at event dates, sorted.
    std::vector<event_window_cell> cells; ///< Cells; non-finite ledger values skipped.
};

/// Event-window averages for one quantile.
struct quantile_event_statistics {
    int quantile = 0; ///< Quantile label.
    std::vector<int> offsets; ///< Contiguous offsets, first through last.
    std::vector<double> means; ///< Mean value per offset; NaN when empty.
    std::vector<double> standard_deviations; ///< Sample deviation; NaN below two values.
};

/// Top-minus-bottom quantile spreads.
struct quantile_spread_result {
    std::vector<std::string> labels; ///< Periods; repeats for each group.
    std::vector<double> spreads; ///< Upper mean minus lower mean.
    std::vector<double> standard_errors; ///< Combined error; NaN when either leg is.
    /// Empty for aggregate output; otherwise parallel to labels.
    std::vector<std::string> groups;
};

/// Dated scalar series.
struct factor_series_result {
    std::vector<timestamp> dates; ///< Series dates, parallel to values.
    std::vector<double> values; ///< Values; NaN marks a missing point.
};

/// Computes the Spearman rank information coefficient per date and label.
///
/// Each cell correlates the finite factor values with the matching forward
/// returns across the assets of one date bucket; fewer than two usable pairs
/// leave the cell as NaN. `group_adjust` demeans forward returns within each
/// (date, group) bucket first and therefore requires groups, while `by_group`
/// emits one row per (date, group) pair instead of one row per date.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param group_adjust Demean forward returns per date and group before ranking.
/// @param by_group Emit one row per (date, group) instead of one row per date.
/// @return IC table whose `groups` member stays empty unless grouped.
/// @throws factor_data_error If group data is required or requested and a row
///     has no group column.
factor_ic_table factor_information_coefficient(
    const prepared_factor_data& factor_data, bool group_adjust = false,
    bool by_group = false);

/// Computes the mean information coefficient per label or per group.
///
/// Averages the finite IC values across dates for aggregate output, or across
/// the rows of each group when `by_group` is true. A label or group with no
/// finite value yields NaN.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param group_adjust Demean forward returns per date and group before ranking.
/// @param by_group Summarize one row per group instead of per label.
/// @return Summary with one mean per label or per group row.
/// @throws factor_data_error If group data is required or requested and a row
///     has no group column.
factor_ic_summary mean_information_coefficient(
    const prepared_factor_data& factor_data, bool group_adjust = false,
    bool by_group = false);

/// Computes per-observation factor weights.
///
/// Weights form inside each (date[, group]) bucket: `demeaned` centers the
/// bucket values, `equal_weight` gives each side of zero an equal share, and
/// the bucket is divided by its gross absolute exposure. `group_adjust` then
/// renormalizes across the groups of each date. A bucket whose gross exposure
/// is zero or non-finite yields NaN weights.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param demeaned Center the bucket values before weighting.
/// @param group_adjust Form weights per (date, group) and renormalize per date.
/// @param equal_weight Give each side of zero an equal share.
/// @return One row per prepared row, in input order.
/// @throws factor_data_error If `group_adjust` is true and a row has no group.
std::vector<factor_weight_row> factor_weights(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool equal_weight = false);

/// Computes portfolio returns from factor weights.
///
/// Calls `factor_weights` with the same options and accumulates weight times
/// forward return per date and label, skipping any pair with a non-finite
/// value.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param demeaned Center the bucket values before weighting.
/// @param group_adjust Form weights per (date, group) and renormalize per date.
/// @param equal_weight Give each side of zero an equal share.
/// @return Returns per date and label; a cell with no usable pair stays zero.
/// @throws factor_data_error If `group_adjust` is true and a row has no group.
factor_return_table factor_returns(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool equal_weight = false);

/// Regresses portfolio returns on the same-date universe mean return.
///
/// The slope is the OLS beta against the equal-weighted universe mean of each
/// label; the intercept is annualized with `252 / N` periods, where `N` is the
/// leading whole-day count of the label. Fewer than two usable dates or a zero
/// market variance leaves the estimates NaN, and a label without a leading day
/// count leaves only `annual_alpha` NaN.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param demeaned Center the bucket values before weighting.
/// @param group_adjust Form weights per (date, group) and renormalize per date.
/// @param equal_weight Give each side of zero an equal share.
/// @return One row per forward-return label, in label order.
/// @throws factor_data_error If `group_adjust` is true and a row has no group.
std::vector<factor_alpha_beta_row> factor_alpha_beta(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool equal_weight = false);

/// Computes mean forward returns per quantile.
///
/// Each date's quantile mean is the equal-weighted mean of its finite forward
/// returns; the reported mean averages those date means, and the standard error
/// is their sample deviation divided by the square root of the usable date
/// count. A quantile with no usable date keeps NaN.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param by_date Must be false; the by-date view has its own function.
/// @param by_group Emit one row per (quantile, group) pair.
/// @param demeaned Demean forward returns per date before averaging.
/// @param group_adjust Demean per date and group instead; implies groups.
/// @return Quantile table with means and standard errors.
/// @throws factor_data_error If `by_date` is true or group data is missing.
quantile_return_table mean_return_by_quantile(
    const prepared_factor_data& factor_data, bool by_date = false,
    bool by_group = false, bool demeaned = true,
    bool group_adjust = false);

/// Computes per-date quantile mean forward returns.
///
/// Each (date, quantile, label) cell averages the finite forward returns of the
/// assets in that bucket; the standard error is their sample deviation divided
/// by the square root of their count and stays NaN below two values.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param demeaned Demean forward returns per date before averaging.
/// @param group_adjust Demean per date and group instead; implies groups.
/// @param by_group Emit one row per (date, group) pair.
/// @return Cube over (date[, group]) rows x quantiles x labels.
/// @throws factor_data_error If group data is missing while required.
quantile_date_return_table mean_return_by_quantile_by_date(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool by_group = false);

/// Extracts event-aligned return windows for a factor panel.
///
/// Event dates are the factor dates present in the returns index. Each window
/// spans `before` ledger rows before the event through `after` rows after it,
/// clipped to the ledger bounds, so offsets near the edges are absent.
/// Non-finite ledger values are skipped. With `cumulative` the ledger is read
/// as already cumulative, otherwise each column is compounded into a wealth
/// index from one. `demean_by` subtracts the same-date cross-sectional mean of
/// its assets, and `mean_by_date` emits one averaged cell per offset and date.
///
/// @param factor Event panel; its dates and assets select the windows.
/// @param returns Ledger indexed by date with one double column per asset.
/// @param before Non-negative ledger rows before each event date.
/// @param after Non-negative ledger rows after each event date.
/// @param cumulative Read the ledger as cumulative rather than simple returns.
/// @param mean_by_date Emit date means with an empty asset label.
/// @param demean_by Optional panel supplying the assets to demean against.
/// @return Observed offsets, event dates, asset labels and window cells.
/// @throws factor_data_error If an offset is negative, the returns index has
///     duplicates, a factor asset is missing from the ledger, or a demean asset
///     is missing at an event date.
event_window_result common_start_returns(
    const factor_series& factor, const dataframe& returns, int before,
    int after, bool cumulative = false, bool mean_by_date = false,
    const factor_series* demean_by = nullptr);

/// Averages cumulative event-window returns per quantile.
///
/// Builds one event panel per observed quantile and calls `common_start_returns`
/// with cumulative and date-mean output; each offset reports the mean and the
/// sample standard deviation of its cells, an offset with no cell has NaN mean
/// and deviation, and an offset with a single cell has a NaN deviation.
///
/// @param factor_data Prepared rows; their quantiles define the groups.
/// @param returns Ledger indexed by date with one double column per asset.
/// @param before Non-negative ledger rows before each event date.
/// @param after Non-negative ledger rows after each event date.
/// @param demeaned Subtract the full-universe date mean from each cell.
/// @return One statistics block per quantile, in ascending quantile order.
/// @throws factor_data_error If an offset is negative or the ledger is invalid
///     for `common_start_returns`.
std::vector<quantile_event_statistics>
average_cumulative_return_by_quantile(
    const prepared_factor_data& factor_data, const dataframe& returns,
    int before = 10, int after = 15, bool demeaned = true);

/// Computes the upper-minus-lower quantile return spread.
///
/// The spread subtracts the lower quantile mean from the upper quantile mean
/// and combines their standard errors in quadrature when both are finite.
///
/// @param table Quantile means and standard errors to read.
/// @param upper_quantile Quantile used as the long leg.
/// @param lower_quantile Quantile used as the short leg.
/// @return Spread per label, or per group and label when the table is grouped.
/// @throws factor_data_error If an aggregate table lacks either quantile, or a
///     grouped table's groups are not parallel to its quantile rows.
quantile_spread_result mean_return_spread(
    const quantile_return_table& table, int upper_quantile,
    int lower_quantile);

/// Computes quantile turnover at a fixed lag.
///
/// Turnover on a date is the share of the quantile's assets that were absent
/// `period` dates earlier; membership follows the supplied rows even when a
/// factor value is non-finite. The first `period` dates and dates whose
/// quantile is empty are NaN.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param quantile Quantile whose membership is tracked.
/// @param period Positive lag in dates.
/// @return Series over every unique date, sorted ascending.
/// @throws factor_data_error If `period` is not positive.
factor_series_result quantile_turnover(
    const prepared_factor_data& factor_data, int quantile,
    int period = 1);

/// Computes the rank autocorrelation of the factor at a fixed lag.
///
/// Factor values are average-ranked within each date and then correlated with
/// the lagged ranks over the assets present on both dates. Non-finite factor
/// values are excluded; the first `period` dates, fewer than two shared assets
/// and a constant rank vector yield NaN.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param period Positive lag in dates.
/// @return Series over every unique date, sorted ascending.
/// @throws factor_data_error If `period` is not positive.
factor_series_result factor_rank_autocorrelation(
    const prepared_factor_data& factor_data, int period = 1);

/// Computes the mean of a sample of IC values.
///
/// NaN entries are skipped; an empty or all-NaN sample yields NaN.
///
/// @param values IC observations.
/// @return Arithmetic mean of the non-NaN values.
/// @throws factor_data_error If a value is infinite.
double ic_mean(std::span<const double> values);

/// Computes the t statistic of a sample of IC values.
///
/// The statistic is the mean divided by the sample standard error computed from
/// the one-degree-of-freedom deviation. NaN entries are skipped; fewer than two
/// usable values yield NaN, and a standard error below 1e-15 yields an infinity
/// or zero that follows the sign of the mean.
///
/// @param values IC observations.
/// @return t statistic of the sample.
/// @throws factor_data_error If a value is infinite.
double ic_t_stat(std::span<const double> values);

/// Returns the two-sided Student-t tail probability.
///
/// Uses the regularized incomplete beta function for `df` degrees of freedom.
/// A NaN statistic or a non-positive or NaN `df` yields NaN; an infinite
/// statistic yields zero.
///
/// @param statistic t statistic.
/// @param df Degrees of freedom; must be positive.
/// @return Two-sided p value, or NaN when it is undefined.
double student_t_two_sided_p(double statistic, double df);

/// Symmetric confidence interval for a mean IC.
struct ic_interval {
    double lower; ///< Lower bound; NaN when undefined.
    double upper; ///< Upper bound; NaN when undefined.
};

/// Computes a normal confidence interval for the mean IC.
///
/// The half-width is `z` times the sample standard error of the non-NaN values.
/// Fewer than two usable values yield NaN bounds.
///
/// @param values IC observations.
/// @param z Positive finite multiplier, conventionally 1.96 for 95%.
/// @return Interval bounds; both NaN below two usable values.
/// @throws factor_data_error If `z` is not finite and positive, or a value is
///     infinite.
ic_interval ic_confidence_interval(std::span<const double> values,
                                   double z = 1.96);

/// Benjamini-Hochberg step-up adjustment output.
struct fdr_result {
    std::vector<double> p_values; ///< Input p values, in input order.
    std::vector<double> adjusted_p_values; ///< Adjusted values, in input order.
    std::vector<bool> rejected; ///< Rejection flag per input, in input order.
    double alpha = 0.05; ///< Requested false-discovery rate.
};

/// Applies the Benjamini-Hochberg step-up correction.
///
/// Adjusted values are the monotone running minimum of `n * p / rank`, clamped
/// to [0, 1]. A hypothesis is rejected when its sorted p value is at or below
/// `alpha * rank / n`, which rejects the leading block up to the largest
/// passing rank. Empty input yields empty output.
///
/// @param p_values Finite probabilities in [0, 1].
/// @param alpha Target false-discovery rate in (0, 1].
/// @return Copies of the inputs plus adjusted values and rejection flags.
/// @throws factor_data_error If `alpha` is outside (0, 1] or a p value is not
///     a finite probability in [0, 1].
fdr_result benjamini_hochberg(std::span<const double> p_values,
                              double alpha = 0.05);

/// Covariance estimator for the Fama-MacBeth standard errors.
enum class fama_macbeth_covariance {
    iid,       ///< I.i.d. sample standard errors.
    newey_west ///< Bartlett-kernel HAC standard errors at `newey_west_lags`.
};

/// One estimated Fama-MacBeth coefficient series.
struct fama_macbeth_estimate {
    double mean = 0.0; ///< Mean of the per-date estimates.
    double std_error = 0.0; ///< Standard error of that mean.
    double t_stat = 0.0; ///< Ratio of the mean to the standard error.
};

/// Fama-MacBeth cross-sectional regression result.
struct fama_macbeth_result {
    fama_macbeth_estimate intercept; ///< Intercept across cross-sections.
    fama_macbeth_estimate exposure; ///< Exposure slope across cross-sections.
    std::size_t n_cross_sections = 0U; ///< Fitted cross-sections.
    fama_macbeth_covariance covariance = fama_macbeth_covariance::iid; ///< Requested estimator.
    std::size_t newey_west_lags = 1U; ///< Requested lags; recorded even for i.i.d.
};

/// Runs Fama-MacBeth cross-sectional regressions.
///
/// Each return date with a matching exposure row, or the single exposure row
/// that is broadcast across dates, is regressed cross-sectionally across the
/// shared asset columns. A cross-section is skipped when fewer than two assets
/// are finite or the exposures do not vary. The coefficient series are then
/// summarized with i.i.d. or Bartlett Newey-West standard errors; a standard
/// error at or below 1e-15 yields an infinite t statistic, or zero when the
/// mean is zero.
///
/// @param returns Returns with at least two double asset columns.
/// @param exposures Per-date exposures for the same assets, or one row
///     broadcast across every return date.
/// @param covariance Standard-error estimator for the coefficient series.
/// @param newey_west_lags Bartlett lags; must be smaller than the fitted
///     cross-section count when `covariance` is `newey_west`.
/// @return Slope and intercept estimates plus the fitted cross-section count.
/// @throws factor_data_error If an input is empty, has duplicate index values,
///     lacks two double asset columns, misses return assets, is unsorted for
///     Newey-West, or requests too many lags.
fama_macbeth_result fama_macbeth(
    const dataframe& returns, const dataframe& exposures,
    fama_macbeth_covariance covariance = fama_macbeth_covariance::iid,
    std::size_t newey_west_lags = 1U);

/// Inference outcome for one information-coefficient label.
struct factor_hypothesis {
    std::string label; ///< Forward-return label.
    std::size_t n_observations = 0U; ///< Finite IC observations used.
    double mean_ic = 0.0; ///< Mean of those observations.
    double t_stat = 0.0; ///< t statistic; NaN for an untestable label.
    double p_value = 0.0; ///< Two-sided p value; NaN when untestable.
    double adjusted_p_value = 0.0; ///< Benjamini-Hochberg value; NaN when untestable.
    bool rejected = false; ///< True when the adjusted test rejects at `alpha`.
    bool testable = false; ///< True with at least two finite observations.
};

/// Per-label IC inference with a false-discovery-rate adjustment.
struct ic_inference_result {
    double alpha = 0.05; ///< Requested false-discovery rate.
    std::vector<factor_hypothesis> hypotheses; ///< One entry per label.
};

/// Tests each IC label for a zero mean with an FDR adjustment.
///
/// Per label, the finite entries of the IC table are pooled and tested with a
/// two-sided Student-t test on `n - 1` degrees of freedom. Labels with fewer
/// than two finite entries are reported as untestable, with NaN t and p values.
/// Only the testable labels enter the Benjamini-Hochberg correction, which
/// sets `rejected` and `adjusted_p_value`.
///
/// @param information IC table whose rows are pooled per label.
/// @param alpha Target false-discovery rate in (0, 1].
/// @return One hypothesis per label, in the table's label order.
/// @throws factor_data_error If `alpha` is outside (0, 1].
ic_inference_result information_coefficient_inference(
    const factor_ic_table& information, double alpha = 0.05);

namespace detail {

/// Computes rank autocorrelation at several lags from one ranked panel.
///
/// Equivalent to calling `factor_rank_autocorrelation` once per lag, but the
/// within-date ranks are built only once.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param lags Positive lags in dates.
/// @return One series per lag, in the requested order.
/// @throws factor_data_error If any lag is not positive.
std::vector<factor_series_result> factor_rank_autocorrelation_many(
    const prepared_factor_data& factor_data, const std::vector<int>& lags);

/// Computes quantile turnover for several quantiles at one lag.
///
/// Equivalent to calling `quantile_turnover` once per quantile, but the
/// membership sets are collected only once.
///
/// @param factor_data Prepared rows and forward-return labels.
/// @param quantiles Quantiles whose membership is tracked.
/// @param period Positive lag in dates.
/// @return One series per quantile, in the requested order.
/// @throws factor_data_error If `period` is not positive.
std::vector<factor_series_result> quantile_turnover_many(
    const prepared_factor_data& factor_data, const std::vector<int>& quantiles,
    int period);

}  // namespace detail

}  // namespace fincore
