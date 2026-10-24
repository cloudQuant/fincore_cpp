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
inline constexpr double decimal_to_bps = DECIMAL_TO_BPS;

/// Renderer style contexts (seaborn ``plotting_context`` / ``axes_style``).
struct style_context {
    std::string name;
    double font_scale = 1.5;
    std::map<std::string, std::string> rc;
};

style_context plotting_context(std::string context = "notebook",
                               double font_scale = 1.5,
                               std::map<std::string, std::string> rc = {});
style_context axes_style(std::string style = "darkgrid",
                         std::map<std::string, std::string> rc = {});

/// Native replacement for the ``customize`` decorator: the context a renderer
/// applies around a chart call. It carries no callable identity.
struct render_customization {
    bool set_context = true;
    bool colorblind_palette = true;
    bool despine_left = true;
    style_context context{"notebook", 1.5, {{"lines.linewidth", "1.5"}}};
    style_context axes{"darkgrid", 1.5, {}};
    std::vector<std::string> palette{"#0173B2", "#DE8F05", "#029E73",
                                     "#D55E00", "#CC78BC", "#CA9161",
                                     "#FBAFE4", "#949494"};
};

render_customization customize(bool set_context = true);

// --- Chart data ----------------------------------------------------------

struct cumulative_quantile_series {
    int quantile = 0;
    std::vector<timestamp> dates;
    std::vector<double> values;  // cumulative growth, NaN where undefined
};

struct quantile_cumulative_chart {
    std::string period;
    std::string title;
    std::string y_label = "Log Cumulative Returns";
    std::vector<cumulative_quantile_series> series;  // quantiles descending
    double y_min = 0.0;
    double y_max = 1.0;
    bool log_scale = true;
    double reference = 1.0;
    /// Deterministic native SVG document for this chart.
    std::string to_svg() const;
};

quantile_cumulative_chart build_cumulative_returns_by_quantile(
    const quantile_date_return_table& returns, std::string period);

struct events_distribution_chart {
    bool valid = true;
    std::string title = "Distribution of events in time";
    std::string x_label = "Date";
    std::string y_label = "Number of events";
    std::size_t requested_bars = 50U;
    std::vector<timestamp> bucket_starts;
    std::vector<double> counts;
    std::string to_svg() const;
};

/// Event counts per equal-width time bucket. Values follow the dates and only
/// finite values are counted; pass an empty span to count every event.
events_distribution_chart build_events_distribution(
    const std::vector<timestamp>& dates,
    const std::vector<double>& values = {}, std::size_t num_bars = 50U);

struct qq_series {
    std::string period;
    std::vector<double> theoretical_quantiles;
    std::vector<double> observed_quantiles;
    double mean = 0.0;
    double scale = 1.0;
    double reference_slope = 1.0;
    double reference_intercept = 0.0;
};

struct qq_chart {
    std::vector<qq_series> series;
    std::string x_label = "Normal Distribution Quantile";
    std::string y_label = "Observed Quantile";
    std::string to_svg() const;
};

std::vector<qq_chart> build_ic_qq(const factor_ic_table& ic);

struct spread_time_series_chart {
    std::string period;
    std::string title;
    std::string y_label =
        "Difference In Quantile Mean Return (bps)";
    std::vector<timestamp> dates;
    std::vector<double> spreads;             // bps
    std::vector<double> rolling_mean;        // 22-observation mean, bps
    std::vector<double> band_lower;          // bps, empty without std_err
    std::vector<double> band_upper;
    double y_limit = 1.0;
    double bandwidth = 1.0;
    std::string to_svg() const;
};

spread_time_series_chart build_mean_quantile_returns_spread_time_series(
    const std::vector<timestamp>& dates, const std::vector<double>& spreads,
    std::string period, const std::vector<double>& standard_errors = {},
    double bandwidth = 1.0);

struct monthly_ic_heatmap_chart {
    std::string period;
    std::string group;
    std::string title;
    std::vector<int> years;
    std::vector<std::string> month_columns;
    std::vector<double> values;   // years x 12, row-major, NaN when absent
    std::vector<bool> present;
    std::string to_svg() const;
};

