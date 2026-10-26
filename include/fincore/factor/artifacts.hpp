#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/viz/chart_data.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/// Alphalens-compatible decimal-to-basis-points scale.
inline constexpr double DECIMAL_TO_BPS = 10'000.0;
/// Alias for `DECIMAL_TO_BPS`.
inline constexpr double decimal_to_bps = DECIMAL_TO_BPS;

/// Renderer style contexts (seaborn ``plotting_context`` / ``axes_style``).
struct style_context {
    std::string name;  ///< Context or style name, such as "notebook".
    double font_scale = 1.5;  ///< Font scaling factor carried by the context.
    std::map<std::string, std::string> rc;  ///< rc parameter overrides by name.
};

/// Creates a renderer style context with an explicit font scale.
///
/// The returned context uses `context` as its name and keeps `rc` unchanged
/// except for adding ``lines.linewidth=1.5`` when that key is absent.
///
/// @param context Context name.
/// @param font_scale Font scaling factor stored on the context.
/// @param rc rc parameter overrides keyed by parameter name.
/// @return Style context carrying the name, font scale and overrides.
style_context plotting_context(std::string context = "notebook",
                               double font_scale = 1.5,
                               std::map<std::string, std::string> rc = {});
/// Creates a renderer axes style with a fixed font scale of 1.5.
///
/// @param style Style name.
/// @param rc rc parameter overrides keyed by parameter name.
/// @return Style context named `style`; `font_scale` is always 1.5.
style_context axes_style(std::string style = "darkgrid",
                         std::map<std::string, std::string> rc = {});

/// Native replacement for the ``customize`` decorator: the context a renderer
/// applies around a chart call. It carries no callable identity.
struct render_customization {
    bool set_context = true;  ///< Whether `context` is applied around the call.
    bool colorblind_palette = true;  ///< Whether a colorblind palette is used.
    bool despine_left = true;  ///< Whether the left spine is removed.
    /// Context applied around the chart call when `set_context` is true.
    style_context context{"notebook", 1.5, {{"lines.linewidth", "1.5"}}};
    /// Axes style applied around the chart call.
    style_context axes{"darkgrid", 1.5, {}};
    /// Colorblind-safe color cycle used when `colorblind_palette` is true.
    std::vector<std::string> palette{"#0173B2", "#DE8F05", "#029E73",
                                     "#D55E00", "#CC78BC", "#CA9161",
                                     "#FBAFE4", "#949494"};
};

/// Creates the native render customization used around chart calls.
///
/// The returned value uses the default `plotting_context()` and
/// `axes_style()` values and keeps the built-in eight-color palette.
///
/// @param set_context Whether the context is applied around the chart call.
/// @return Customization carrying the default context, axes style and palette.
render_customization customize(bool set_context = true);

// --- Chart data ----------------------------------------------------------

/// Cumulative growth of one quantile across a chart's dates.
struct cumulative_quantile_series {
    int quantile = 0;  ///< Quantile bucket label.
    /// Dates aligned with `values`; sorted and unique in builder output.
    std::vector<timestamp> dates;
    /// Cumulative growth from 1.0; a date without a finite mean keeps the
    /// previous value.
    std::vector<double> values;
};

/// Cumulative-return chart data for every quantile of one forward period.
struct quantile_cumulative_chart {
    std::string period;  ///< Forward-return period label used in the title.
    std::string title;  ///< Chart title.
    std::string y_label = "Log Cumulative Returns";  ///< Y-axis label.
    std::vector<cumulative_quantile_series> series;  ///< Quantiles descending.
    double y_min = 0.0;  ///< Lower bound of the cumulative-growth axis.
    double y_max = 1.0;  ///< Upper bound of the cumulative-growth axis.
    /// Log-scale flag; the native SVG renderer maps cumulative values
    /// linearly.
    bool log_scale = true;
    /// Horizontal reference value, drawn by `to_svg()` when inside the bounds.
    double reference = 1.0;
    /// Renders the chart as a deterministic native SVG document.
    std::string to_svg() const;
};

