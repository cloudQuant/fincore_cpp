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
    std::string background;  ///< Plot and paper background color.
    std::string grid;        ///< Grid and zero-line color.
    std::string text;        ///< Title, axis and tick text color.
    std::string positive;    ///< Color for positive markers.
    std::string negative;    ///< Color for negative series.
    std::string neutral;     ///< Color for benchmark or neutral series.
    std::string highlight;   ///< Primary series highlight color.
    std::string warning;     ///< Secondary marker color.
    bool dark = false;       ///< Whether the theme uses dark backgrounds.
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
    /// Supported trace kinds.
    enum class kind {
        scatter,  ///< Scatter or line trace.
        heatmap   ///< Heatmap trace.
    };

    kind type = kind::scatter;    ///< Trace kind.
    std::string name;             ///< Trace name used by the legend.
    std::string mode;             ///< Plotly mode, such as `lines` or `markers`.
    std::string line_color;       ///< Line color; empty omits the color.
    double line_width = 0.0;      ///< Line width; zero omits the line block.
    std::string line_dash;        ///< Line dash style.
    std::string fill;             ///< Fill mode, such as `tozeroy`.
    std::string fill_color;       ///< Fill color.
    double marker_size = 0.0;     ///< Marker size; zero disables markers.
    std::string marker_color;     ///< Marker color.
    std::string marker_symbol;    ///< Marker symbol, such as `star`.
    std::string hovertemplate;    ///< Hover template string.

    // Scatter data.
    /// Date or category labels; takes precedence over x_values.
    std::vector<std::string> x_labels;
    /// Numeric x values used when x_labels is empty.
    std::vector<double> x_values;
    /// Y values; NaN entries serialize as JSON null.
    std::vector<double> y_values;
    /// Per-point custom data; NaN entries serialize as JSON null.
    std::vector<double> custom_data;

    // Heatmap data.
    std::vector<std::string> heatmap_x;  ///< Heatmap column labels.
    std::vector<std::string> heatmap_y;  ///< Heatmap row labels.
    /// Heatmap values in row-major order; NaN entries serialize as JSON null.
    std::vector<double> heatmap_z;
    std::size_t heatmap_columns = 0U;  ///< Number of columns in `heatmap_z`.
    /// Named color scale.
    std::string colorscale;
    /// Custom color scale stops as value/color pairs.
    std::vector<std::pair<double, std::string>> color_stops;
    bool color_range_set = false;  ///< Whether the explicit color range applies.
    double color_min = 0.0;        ///< Lower color bound when set.
    double color_max = 0.0;        ///< Upper color bound when set.
    bool zmid_zero = false;        ///< Whether the color scale is centered at zero.
    std::string colorbar_title;    ///< Colorbar title; empty omits it.
    std::string colorbar_tickformat;  ///< Colorbar tick format.
};

/// Axis configuration for a Plotly-compatible document.
struct plotly_axis {
    std::string title;                    ///< Axis title.
    std::string side;                     ///< Axis side, such as `bottom`.
    bool reversed = false;                ///< Whether the axis range is reversed.
    std::string tickmode;                 ///< Tick mode, such as `array`.
    std::vector<double> tick_values;      ///< Explicit tick positions.
    std::vector<std::string> tick_text;   ///< Explicit tick labels.
};

/// Horizontal or line shape added to the layout.
struct plotly_shape {
    std::string type = "line";       ///< Shape type.
    double y0 = 0.0;                 ///< First y coordinate.
    double y1 = 0.0;                 ///< Second y coordinate.
    std::string yref = "y";          ///< Y reference axis.
    std::string xref = "paper";      ///< X reference coordinate system.
    double x0 = 0.0;                 ///< First x coordinate.
    double x1 = 1.0;                 ///< Second x coordinate.
    std::string line_color;          ///< Shape line color.
    double line_width = 1.0;         ///< Shape line width.
    std::string line_dash;           ///< Shape line dash style.
};

/// Text annotation in layout coordinates.
struct plotly_annotation {
    std::string text;                ///< Annotation text.
    double x = 0.0;                  ///< X position.
    double y = 0.0;                  ///< Y position.
    std::string xref = "paper";      ///< X reference coordinate system.
    std::string yref = "paper";      ///< Y reference coordinate system.
    bool show_arrow = false;         ///< Whether to draw an arrow.
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
    std::string title;                 ///< Document title.
    std::string template_name;         ///< Plotly template name; empty omits it.
    std::optional<int> height = 500;   ///< Layout height; nullopt omits it.
    std::optional<int> width;          ///< Layout width; nullopt omits it.
    bool show_legend = true;           ///< Whether the legend is shown.
    std::string hovermode;             ///< Hover mode; empty omits it.
    std::string font_color;            ///< Font color; empty omits the font block.
    std::string plot_background;       ///< Plot background color.
    std::string paper_background;      ///< Paper background color.
    std::string grid_color;            ///< Grid and zero-line color.
    plotly_axis x_axis;                ///< X-axis configuration.
    plotly_axis y_axis;                ///< Y-axis configuration.
    std::vector<plotly_trace> traces;  ///< Traces in draw order.
    std::vector<plotly_shape> shapes;  ///< Layout shapes.
    std::vector<plotly_annotation> annotations;  ///< Layout annotations.

