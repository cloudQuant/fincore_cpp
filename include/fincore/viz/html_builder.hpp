#pragma once

#include <fincore/viz/chart_data.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/**
 * One owning key/value entry for the native HTML report builder.
 *
 * Python's builder accepts a `pd.Series` of floats; the native builder uses
 * this explicit entry so a value is either a finite-checked number, text or
 * absent. Missing rendering follows the Python oracle: stats tables print
 * `nan`, metric cards print `N/A`.
 */
struct html_metric {
    std::string key;                  ///< Metric display key.
    std::optional<double> number;     ///< Numeric value when present.
    std::optional<std::string> text;  ///< Text value when no number is present.

    /// Creates a metric with a numeric value.
    static html_metric number_metric(std::string key, double value);
    /// Creates a metric with a text value.
    static html_metric text_metric(std::string key, std::string value);
    /// Creates a metric with no value.
    static html_metric missing(std::string key);
};

/**
 * Owning native self-contained HTML performance report builder.
 *
 * Mirrors `fincore.viz.html_backend.HtmlReportBuilder`: title/heading/raw
 * HTML sections, stats tables, metric cards, cumulative-returns/drawdown/
 * rolling-Sharpe tables and the monthly-returns pivot table. All chart values
 * come from precomputed native chart data; the builder never recomputes a
 * financial kernel. `build()` is deterministic and contains no external
 * stylesheet, script or image reference.
 */
class html_report_builder {
  public:
    /** Stable report CSS (the Python `HtmlReportBuilder._CSS` block). */
    static std::string_view css();

    /// Appends an escaped level-1 title section.
    html_report_builder& add_title(std::string title = "Performance Report");
    /// Appends an escaped heading clamped to levels 1 through 6.
    html_report_builder& add_heading(std::string text, int level = 2);
    /// Appends a two-column stats table with four-decimal numeric values.
    ///
    /// Missing values print `nan`, matching the Python oracle.
    html_report_builder& add_stats_table(const std::vector<html_metric>& stats);
    /// Appends metric cards using each metric's key in input order.
    html_report_builder& add_metric_cards(const std::vector<html_metric>& stats);
    /// Appends metric cards for `keys` in the requested order.
    ///
    /// Missing or non-finite values render as `N/A`.
    html_report_builder& add_metric_cards(const std::vector<html_metric>& stats,
                                          const std::vector<std::string>& keys);
    /// Appends raw HTML without escaping.
    html_report_builder& add_html(std::string raw_html);

    /// Appends a cumulative-returns table section.
    html_report_builder& plot_returns(const chart_series& series);
    /// Appends a drawdown table section.
    html_report_builder& plot_drawdown(const chart_series& series);
    /// Appends rolling-Sharpe tables and an optional benchmark table.
    html_report_builder& plot_rolling_sharpe(const chart_series& sharpe,
                                             const chart_series* benchmark = nullptr,
                                             std::size_t window = 252);
    /// Appends the monthly heatmap table in percent.
    ///
    /// Throws `input_error` when `grid.values` is not `years x 12`; missing
    /// cells print `NaN`.
    html_report_builder& plot_monthly_heatmap(const heat_grid& grid);

    /// Builds a self-contained deterministic HTML document.
    std::string build() const;
    /// Writes the built document to `path`.
    ///
    /// Throws `input_error` for an empty path or a file that cannot be opened
    /// or written.
    void save(const std::string& path) const;

    /// Returns the appended section fragments in order.
    const std::vector<std::string>& sections() const noexcept {
        return sections_;
    }

  private:
    std::vector<std::string> sections_;
};

}  // namespace fincore