/// Builds cumulative-growth chart data for each quantile of one period.
///
/// Means are averaged per date for the requested quantile and label, then
/// compounded from 1.0 in date order. Dates without a finite mean keep the
/// previous growth value. Quantiles are emitted in descending order, and the
/// y bounds cover every emitted growth value.
///
/// @param returns Mean returns by date, quantile and label.
/// @param period Forward-return label; must be present in `returns.labels`.
/// @return Chart data with one series per distinct quantile.
/// @throws factor_data_error If `period` is not present in `returns.labels`.
quantile_cumulative_chart build_cumulative_returns_by_quantile(
    const quantile_date_return_table& returns, std::string period);

/// Event counts per equal-width time bucket.
struct events_distribution_chart {
    /// Whether the chart is drawable; the builders leave it true.
    bool valid = true;
    std::string title = "Distribution of events in time";  ///< Chart title.
    std::string x_label = "Date";  ///< X-axis label.
    std::string y_label = "Number of events";  ///< Y-axis label.
    /// Requested number of equal-width buckets, stored as supplied.
    std::size_t requested_bars = 50U;
    /// Bucket start timestamps, aligned with `counts`.
    std::vector<timestamp> bucket_starts;
    /// Event counts aligned with `bucket_starts`.
    std::vector<double> counts;
    /// Renders the bucket counts as a deterministic native SVG bar chart.
    ///
    /// A chart without counts still carries the title and axis labels.
    std::string to_svg() const;
};

/// Builds event counts per equal-width time bucket.
///
/// `values` follows `dates`; non-finite values are skipped, and an empty
/// `values` counts every event. Buckets are half-open except the final bucket,
/// which also covers the latest date. A single distinct date produces one
/// bucket.
///
/// @param dates Event timestamps.
/// @param values Optional event values aligned with `dates`.
/// @param num_bars Number of equal-width buckets; must be positive.
/// @return Bucket starts and counts; no buckets are produced for empty dates.
/// @throws factor_data_error If `num_bars` is zero, `values` is non-empty with
///     a different length than `dates`, or the date span cannot form
///     positive-width buckets.
events_distribution_chart build_events_distribution(
    const std::vector<timestamp>& dates,
    const std::vector<double>& values = {}, std::size_t num_bars = 50U);

/// Normal Q-Q series for the information coefficient of one period.
struct qq_series {
    std::string period;  ///< Forward-return period label.
    /// Standard-normal quantiles at ``(i + 1) / (n + 1)``.
    std::vector<double> theoretical_quantiles;
    /// Sorted IC observations aligned with `theoretical_quantiles`; values are
    /// standardized only when `mean` is nonzero and `scale` is positive and
    /// not 1.
    std::vector<double> observed_quantiles;
    /// Mean of the observations; non-finite IC values count as zero.
    double mean = 0.0;
    /// Population standard deviation of the observations.
    double scale = 1.0;
    /// Slope of the reference line drawn by the interactive backend.
    double reference_slope = 1.0;
    /// Intercept of the reference line drawn by the interactive backend.
    double reference_intercept = 0.0;
};

/// Q-Q chart data for one or more periods.
struct qq_chart {
    std::vector<qq_series> series;  ///< Series in draw order.
    std::string x_label = "Normal Distribution Quantile";  ///< X-axis label.
    std::string y_label = "Observed Quantile";  ///< Y-axis label.
    /// Renders the Q-Q scatter and a diagonal reference line as native SVG.
    ///
    /// `reference_slope` and `reference_intercept` are not used by this
    /// renderer.
    std::string to_svg() const;
};

/// Builds normal Q-Q chart data from information coefficients.
///
/// One chart is returned per label, each holding one series whose observations
/// are the IC values for that label; non-finite values are treated as zero.
/// Theoretical quantiles use the standard-normal inverse CDF.
///
/// @param ic Information-coefficient table with values in date-major order.
/// @return One chart per label, in `ic.labels` order; empty when `ic.dates` or
///     `ic.labels` is empty or no observations are available.
std::vector<qq_chart> build_ic_qq(const factor_ic_table& ic);

