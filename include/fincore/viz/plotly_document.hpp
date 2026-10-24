#pragma once

#include <fincore/viz/chart_data.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/**
 * Headless interactive status recorded by native `show` entry points.
 *
 * Native builds never open a browser or a GUI window. `show` returns `false`
 * and this constant is the documented status a caller may record instead of
 * claiming an interactive display.
 */
inline constexpr std::string_view NOT_RUN_INTERACTIVE = "NOT_RUN_INTERACTIVE";

/** Theme colors used by the native Plotly-compatible document. */
struct plotly_color_theme {
    std::string background;
    std::string grid;
    std::string text;
    std::string positive;
    std::string negative;
    std::string neutral;
    std::string highlight;
    std::string warning;
    bool dark = false;
};

/**
 * One owning Plotly-compatible trace.
 *
 * Values are copied from native chart data (`series_chart`, `heat_grid`,
 * `correlation_matrix`, `frontier_chart_data`); rendering never recomputes a
 * financial kernel. NaN entries are serialized as JSON `null` so missingness
 * is explicit.
 */
struct plotly_trace {
    enum class kind { scatter, heatmap };

    kind type = kind::scatter;
    std::string name;
    std::string mode;
    std::string line_color;
    double line_width = 0.0;
    std::string line_dash;
    std::string fill;
    std::string fill_color;
    double marker_size = 0.0;
    std::string marker_color;
    std::string marker_symbol;
    std::string hovertemplate;

    // Scatter data.
    std::vector<std::string> x_labels;  // date/category labels, wins over x_values
    std::vector<double> x_values;       // numeric x when x_labels is empty
    std::vector<double> y_values;       // NaN -> JSON null
    std::vector<double> custom_data;    // NaN -> JSON null

    // Heatmap data.
    std::vector<std::string> heatmap_x;
    std::vector<std::string> heatmap_y;
    std::vector<double> heatmap_z;  // rows x columns, row-major; NaN -> JSON null
    std::size_t heatmap_columns = 0U;
    std::string colorscale;                                    // named scale
    std::vector<std::pair<double, std::string>> color_stops;    // custom scale
    bool color_range_set = false;
    double color_min = 0.0;
    double color_max = 0.0;
    bool zmid_zero = false;
    std::string colorbar_title;
    std::string colorbar_tickformat;
};

struct plotly_axis {
    std::string title;
    std::string side;
    bool reversed = false;
    std::string tickmode;
    std::vector<double> tick_values;
    std::vector<std::string> tick_text;
};

struct plotly_shape {
    std::string type = "line";
    double y0 = 0.0;
    double y1 = 0.0;
    std::string yref = "y";
    std::string xref = "paper";
    double x0 = 0.0;
    double x1 = 1.0;
    std::string line_color;
    double line_width = 1.0;
    std::string line_dash;
};

struct plotly_annotation {
    std::string text;
    double x = 0.0;
    double y = 0.0;
    std::string xref = "paper";
    std::string yref = "paper";
    bool show_arrow = false;
};

/**
 * Owning native Plotly-compatible chart document.
 *
 * `to_json()` is deterministic (fixed key order, Python-style float text) and
 * covers the emitted subset of the Plotly schema: `data` traces with
 * scatter/heatmap fields and a `layout` with title, axes, legend, shapes and
 * annotations. Plotly.js object identity is not claimed.
 */
struct plotly_document {
    std::string title;
    std::string template_name;
    std::optional<int> height = 500;
    std::optional<int> width;
    bool show_legend = true;
    std::string hovermode;
    std::string font_color;
    std::string plot_background;
    std::string paper_background;
    std::string grid_color;
    plotly_axis x_axis;
    plotly_axis y_axis;
    std::vector<plotly_trace> traces;
    std::vector<plotly_shape> shapes;
    std::vector<plotly_annotation> annotations;

    std::string to_json() const;
    /** Deterministic SVG rendering of the document (no third-party asset). */
    std::string to_svg() const;
    /** Self-contained HTML: inline SVG plus the deterministic JSON payload. */
    std::string to_html() const;
    void save_html(const std::string& path) const;
    /** Writes SVG; declared format variance from Plotly's PNG export. */
    void save_image(const std::string& path) const;
};

/**
 * Native Plotly-compatible backend.
 *
 * Mirrors `fincore.viz.interactive.plotly_backend.PlotlyBackend` artifact
 * behaviour. Chart inputs are native chart data, not pandas objects. `show`
 * is headless and always reports `NOT_RUN_INTERACTIVE`.
 */
class plotly_backend {
  public:
    explicit plotly_backend(std::string theme = "light", int height = 500,
                            std::optional<int> width = std::nullopt,
                            std::optional<std::string> template_name = std::nullopt,
                            bool show_legend = true);

    const std::string& theme() const noexcept { return theme_; }
    const plotly_color_theme& colors() const noexcept { return colors_; }
    const std::string& default_template() const noexcept {
        return default_template_;
    }
    const std::string& effective_template() const noexcept;
    int height() const noexcept { return height_; }
    const std::optional<int>& width() const noexcept { return width_; }
    bool show_legend() const noexcept { return show_legend_; }

    /** Base document with the configured template, size and theme colors. */
    plotly_document create_document(std::string title) const;

    plotly_document plot_returns(const series_chart& chart);
    plotly_document plot_drawdown(const series_chart& chart);
    // The recorded `chart.window` wins when non-zero; `window` is the
    // fallback for charts built without a recorded window.
    plotly_document plot_rolling_sharpe(const series_chart& chart,
                                        std::size_t window = 252);
    plotly_document plot_monthly_heatmap(const heat_grid& grid);
    plotly_document plot_correlation_matrix(const correlation_matrix& matrix);
    plotly_document plot_efficient_frontier(const frontier_chart_data& frontier);

    /** Record an externally assembled document as the current figure. */
    void set_figure(plotly_document document);
    bool has_figure() const noexcept { return figure_.has_value(); }
    /** Throws `input_error` when no plot method ran yet. */
    const plotly_document& figure() const;

    void save_html(const std::string& path) const;
    void save_image(const std::string& path) const;
    /** Headless: returns false, records NOT_RUN_INTERACTIVE. */
    bool show() const;
    std::string_view last_show_status() const noexcept {
        return last_show_status_;
    }

  private:
    std::string theme_;
    int height_;
    std::optional<int> width_;
    std::optional<std::string> template_option_;
    bool show_legend_;
    plotly_color_theme colors_;
    std::string default_template_;
    std::optional<plotly_document> figure_;
    mutable std::string_view last_show_status_ = "NOT_RUN_INTERACTIVE";
};

}  // namespace fincore
