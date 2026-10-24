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
    std::string key;
    std::optional<double> number;
    std::optional<std::string> text;

    static html_metric number_metric(std::string key, double value);
    static html_metric text_metric(std::string key, std::string value);
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

    html_report_builder& add_title(std::string title = "Performance Report");
    html_report_builder& add_heading(std::string text, int level = 2);
    html_report_builder& add_stats_table(const std::vector<html_metric>& stats);
    html_report_builder& add_metric_cards(const std::vector<html_metric>& stats);
    html_report_builder& add_metric_cards(const std::vector<html_metric>& stats,
                                          const std::vector<std::string>& keys);
    html_report_builder& add_html(std::string raw_html);

    html_report_builder& plot_returns(const chart_series& series);
    html_report_builder& plot_drawdown(const chart_series& series);
    html_report_builder& plot_rolling_sharpe(const chart_series& sharpe,
                                             const chart_series* benchmark = nullptr,
                                             std::size_t window = 252);
    html_report_builder& plot_monthly_heatmap(const heat_grid& grid);

    std::string build() const;
    void save(const std::string& path) const;

    const std::vector<std::string>& sections() const noexcept {
        return sections_;
    }

  private:
    std::vector<std::string> sections_;
};

}  // namespace fincore