/// One heatmap per forward period (and per group when the table is grouped).
std::vector<monthly_ic_heatmap_chart> build_monthly_ic_heatmap(
    const factor_time_information_table& monthly);

struct violin_group {
    int quantile = 0;
    std::string period;
    std::vector<double> samples;  // finite period returns in bps
    double first_quartile = 0.0;
    double median = 0.0;
    double third_quartile = 0.0;
    std::vector<double> density_x;
    std::vector<double> density_y;
};

struct violin_chart {
    std::string title = "Period Wise Return By Factor Quantile";
    std::string y_label = "Return (bps)";
    std::vector<violin_group> groups;
    std::string to_svg() const;
};

violin_chart build_quantile_returns_violin(
    const quantile_date_return_table& returns, std::string period);

// Route-named native equivalents. The builders above are the canonical
// implementations; these aliases keep the Python renderer entry names.
inline quantile_cumulative_chart plot_cumulative_returns_by_quantile(
    const quantile_date_return_table& returns, std::string period) {
    return build_cumulative_returns_by_quantile(returns, std::move(period));
}
inline events_distribution_chart plot_events_distribution(
    const std::vector<timestamp>& dates,
    const std::vector<double>& values = {}, std::size_t num_bars = 50U) {
    return build_events_distribution(dates, values, num_bars);
}
inline std::vector<qq_chart> plot_ic_qq(const factor_ic_table& ic) {
    return build_ic_qq(ic);
}
inline spread_time_series_chart plot_mean_quantile_returns_spread_time_series(
    const std::vector<timestamp>& dates, const std::vector<double>& spreads,
    std::string period, const std::vector<double>& standard_errors = {},
    double bandwidth = 1.0) {
    return build_mean_quantile_returns_spread_time_series(
        dates, spreads, std::move(period), standard_errors, bandwidth);
}
inline std::vector<monthly_ic_heatmap_chart> plot_monthly_ic_heatmap(
    const factor_time_information_table& monthly) {
    return build_monthly_ic_heatmap(monthly);
}
inline violin_chart plot_quantile_returns_violin(
    const quantile_date_return_table& returns, std::string period) {
    return build_quantile_returns_violin(returns, std::move(period));
}

// --- GridFigure: native layout model -------------------------------------

struct grid_cell {
    std::size_t row = 0U;
    std::size_t col = 0U;
    bool full_row = false;
    std::size_t index = 0U;
};

/// Native layout model replacing the Matplotlib-backed ``GridFigure``.
///
/// It owns only geometry (figure size, spacing and allocated cells); Python
/// figure object identity is not claimed. ``close`` is idempotent and any
/// later allocation fails, matching the source lifecycle.
class grid_figure {
public:
    grid_figure(std::size_t rows, std::size_t cols);

    std::size_t rows() const noexcept { return rows_; }
    std::size_t cols() const noexcept { return cols_; }
    std::size_t current_row() const noexcept { return current_row_; }
    std::size_t current_col() const noexcept { return current_col_; }
    bool closed() const noexcept { return closed_; }
    double width() const noexcept { return 14.0; }
    double height() const noexcept { return static_cast<double>(rows_) * 7.0; }
    double wspace() const noexcept { return 0.4; }
    double hspace() const noexcept { return 0.3; }
    const std::vector<grid_cell>& cells() const noexcept { return cells_; }

    /// Return the owned figure identifier (1 for every native grid).
    std::uint64_t create_new_figure() const;
    /// Allocate the next full-width row.
    grid_cell next_row();
    /// Allocate the next cell, advancing to a new row when necessary.
    grid_cell next_cell();
    /// Close this grid's figure and release layout state.
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
    std::vector<std::uint64_t> figures;
    std::vector<bool> closed;
};

void close_owned_figures(owned_figures& figures);
/// Return the display order without opening a UI window.
std::vector<std::uint64_t> show_owned_figures(const owned_figures& figures);

}  // namespace fincore