    /// Serializes the document to deterministic Plotly-compatible JSON.
    ///
    /// Throws `input_error` when a heatmap trace has an invalid
    /// `heatmap_z`/`heatmap_columns` size or its labels do not match the
    /// matrix.
    std::string to_json() const;
    /** Deterministic SVG rendering of the document (no third-party asset). */
    std::string to_svg() const;
    /** Self-contained HTML: inline SVG plus the deterministic JSON payload. */
    std::string to_html() const;
    /// Writes `to_html()` output to `path`.
    ///
    /// Throws `input_error` for an empty path or a file that cannot be opened
    /// or written.
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
    /// Creates a backend with a theme, size, template and legend option.
    ///
    /// Themes `dark` and `plotly_dark` resolve to dark colors and the
    /// `plotly_dark` default template; other names use light colors and
    /// `plotly_white`. Throws `input_error` when `height` or `width` is not
    /// positive.
    explicit plotly_backend(std::string theme = "light", int height = 500,
                            std::optional<int> width = std::nullopt,
                            std::optional<std::string> template_name = std::nullopt,
                            bool show_legend = true);

    /// Returns the configured theme name.
    const std::string& theme() const noexcept { return theme_; }
    /// Returns the resolved theme colors.
    const plotly_color_theme& colors() const noexcept { return colors_; }
    /// Returns the theme-derived default template name.
    const std::string& default_template() const noexcept {
        return default_template_;
    }
    /// Returns the explicit template, or the theme default when unset.
    const std::string& effective_template() const noexcept;
    /// Returns the configured figure height.
    int height() const noexcept { return height_; }
    /// Returns the configured figure width, if any.
    const std::optional<int>& width() const noexcept { return width_; }
    /// Returns whether the document legend is enabled.
    bool show_legend() const noexcept { return show_legend_; }

    /** Base document with the configured template, size and theme colors. */
    plotly_document create_document(std::string title) const;

    /// Builds and records a cumulative-returns figure.
    ///
    /// Throws `input_error` when the chart has no series or a series label and
    /// value count differ.
    plotly_document plot_returns(const series_chart& chart);
    /// Builds and records a drawdown figure with filled series.
    ///
    /// Throws `input_error` when the chart has no series or a series label and
    /// value count differ.
    plotly_document plot_drawdown(const series_chart& chart);
    /// Builds and records a rolling-Sharpe figure.
    ///
    /// The recorded `chart.window` wins when non-zero; `window` is the
    /// fallback for charts built without a recorded window. Throws
    /// `input_error` when the chart has no series or lengths differ.
    plotly_document plot_rolling_sharpe(const series_chart& chart,
                                        std::size_t window = 252);
    /// Builds and records a monthly returns heatmap.
    ///
    /// Throws `input_error` when there is no year or values are not
    /// `years x 12`.
    plotly_document plot_monthly_heatmap(const heat_grid& grid);
    /// Builds and records a correlation heatmap.
    ///
    /// Throws `input_error` when labels are empty or values are not
    /// `labels x labels`.
    plotly_document plot_correlation_matrix(const correlation_matrix& matrix);
    /// Builds and records an efficient frontier figure.
    ///
    /// Throws `input_error` when volatilities and returns differ in length.
    plotly_document plot_efficient_frontier(const frontier_chart_data& frontier);

    /** Record an externally assembled document as the current figure. */
    void set_figure(plotly_document document);
    /// Returns whether a figure has been recorded.
    bool has_figure() const noexcept { return figure_.has_value(); }
    /** Throws `input_error` when no plot method ran yet. */
    const plotly_document& figure() const;

    /// Writes the recorded figure as self-contained HTML.
    ///
    /// Throws `input_error` when no figure exists or the path cannot be
    /// written.
    void save_html(const std::string& path) const;
    /// Writes the recorded figure as SVG.
    ///
    /// Throws `input_error` when no figure exists or the path cannot be
    /// written.
    void save_image(const std::string& path) const;
    /** Headless: returns false, records NOT_RUN_INTERACTIVE. */
    bool show() const;
    /// Returns the status recorded by the most recent `show` call.
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
