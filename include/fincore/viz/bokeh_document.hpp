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
    enum class kind { line, varea, rect };

    kind type = kind::line;
    std::string x_field;
    std::string y_field;
    std::vector<std::string> x_labels;  // ISO dates for datetime axes
    std::vector<double> x_values;       // numeric x when x_labels is empty
    std::vector<double> y_values;       // NaN -> JSON null
    std::string color;
    double line_width = 2.0;
    std::string line_dash;  // "solid" or "dashed"
    std::string fill_color;
    double fill_alpha = 1.0;
    std::string legend_label;

    // Heatmap rectangle glyph (flattened year x month cells).
    std::vector<std::string> rect_x;
    std::vector<std::string> rect_y;
    std::vector<double> rect_values;  // NaN -> JSON null
    std::string palette;
    bool mapper_range_set = false;
    double mapper_low = 0.0;
    double mapper_high = 0.0;
};

struct bokeh_span {
    double location = 0.0;
    std::string dimension = "width";
    std::string line_color;
    double line_alpha = 1.0;
};

struct bokeh_hover {
    std::vector<std::pair<std::string, std::string>> tooltips;
    std::string mode = "mouse";
    std::map<std::string, std::string> formatters;
};

/**
 * Owning Bokeh-compatible chart document.
 *
 * Emits a self-contained HTML/JSON document with equivalent data, axes,
 * legends and series. BokehJS model identity is not cloned.
 */
struct bokeh_document {
    std::string title;
    std::string x_axis_type = "datetime";
    std::string x_axis_label;
    std::string y_axis_label;
    int height = 500;
    int width = 800;
    std::string sizing_mode = "fixed";
    std::string background_fill_color = "#FFFFFF";
    std::string border_fill_color = "#FFFFFF";
    std::string grid_line_color = "#E0E0E0";
    std::string text_color = "#424242";
    std::string axis_location;
    std::string toolbar_location;
    bool grid_visible = true;
    bool axis_visible = true;
    std::string major_label_text_font_size;
    int major_label_standoff = 0;
    int color_bar_desired_ticks = 10;
    std::vector<bokeh_glyph> glyphs;
    std::vector<bokeh_hover> hovers;
    std::vector<bokeh_span> spans;
    std::string legend_location;
    std::string legend_click_policy;
    std::vector<std::string> x_factors;
    std::vector<std::string> y_factors;
    std::string color_mapper_palette;
    bool mapper_range_set = false;
    double mapper_low = 0.0;
    double mapper_high = 0.0;
    bool color_bar = false;
    std::string color_bar_ticker = "BasicTicker";

    std::string to_json() const;
    /** Deterministic SVG rendering of the document (no third-party asset). */
    std::string to_svg() const;
    std::string to_html() const;
    void save_html(const std::string& path) const;
};

/**
 * Native equivalent of `PrintfTickFormatter`.
 *
 * Supports one printf conversion (`%[flags][width][.precision](f|e|g|d|i)`)
 * plus `%%`. Invalid formats raise `input_error` instead of Python's
 * `ValueError`/`TypeError`.
 */
class printf_tick_formatter {
  public:
    explicit printf_tick_formatter(std::string format = "%.1f");
    std::string format(double value) const;
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
    explicit bokeh_backend(std::string theme = "light", int height = 500,
                           int width = 800,
                           std::string sizing_mode = "fixed");

    const std::string& theme() const noexcept { return theme_; }
    int height() const noexcept { return height_; }
    int width() const noexcept { return width_; }
    const std::string& sizing_mode() const noexcept { return sizing_mode_; }
    const std::string& background_fill_color() const noexcept {
        return background_fill_color_;
    }
    const std::string& grid_line_color() const noexcept {
        return grid_line_color_;
    }
    const std::string& text_color() const noexcept { return text_color_; }

    bokeh_document create_document(std::string title) const;

    bokeh_document plot_returns(const series_chart& chart) const;
    bokeh_document plot_drawdown(const series_chart& chart) const;
    // The recorded `chart.window` wins when non-zero; `window` is the
    // fallback for charts built without a recorded window.
    bokeh_document plot_rolling_sharpe(const series_chart& chart,
                                       std::size_t window = 252) const;
    bokeh_document plot_monthly_heatmap(const heat_grid& grid) const;

    /** Headless: returns false, records NOT_RUN_INTERACTIVE. */
    bool show(const bokeh_document& document) const;
    void save_html(const bokeh_document& document,
                   const std::string& path) const;
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
