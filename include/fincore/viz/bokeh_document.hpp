#pragma once

#include <fincore/viz/chart_data.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/**
 * One owning Bokeh-compatible glyph renderer.
 *
 * Values are copied from native chart data; no kernel is recomputed here.
 * NaN chart values are serialized as JSON `null` (an explicit gap) while the
 * parallel label arrays keep their length.
 */
struct bokeh_glyph {
    /// Supported glyph kinds.
    enum class kind {
        line,   ///< Line glyph.
        varea,  ///< Vertical-area glyph.
        rect    ///< Rectangle heatmap glyph.
    };

    kind type = kind::line;           ///< Glyph kind.
    std::string x_field;              ///< Data field used for x values.
    std::string y_field;              ///< Data field used for y values.
    /// ISO date labels for datetime x axes; takes precedence over x_values.
    std::vector<std::string> x_labels;
    /// Numeric x values used when x_labels is empty.
    std::vector<double> x_values;
    /// Y values; NaN entries serialize as JSON null.
    std::vector<double> y_values;
    std::string color;                ///< Line color.
    double line_width = 2.0;          ///< Line width.
    /// Line dash style, such as "solid" or "dashed".
    std::string line_dash;
    std::string fill_color;           ///< Area fill color.
    double fill_alpha = 1.0;          ///< Area fill opacity.
    std::string legend_label;         ///< Legend entry label.

    // Heatmap rectangle glyph (flattened year x month cells).
    std::vector<std::string> rect_x;      ///< Rectangle x category labels.
    std::vector<std::string> rect_y;      ///< Rectangle y category labels.
    /// Flattened heatmap cell values; NaN entries serialize as JSON null.
    std::vector<double> rect_values;
    std::string palette;                  ///< Named color palette.
    bool mapper_range_set = false;        ///< Whether the mapper range applies.
    double mapper_low = 0.0;              ///< Lower mapper bound when set.
    double mapper_high = 0.0;             ///< Upper mapper bound when set.
};

/// Span annotation drawn across a plot.
struct bokeh_span {
    double location = 0.0;            ///< Span position on its axis.
    std::string dimension = "width";  ///< Span dimension, `width` or `height`.
    std::string line_color;           ///< Span line color.
    double line_alpha = 1.0;          ///< Span line opacity.
};

/// Hover tool configuration for a Bokeh-compatible document.
struct bokeh_hover {
    std::vector<std::pair<std::string, std::string>> tooltips;  ///< Tooltip pairs.
    std::string mode = "mouse";       ///< Hover mode.
    std::map<std::string, std::string> formatters;  ///< Field formatter names.
};

/**
 * Owning Bokeh-compatible chart document.
 *
 * Emits a self-contained HTML/JSON document with equivalent data, axes,
 * legends and series. BokehJS model identity is not cloned.
 */
struct bokeh_document {
    std::string title;                    ///< Document title.
    std::string x_axis_type = "datetime"; ///< X-axis type.
    std::string x_axis_label;             ///< X-axis label.
    std::string y_axis_label;             ///< Y-axis label.
    int height = 500;                     ///< Plot height.
    int width = 800;                      ///< Plot width.
    std::string sizing_mode = "fixed";    ///< Bokeh sizing mode.
    std::string background_fill_color = "#FFFFFF";  ///< Plot background.
    std::string border_fill_color = "#FFFFFF";      ///< Outer background.
    std::string grid_line_color = "#E0E0E0";        ///< Grid line color.
    std::string text_color = "#424242";             ///< Text color.
    std::string axis_location;            ///< Axis location; empty omits it.
    std::string toolbar_location;         ///< Toolbar location; empty omits it.
    bool grid_visible = true;             ///< Whether grid lines are visible.
    bool axis_visible = true;             ///< Whether axes are visible.
    std::string major_label_text_font_size;  ///< Major tick font size.
    int major_label_standoff = 0;         ///< Major tick standoff.
    int color_bar_desired_ticks = 10;     ///< Requested colorbar tick count.
    std::vector<bokeh_glyph> glyphs;      ///< Glyphs in draw order.
    std::vector<bokeh_hover> hovers;      ///< Hover tools.
    std::vector<bokeh_span> spans;        ///< Span annotations.
    std::string legend_location;          ///< Legend location.
    std::string legend_click_policy;      ///< Legend click policy.
    std::vector<std::string> x_factors;   ///< Categorical x factors.
    std::vector<std::string> y_factors;   ///< Categorical y factors.
    std::string color_mapper_palette;     ///< Color mapper palette.
    bool mapper_range_set = false;        ///< Whether the mapper range applies.
    double mapper_low = 0.0;              ///< Lower mapper bound when set.
    double mapper_high = 0.0;             ///< Upper mapper bound when set.
    bool color_bar = false;               ///< Whether a colorbar is emitted.
    std::string color_bar_ticker = "BasicTicker";  ///< Colorbar ticker type.