/// Spread chart data for one forward period.
struct spread_time_series_chart {
    std::string period;  ///< Forward-return period label used in the title.
    std::string title;  ///< Chart title.
    std::string y_label =
        "Difference In Quantile Mean Return (bps)";  ///< Y-axis label.
    std::vector<timestamp> dates;  ///< Observation timestamps.
    /// Spreads in basis points; non-finite inputs become NaN.
    std::vector<double> spreads;
    /// Trailing 22-observation mean in basis points; NaN until 22 consecutive
    /// finite spreads are available.
    std::vector<double> rolling_mean;
    /// Lower band in basis points; empty when no standard errors are supplied
    /// and NaN where an input is non-finite.
    std::vector<double> band_lower;
    /// Upper band in basis points, aligned with `band_lower`.
    std::vector<double> band_upper;
    /// Symmetric y-axis limit: the 95th percentile of finite absolute spreads,
    /// falling back to 1.0 when no positive value exists.
    double y_limit = 1.0;
    /// Standard-error multiplier used to build the band.
    double bandwidth = 1.0;
    /// Renders spreads, the rolling mean and the optional band as native SVG.
    ///
    /// A zero line is added when dates are present; the band polygon is
    /// skipped when `band_lower` is empty.
    std::string to_svg() const;
};

/// Builds spread time-series chart data for one forward period.
///
/// Spreads and standard errors are converted from decimal returns to basis
/// points. The rolling mean is a trailing 22-observation mean and stays NaN
/// until 22 consecutive finite spreads exist. Bands are built as
/// `spread +/- standard_error * 10000 * bandwidth` when standard errors are
/// supplied.
///
/// @param dates Observation timestamps.
/// @param spreads Spreads in decimal return units, aligned with `dates`.
/// @param period Forward-return period label used in the title.
/// @param standard_errors Optional standard errors in decimal return units,
///     aligned with `spreads`.
/// @param bandwidth Standard-error multiplier used for the band.
/// @return Chart data with bps spreads, rolling mean and optional band.
/// @throws factor_data_error If `dates` and `spreads` differ in length, or
///     `standard_errors` is non-empty with a different length.
spread_time_series_chart build_mean_quantile_returns_spread_time_series(
    const std::vector<timestamp>& dates, const std::vector<double>& spreads,
    std::string period, const std::vector<double>& standard_errors = {},
    double bandwidth = 1.0);

/// Monthly information-coefficient heatmap for one period and group.
struct monthly_ic_heatmap_chart {
    std::string period;  ///< Forward-return period label.
    std::string group;  ///< Group label; empty for aggregate output.
    std::string title;  ///< Chart title.
    std::vector<int> years;  ///< Row year labels, ascending.
    /// Twelve month labels, January first.
    std::vector<std::string> month_columns;
    /// Years x 12 values in row-major order; NaN for an absent month.
    std::vector<double> values;
    /// Cell presence flags aligned with `values`.
    std::vector<bool> present;
    /// Renders one rectangle per present heatmap cell as native SVG.
    ///
    /// Cell colors use a red/blue diverging scale normalized by the largest
    /// absolute value. Year and month labels are not drawn.
    std::string to_svg() const;
};

/// Builds one heatmap per forward period (and per group when the table is
/// grouped).
///
/// Years are the distinct calendar years observed for the selected group and
/// are emitted in ascending order. Months without a finite mean stay NaN with
/// `present == false`. No charts are returned when `monthly.labels` or
/// `monthly.dates` is empty.
///
/// @param monthly Monthly information-coefficient table with means in
///     date-major order and optional groups parallel to `dates`.
/// @return One chart per group and label; groups are emitted in sorted order.
std::vector<monthly_ic_heatmap_chart> build_monthly_ic_heatmap(
    const factor_time_information_table& monthly);

/// Violin samples and quartiles for one quantile of a forward period.
struct violin_group {
    int quantile = 0;  ///< Quantile bucket label.
    std::string period;  ///< Forward-return period label.
    std::vector<double> samples;  ///< Finite period returns in basis points.
    double first_quartile = 0.0;  ///< First quartile of `samples` in bps.
    double median = 0.0;  ///< Median of `samples` in bps.
    double third_quartile = 0.0;  ///< Third quartile of `samples` in bps.
    /// Support points for the kernel-density estimate.
    std::vector<double> density_x;
    /// Density values aligned with `density_x`; empty when no density was
    /// computed.
    std::vector<double> density_y;
};

/// Quantile-return violin chart data for one forward period.
struct violin_chart {
    /// Chart title.
    std::string title = "Period Wise Return By Factor Quantile";
    std::string y_label = "Return (bps)";  ///< Y-axis label.
    std::vector<violin_group> groups;  ///< Groups, ascending by quantile.
    /// Renders one violin polygon per group as native SVG.
    ///
    /// Groups without density points or with zero density are skipped.
    std::string to_svg() const;
};

/// Builds quantile-return violin data for one forward period.
///
/// Each group collects finite mean returns for one quantile, converted from
/// decimal returns to basis points. Quartiles use linear interpolation over
/// the sorted samples, and the density is a Gaussian kernel estimate on 64
/// support points when there are at least two samples and their deviation is
/// positive. Quantiles are emitted in ascending order, and quantiles without
/// finite samples are omitted.
///
/// @param returns Mean returns by date, quantile and label.
/// @param period Forward-return label; must be present in `returns.labels`.
/// @return Violin chart data with one group per quantile that has samples.
/// @throws factor_data_error If `period` is not present in `returns.labels`.
violin_chart build_quantile_returns_violin(
    const quantile_date_return_table& returns, std::string period);

// Route-named native equivalents. The builders above are the canonical
// implementations; these aliases keep the Python renderer entry names.
/// Builds cumulative-growth chart data via
/// `build_cumulative_returns_by_quantile`.
///
/// Arguments and validation are forwarded unchanged.
///
/// @param returns Mean returns by date, quantile and label.
/// @param period Forward-return label; must be present in `returns.labels`.
/// @return Chart data identical to the canonical builder's result.
inline quantile_cumulative_chart plot_cumulative_returns_by_quantile(
    const quantile_date_return_table& returns, std::string period) {
    return build_cumulative_returns_by_quantile(returns, std::move(period));
}
/// Builds event-bucket chart data via `build_events_distribution`.
///
/// @param dates Event timestamps.
/// @param values Optional event values aligned with `dates`.
/// @param num_bars Number of equal-width buckets; must be positive.
/// @return Chart data identical to the canonical builder's result.
inline events_distribution_chart plot_events_distribution(
    const std::vector<timestamp>& dates,
    const std::vector<double>& values = {}, std::size_t num_bars = 50U) {
    return build_events_distribution(dates, values, num_bars);
}
/// Builds Q-Q chart data via `build_ic_qq`.
///
/// @param ic Information-coefficient table with values in date-major order.
/// @return One chart per label, in `ic.labels` order.
inline std::vector<qq_chart> plot_ic_qq(const factor_ic_table& ic) {
    return build_ic_qq(ic);
}
/// Builds spread chart data via
/// `build_mean_quantile_returns_spread_time_series`.
///
/// @param dates Observation timestamps.
/// @param spreads Spreads in decimal return units, aligned with `dates`.
/// @param period Forward-return period label.
/// @param standard_errors Optional standard errors in decimal return units.
/// @param bandwidth Standard-error multiplier used for the band.
/// @return Chart data identical to the canonical builder's result.
inline spread_time_series_chart plot_mean_quantile_returns_spread_time_series(
    const std::vector<timestamp>& dates, const std::vector<double>& spreads,
    std::string period, const std::vector<double>& standard_errors = {},
    double bandwidth = 1.0) {
    return build_mean_quantile_returns_spread_time_series(
        dates, spreads, std::move(period), standard_errors, bandwidth);
}
/// Builds monthly IC heatmap data via `build_monthly_ic_heatmap`.
///
/// @param monthly Monthly information-coefficient table.
/// @return One chart per group and label.
inline std::vector<monthly_ic_heatmap_chart> plot_monthly_ic_heatmap(
    const factor_time_information_table& monthly) {
    return build_monthly_ic_heatmap(monthly);
}
/// Builds quantile-return violin data via `build_quantile_returns_violin`.
///
/// @param returns Mean returns by date, quantile and label.
/// @param period Forward-return label; must be present in `returns.labels`.
/// @return Violin chart data with one group per quantile that has samples.
inline violin_chart plot_quantile_returns_violin(
    const quantile_date_return_table& returns, std::string period) {
    return build_quantile_returns_violin(returns, std::move(period));
}