    /// Serializes the document to deterministic Bokeh-compatible JSON.
    ///
    /// Throws `input_error` when a rect glyph's x, y and value lengths differ,
    /// or a glyph's x and y lengths differ.
    std::string to_json() const;
    /** Deterministic SVG rendering of the document (no third-party asset). */
    std::string to_svg() const;
    /// Returns self-contained HTML with inline SVG and the JSON payload.
    std::string to_html() const;
    /// Writes `to_html()` output to `path`.
    ///
    /// Throws `input_error` for an empty path or a file that cannot be opened
    /// or written.
    void save_html(const std::string& path) const;
};

/**
 * Native equivalent of `PrintfTickFormatter`.
 *
 * Supports one printf conversion — `%[flags][width][.precision]` followed by
 * one of `f`, `e`, `g`, `d` or `i` — plus `%%`. Invalid formats raise
 * `input_error` instead of Python's `ValueError`/`TypeError`.
 */
class printf_tick_formatter {
  public:
    /// Creates a formatter and validates `format`.
    ///
    /// Throws `input_error` for an embedded NUL, unsupported conversions,
    /// incomplete formats, or more than one conversion.
    explicit printf_tick_formatter(std::string format = "%.1f");
    /// Formats `value` with the configured conversion.
    std::string format(double value) const;
    /// Returns the configured format string.
    const std::string& format_string() const noexcept { return format_; }

  private:
    std::string format_;
};

/**
 * Native Bokeh-compatible backend.
 *
 * Mirrors `fincore.viz.interactive.bokeh_backend.BokehBackend` artifact
 * behaviour over native chart data. `show` is headless and returns false.
 */
class bokeh_backend {
  public:
    /// Creates a backend with a theme, size and sizing mode.
    ///
    /// The `dark` theme selects dark fill, grid and text colors; other names
    /// use light colors. Throws `input_error` when `height` or `width` is not
    /// positive.
    explicit bokeh_backend(std::string theme = "light", int height = 500,
                           int width = 800,
                           std::string sizing_mode = "fixed");

    /// Returns the configured theme name.
    const std::string& theme() const noexcept { return theme_; }
    /// Returns the configured plot height.
    int height() const noexcept { return height_; }
    /// Returns the configured plot width.
    int width() const noexcept { return width_; }
    /// Returns the configured sizing mode.
    const std::string& sizing_mode() const noexcept { return sizing_mode_; }
    /// Returns the resolved plot background color.
    const std::string& background_fill_color() const noexcept {
        return background_fill_color_;
    }
    /// Returns the resolved grid line color.
    const std::string& grid_line_color() const noexcept {
        return grid_line_color_;
    }
    /// Returns the resolved text color.
    const std::string& text_color() const noexcept { return text_color_; }

    /// Creates a base document with the configured size, colors and axes.
    bokeh_document create_document(std::string title) const;

    /// Builds a cumulative-returns document.
    ///
    /// Throws `input_error` when the chart has no series or a series label and
    /// value count differ.
    bokeh_document plot_returns(const series_chart& chart) const;
    /// Builds a drawdown document with area and line glyphs.
    ///
    /// Throws `input_error` when the chart has no series or a series label and
    /// value count differ.
    bokeh_document plot_drawdown(const series_chart& chart) const;
    /// Builds a rolling-Sharpe document.
    ///
    /// The recorded `chart.window` wins when non-zero; `window` is the
    /// fallback for charts built without a recorded window. Throws
    /// `input_error` when the chart has no series or lengths differ.
    bokeh_document plot_rolling_sharpe(const series_chart& chart,
                                       std::size_t window = 252) const;
    /// Builds a monthly returns heatmap document.
    ///
    /// Throws `input_error` when there is no year or values are not
    /// `years x 12`.
    bokeh_document plot_monthly_heatmap(const heat_grid& grid) const;

    /** Headless: returns false, records NOT_RUN_INTERACTIVE. */
    bool show(const bokeh_document& document) const;
    /// Writes `document` as self-contained HTML to `path`.
    ///
    /// Throws `input_error` for an empty path or a file that cannot be opened
    /// or written.
    void save_html(const bokeh_document& document,
                   const std::string& path) const;
    /// Returns the status recorded by the most recent `show` call.
    std::string_view last_show_status() const noexcept {
        return last_show_status_;
    }

  private:
    std::string theme_;
    int height_;
    int width_;
    std::string sizing_mode_;
    std::string background_fill_color_;
    std::string grid_line_color_;
    std::string text_color_;
    mutable std::string_view last_show_status_ = "NOT_RUN_INTERACTIVE";
};

}  // namespace fincore