// --- GridFigure: native layout model -------------------------------------

/// Zero-based position of one allocated grid cell.
struct grid_cell {
    std::size_t row = 0U;  ///< Zero-based row.
    std::size_t col = 0U;  ///< Zero-based column; 0 for a full-row cell.
    bool full_row = false;  ///< Whether the cell spans the full row.
    std::size_t index = 0U;  ///< Allocation order within `cells()`.
};

/// Native layout model replacing the Matplotlib-backed ``GridFigure``.
///
/// It owns only geometry (figure size, spacing and allocated cells); Python
/// figure object identity is not claimed. ``close`` is idempotent and any
/// later allocation fails, matching the source lifecycle.
class grid_figure {
public:
    /// Creates a grid with `rows` rows and `cols` columns.
    ///
    /// @param rows Row count; must be positive.
    /// @param cols Column count; must be positive.
    /// @throws factor_data_error If `rows` or `cols` is zero.
    grid_figure(std::size_t rows, std::size_t cols);

    /// Returns the configured row count.
    std::size_t rows() const noexcept { return rows_; }
    /// Returns the configured column count.
    std::size_t cols() const noexcept { return cols_; }
    /// Returns the current cursor row.
    std::size_t current_row() const noexcept { return current_row_; }
    /// Returns the current cursor column.
    std::size_t current_col() const noexcept { return current_col_; }
    /// Returns whether `close()` has been called.
    bool closed() const noexcept { return closed_; }
    /// Returns the fixed figure width of 14.
    double width() const noexcept { return 14.0; }
    /// Returns the figure height, seven units per row.
    double height() const noexcept { return static_cast<double>(rows_) * 7.0; }
    /// Returns the fixed horizontal spacing of 0.4.
    double wspace() const noexcept { return 0.4; }
    /// Returns the fixed vertical spacing of 0.3.
    double hspace() const noexcept { return 0.3; }
    /// Returns the allocated cells in allocation order; empty after `close()`.
    const std::vector<grid_cell>& cells() const noexcept { return cells_; }

    /// Returns the owned figure identifier (1 for every native grid).
    ///
    /// @return Figure identifier, always 1.
    /// @throws std::runtime_error If the grid is closed.
    std::uint64_t create_new_figure() const;
    /// Allocates the next full-width row.
    ///
    /// @return Allocated full-row cell.
    /// @throws std::runtime_error If the grid is closed.
    /// @throws std::out_of_range If the next row lies beyond the grid.
    grid_cell next_row();
    /// Allocates the next cell, advancing to a new row when necessary.
    ///
    /// @return Allocated cell.
    /// @throws std::runtime_error If the grid is closed.
    /// @throws std::out_of_range If the next cell lies beyond the grid.
    grid_cell next_cell();
    /// Closes this grid's figure and releases layout state.
    ///
    /// The call is idempotent.
    void close() noexcept;

private:
    void require_open() const;

    std::size_t rows_ = 1U;
    std::size_t cols_ = 1U;
    std::size_t current_row_ = 0U;
    std::size_t current_col_ = 0U;
    bool closed_ = false;
    std::vector<grid_cell> cells_;
};

/// Caller-owned figure identifiers returned by a tear-sheet workflow.
struct owned_figures {
    /// Figure identifiers in display order.
    std::vector<std::uint64_t> figures;
    /// Closed flags aligned with `figures`; filled by `close_owned_figures`.
    std::vector<bool> closed;
};

/// Marks every owned figure as closed.
///
/// The identifier list is left unchanged, so the call is idempotent.
///
/// @param figures Figure list whose `closed` flags are set to true.
void close_owned_figures(owned_figures& figures);
/// Returns the figure identifiers in display order without opening a window.
///
/// @param figures Figure list to display.
/// @return Copy of `figures.figures`.
std::vector<std::uint64_t> show_owned_figures(const owned_figures& figures);

}  // namespace fincore
