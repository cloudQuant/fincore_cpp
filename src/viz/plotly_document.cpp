#include <fincore/viz/plotly_document.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

constexpr std::string_view plotly_positive = "#00C853";
constexpr std::string_view plotly_negative = "#D50000";
constexpr std::string_view plotly_neutral = "#212121";
constexpr std::string_view plotly_highlight = "#2962FF";
constexpr std::string_view plotly_warning = "#FF6D00";
constexpr std::string_view plotly_background_light = "#FFFFFF";
constexpr std::string_view plotly_background_dark = "#1E1E1E";
constexpr std::string_view plotly_grid_light = "#E0E0E0";
constexpr std::string_view plotly_grid_dark = "#424242";
constexpr std::string_view plotly_text_light = "#424242";
constexpr std::string_view plotly_text_dark = "#E0E0E0";

// SVG geometry has no way to express a gap: a non-finite coordinate would
// invalidate the whole path, so it collapses to zero. The JSON channels in this
// file do the opposite and write null, because a trace can hold a gap without
// breaking the figure.
std::string svg_number(const double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    char buffer[64];
    const auto result =
        std::to_chars(buffer, buffer + sizeof(buffer), value,
                      std::chars_format::fixed, 2);
    if (result.ec != std::errc{}) {
        return "0";
    }
    return std::string(buffer, result.ptr);
}

std::string escape_xml(const std::string_view text) {
    std::string output;
    output.reserve(text.size());
    for (const char character : text) {
        switch (character) {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            case '"':
                output += "&quot;";
                break;
            case '\'':
                output += "&#39;";
                break;
            default:
                output.push_back(character);
                break;
        }
    }
    return output;
}

void write_string_array(detail::json_writer& writer,
                        const std::vector<std::string>& values) {
    writer.begin_array();
    for (const auto& value : values) {
        writer.value_string(value);
    }
    writer.end_array();
}

void write_number_array(detail::json_writer& writer,
                        const std::vector<double>& values,
                        const std::string_view context) {
    writer.begin_array();
    for (const double value : values) {
        if (std::isfinite(value)) {
            writer.value_number(value, context);
        } else {
            writer.value_null();
        }
    }
    writer.end_array();
}

void write_axis(detail::json_writer& writer, const plotly_axis& axis,
                const std::string_view grid_color) {
    writer.begin_object();
    writer.key("title");
    writer.begin_object();
    writer.key("text");
    writer.value_string(axis.title);
    writer.end_object();
    writer.key("gridcolor");
    writer.value_string(grid_color);
    writer.key("zerolinecolor");
    writer.value_string(grid_color);
    if (!axis.side.empty()) {
        writer.key("side");
        writer.value_string(axis.side);
    }
    if (axis.reversed) {
        writer.key("autorange");
        writer.value_string("reversed");
    }
    if (!axis.tickmode.empty()) {
        writer.key("tickmode");
        writer.value_string(axis.tickmode);
    }
    if (!axis.tick_values.empty()) {
        writer.key("tickvals");
        write_number_array(writer, axis.tick_values, "plotly tick value");
    }
    if (!axis.tick_text.empty()) {
        writer.key("ticktext");
        write_string_array(writer, axis.tick_text);
    }
    writer.end_object();
}

void write_trace(detail::json_writer& writer, const plotly_trace& trace) {
    writer.begin_object();
    writer.key("type");
    writer.value_string(trace.type == plotly_trace::kind::heatmap
                            ? "heatmap"
                            : "scatter");
    if (!trace.name.empty()) {
        writer.key("name");
        writer.value_string(trace.name);
    }

    if (trace.type == plotly_trace::kind::scatter) {
        if (!trace.mode.empty()) {
            writer.key("mode");
            writer.value_string(trace.mode);
        }
        if (!trace.line_color.empty() || trace.line_width > 0.0 ||
            !trace.line_dash.empty()) {
            writer.key("line");
            writer.begin_object();
            if (!trace.line_color.empty()) {
                writer.key("color");
                writer.value_string(trace.line_color);
            }
            if (trace.line_width > 0.0) {
                writer.key("width");
                writer.value_number(trace.line_width, "plotly line width");
            }
            if (!trace.line_dash.empty()) {
                writer.key("dash");
                writer.value_string(trace.line_dash);
            }
            writer.end_object();
        }
        if (trace.marker_size > 0.0 || !trace.marker_color.empty() ||
            !trace.marker_symbol.empty()) {
            writer.key("marker");
            writer.begin_object();
            if (trace.marker_size > 0.0) {
                writer.key("size");
                writer.value_number(trace.marker_size, "plotly marker size");
            }
            if (!trace.marker_color.empty()) {
                writer.key("color");
                writer.value_string(trace.marker_color);
            }
            if (!trace.marker_symbol.empty()) {
                writer.key("symbol");
                writer.value_string(trace.marker_symbol);
            }
            writer.end_object();
        }
        if (!trace.fill.empty()) {
            writer.key("fill");
            writer.value_string(trace.fill);
        }
        if (!trace.fill_color.empty()) {
            writer.key("fillcolor");
            writer.value_string(trace.fill_color);
        }
        if (!trace.x_labels.empty()) {
            writer.key("x");
            write_string_array(writer, trace.x_labels);
        } else if (!trace.x_values.empty()) {
            writer.key("x");
            write_number_array(writer, trace.x_values, "plotly x");
        }
        writer.key("y");
        write_number_array(writer, trace.y_values, "plotly y");
        if (!trace.custom_data.empty()) {
            writer.key("customdata");
            write_number_array(writer, trace.custom_data,
                               "plotly customdata");
        }
        if (!trace.hovertemplate.empty()) {
            writer.key("hovertemplate");
            writer.value_string(trace.hovertemplate);
        }
    } else {
        writer.key("x");
        write_string_array(writer, trace.heatmap_x);
        writer.key("y");
        write_string_array(writer, trace.heatmap_y);
        if (trace.heatmap_columns == 0U ||
            trace.heatmap_z.size() % trace.heatmap_columns != 0U) {
            throw input_error{
                "plotly heatmap z must be a rows x columns matrix"};
        }
        if (trace.heatmap_y.size() * trace.heatmap_columns !=
            trace.heatmap_z.size()) {
            throw input_error{
                "plotly heatmap labels must match the z matrix"};
        }
        writer.key("z");
        writer.begin_array();
        for (std::size_t row = 0U; row < trace.heatmap_y.size(); ++row) {
            writer.begin_array();
            for (std::size_t column = 0U;
                 column < trace.heatmap_columns; ++column) {
                const double value =
                    trace.heatmap_z[row * trace.heatmap_columns + column];
                if (std::isfinite(value)) {
                    writer.value_number(value, "plotly heatmap z");
                } else {
                    writer.value_null();
                }
            }
            writer.end_array();
        }
        writer.end_array();
        if (!trace.color_stops.empty()) {
            writer.key("colorscale");
            writer.begin_array();
            for (const auto& [position, color] : trace.color_stops) {
                writer.begin_array();
                writer.value_number(position, "plotly colorscale stop");
                writer.value_string(color);
                writer.end_array();
            }
            writer.end_array();
        } else if (!trace.colorscale.empty()) {
            writer.key("colorscale");
            writer.value_string(trace.colorscale);
        }
        if (trace.zmid_zero) {
            writer.key("zmid");
            writer.value_number(0.0, "plotly zmid");
        }
        if (trace.color_range_set && std::isfinite(trace.color_min) &&
            std::isfinite(trace.color_max)) {
            writer.key("zmin");
            writer.value_number(trace.color_min, "plotly zmin");
            writer.key("zmax");
            writer.value_number(trace.color_max, "plotly zmax");
        }
        if (!trace.colorbar_title.empty() ||
            !trace.colorbar_tickformat.empty()) {
            writer.key("colorbar");
            writer.begin_object();
            if (!trace.colorbar_title.empty()) {
                writer.key("title");
                writer.begin_object();
                writer.key("text");
                writer.value_string(trace.colorbar_title);
                writer.end_object();
            }
            if (!trace.colorbar_tickformat.empty()) {
                writer.key("tickformat");
                writer.value_string(trace.colorbar_tickformat);
            }
            writer.end_object();
        }
        if (!trace.hovertemplate.empty()) {
            writer.key("hovertemplate");
            writer.value_string(trace.hovertemplate);
        }
    }
    writer.end_object();
}

std::vector<std::string> date_labels(const std::vector<timestamp>& index) {
    std::vector<std::string> labels;
    labels.reserve(index.size());
    for (const timestamp value : index) {
        labels.push_back(detail::format_timestamp(value));
    }
    return labels;
}

void require_series(const series_chart& chart, const std::string_view context) {
    if (chart.series.empty()) {
        throw input_error{std::string(context) +
                          " requires at least one chart series"};
    }
}

plotly_trace scatter_trace(const chart_series& series) {
    plotly_trace trace;
    trace.type = plotly_trace::kind::scatter;
    trace.name = series.label;
    trace.mode = "lines";
    trace.x_labels = date_labels(series.index);
    trace.y_values = series.values;
    if (trace.x_labels.size() != trace.y_values.size()) {
        throw input_error{
            "plotly series labels and values must have equal lengths"};
    }
    return trace;
}

struct color_rgb {
    int red = 0;
    int green = 0;
    int blue = 0;
};

color_rgb parse_hex_color(const std::string_view color) {
    if (color.size() != 7U || color.front() != '#') {
        return {128, 128, 128};
    }
    const auto component = [&color](const std::size_t offset) {
        unsigned value = 0U;
        const auto result =
            std::from_chars(color.data() + offset,
                            color.data() + offset + 2U, value, 16);
        if (result.ec != std::errc{}) {
            return 128;
        }
        return static_cast<int>(value);
    };
    return {component(1U), component(3U), component(5U)};
}

std::string rgb_text(const color_rgb& color) {
    return "rgb(" + std::to_string(color.red) + "," +
           std::to_string(color.green) + "," +
           std::to_string(color.blue) + ")";
}

std::string interpolate_color(
    const std::vector<std::pair<double, std::string>>& stops,
    const double normalized) {
    if (stops.empty()) {
        return "#808080";
    }
    const double clamped = std::clamp(normalized, 0.0, 1.0);
    if (clamped <= stops.front().first) {
        return stops.front().second;
    }
    if (clamped >= stops.back().first) {
        return stops.back().second;
    }
    for (std::size_t position = 1U; position < stops.size(); ++position) {
        const auto& [upper_position, upper_color] = stops[position];
        if (clamped <= upper_position) {
            const auto& [lower_position, lower_color] = stops[position - 1U];
            const double span = upper_position - lower_position;
            const double weight = span <= 0.0
                                      ? 1.0
                                      : (clamped - lower_position) / span;
            const auto lower = parse_hex_color(lower_color);
            const auto upper = parse_hex_color(upper_color);
            const auto mix = [weight](const int first, const int second) {
                return static_cast<int>(std::lround(
                    first + (second - first) * weight));
            };
            return rgb_text({mix(lower.red, upper.red),
                             mix(lower.green, upper.green),
                             mix(lower.blue, upper.blue)});
        }
    }
    return stops.back().second;
}

std::vector<std::pair<double, std::string>> heat_colorscale(
    const plotly_trace& trace) {
    if (!trace.color_stops.empty()) {
        return trace.color_stops;
    }
    if (trace.colorscale == "RdBu") {
        return {{0.0, "#67001f"}, {0.5, "#f7f7f7"}, {1.0, "#053061"}};
    }
    return {{0.0, "#D32F2F"}, {0.5, "#FFFFFF"}, {1.0, "#388E3C"}};
}

void append_axes_frame(std::string& svg, const double left,
                       const double right, const double top,
                       const double bottom, const std::string& grid) {
    svg += "<rect x=\"" + svg_number(left) + "\" y=\"" + svg_number(top) +
           "\" width=\"" + svg_number(right - left) + "\" height=\"" +
           svg_number(bottom - top) +
           "\" fill=\"none\" stroke=\"" + escape_xml(grid) + "\" "
           "stroke-width=\"1\"/>";
    for (int step = 1; step < 5; ++step) {
        const double y =
            top + (bottom - top) * static_cast<double>(step) / 5.0;
        svg += "<line x1=\"" + svg_number(left) + "\" y1=\"" +
               svg_number(y) + "\" x2=\"" + svg_number(right) + "\" y2=\"" +
               svg_number(y) + "\" stroke=\"" + escape_xml(grid) +
               "\" stroke-width=\"0.5\" stroke-dasharray=\"2,4\"/>";
    }
}

void append_polyline(std::string& svg, const std::vector<double>& x,
                     const std::vector<double>& y,
                     const std::string& color, const double width,
                     const std::string& dash) {
    std::string points;
    bool first = true;
    for (std::size_t position = 0U; position < y.size(); ++position) {
        if (!std::isfinite(y[position])) {
            continue;
        }
        if (!first) {
            points.push_back(' ');
        }
        points += svg_number(x[position]) + "," + svg_number(y[position]);
        first = false;
    }
    if (points.empty()) {
        return;
    }
    svg += "<polyline fill=\"none\" stroke=\"" + escape_xml(color) +
           "\" stroke-width=\"" + svg_number(width) + "\"";
    if (!dash.empty() && dash != "solid") {
        svg += " stroke-dasharray=\"6,4\"";
    }
    svg += " points=\"" + points + "\"/>";
}

}  // namespace

std::string plotly_document::to_json() const {
    std::string output;
    detail::json_writer writer(output);
    writer.begin_object();
    writer.key("data");
    writer.begin_array();
    for (const auto& trace : traces) {
        write_trace(writer, trace);
    }
    writer.end_array();
    writer.key("layout");
    writer.begin_object();
    writer.key("title");
    writer.value_string(title);
    if (!template_name.empty()) {
        writer.key("template");
        writer.value_string(template_name);
    }
    if (height.has_value()) {
        writer.key("height");
        writer.value_integer(static_cast<std::int64_t>(*height));
    }
    if (width.has_value()) {
        writer.key("width");
        writer.value_integer(static_cast<std::int64_t>(*width));
    }
    writer.key("showlegend");
    writer.value_boolean(show_legend);
    if (!hovermode.empty()) {
        writer.key("hovermode");
        writer.value_string(hovermode);
    }
    if (!font_color.empty()) {
        writer.key("font");
        writer.begin_object();
        writer.key("color");
        writer.value_string(font_color);
        writer.end_object();
    }
    writer.key("plot_bgcolor");
    writer.value_string(plot_background);
    writer.key("paper_bgcolor");
    writer.value_string(paper_background);
    writer.key("xaxis");
    write_axis(writer, x_axis, grid_color);
    writer.key("yaxis");
    write_axis(writer, y_axis, grid_color);
    writer.key("shapes");
    writer.begin_array();
    for (const auto& shape : shapes) {
        writer.begin_object();
        writer.key("type");
        writer.value_string(shape.type);
        writer.key("x0");
        writer.value_number(shape.x0, "plotly shape x0");
        writer.key("x1");
        writer.value_number(shape.x1, "plotly shape x1");
        writer.key("y0");
        writer.value_number(shape.y0, "plotly shape y0");
        writer.key("y1");
        writer.value_number(shape.y1, "plotly shape y1");
        writer.key("xref");
        writer.value_string(shape.xref);
        writer.key("yref");
        writer.value_string(shape.yref);
        writer.key("line");
        writer.begin_object();
        writer.key("color");
        writer.value_string(shape.line_color);
        writer.key("width");
        writer.value_number(shape.line_width, "plotly shape line width");
        writer.key("dash");
        writer.value_string(shape.line_dash);
        writer.end_object();
        writer.end_object();
    }
    writer.end_array();
    writer.key("annotations");
    writer.begin_array();
    for (const auto& annotation : annotations) {
        writer.begin_object();
        writer.key("text");
        writer.value_string(annotation.text);
        writer.key("x");
        writer.value_number(annotation.x, "plotly annotation x");
        writer.key("y");
        writer.value_number(annotation.y, "plotly annotation y");
        writer.key("xref");
        writer.value_string(annotation.xref);
        writer.key("yref");
        writer.value_string(annotation.yref);
        writer.key("showarrow");
        writer.value_boolean(annotation.show_arrow);
        writer.end_object();
    }
    writer.end_array();
    writer.end_object();
    writer.end_object();
    output.push_back('\n');
    return output;
}

std::string plotly_document::to_svg() const {
    const double canvas_width =
        static_cast<double>(std::max(width.value_or(800), 320));
    const double canvas_height =
        static_cast<double>(std::max(height.value_or(500), 240));
    const double left = 70.0;
    const double right = canvas_width - 30.0;
    const double top = 60.0;
    const double bottom = canvas_height - 60.0;

    std::string svg;
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
           svg_number(canvas_width) + "\" height=\"" +
           svg_number(canvas_height) + "\" viewBox=\"0 0 " +
           svg_number(canvas_width) + " " + svg_number(canvas_height) +
           "\" role=\"img\">";
    svg += "<title>" + escape_xml(title) + "</title>";
    svg += "<rect width=\"" + svg_number(canvas_width) + "\" height=\"" +
           svg_number(canvas_height) + "\" fill=\"" +
           escape_xml(paper_background) + "\"/>";
    if (!title.empty()) {
        svg += "<text x=\"" + svg_number(canvas_width / 2.0) +
               "\" y=\"30\" text-anchor=\"middle\" font-family=\"sans-serif\" "
               "font-size=\"16\" font-weight=\"bold\" fill=\"" +
               escape_xml(font_color) + "\">" + escape_xml(title) +
               "</text>";
    }
    append_axes_frame(svg, left, right, top, bottom, grid_color);

    // Collect y extents across scatter traces and shapes.
    double minimum_y = std::numeric_limits<double>::infinity();
    double maximum_y = -std::numeric_limits<double>::infinity();
    bool any_y = false;
    bool include_zero = false;
    bool index_x = false;
    double minimum_x = std::numeric_limits<double>::infinity();
    double maximum_x = -std::numeric_limits<double>::infinity();
    bool numeric_x = false;
    std::size_t maximum_points = 0U;
    for (const auto& trace : traces) {
        if (trace.type != plotly_trace::kind::scatter) {
            continue;
        }
        maximum_points = std::max(maximum_points, trace.y_values.size());
        if (!trace.x_labels.empty()) {
            index_x = true;
        } else if (!trace.x_values.empty()) {
            numeric_x = true;
            for (const double value : trace.x_values) {
                if (std::isfinite(value)) {
                    minimum_x = std::min(minimum_x, value);
                    maximum_x = std::max(maximum_x, value);
                }
            }
        }
        for (const double value : trace.y_values) {
            if (!std::isfinite(value)) {
                continue;
            }
            minimum_y = std::min(minimum_y, value);
            maximum_y = std::max(maximum_y, value);
            any_y = true;
        }
        if (trace.fill == "tozeroy") {
            include_zero = true;
        }
    }
    for (const auto& shape : shapes) {
        minimum_y = std::min({minimum_y, shape.y0, shape.y1});
        maximum_y = std::max({maximum_y, shape.y0, shape.y1});
        any_y = true;
    }
    if (!any_y) {
        minimum_y = 0.0;
        maximum_y = 1.0;
    }
    if (include_zero) {
        minimum_y = std::min(minimum_y, 0.0);
        maximum_y = std::max(maximum_y, 0.0);
    }
    if (maximum_y - minimum_y < 1e-12) {
        minimum_y -= 1.0;
        maximum_y += 1.0;
    }
    const double span_y = maximum_y - minimum_y;
    if (index_x) {
        numeric_x = false;
    } else if (!numeric_x || maximum_x - minimum_x < 1e-12) {
        numeric_x = false;
        minimum_x = 0.0;
        maximum_x = static_cast<double>(
            maximum_points == 0U ? 1U : maximum_points - 1U);
        if (maximum_x - minimum_x < 1e-12) {
            maximum_x = minimum_x + 1.0;
        }
    }
    const double span_x = maximum_x - minimum_x;
    const auto map_y = [&](const double value) {
        return bottom - (value - minimum_y) / span_y * (bottom - top);
    };
    const auto map_x = [&](const double value) {
        return left + (value - minimum_x) / span_x * (right - left);
    };

    // Heatmap cells first so lines draw on top.
    for (const auto& trace : traces) {
        if (trace.type != plotly_trace::kind::heatmap) {
            continue;
        }
        if (trace.heatmap_columns == 0U) {
            continue;
        }
        const std::size_t rows =
            trace.heatmap_z.size() / trace.heatmap_columns;
        double heat_min = std::numeric_limits<double>::infinity();
        double heat_max = -std::numeric_limits<double>::infinity();
        for (const double value : trace.heatmap_z) {
            if (std::isfinite(value)) {
                heat_min = std::min(heat_min, value);
                heat_max = std::max(heat_max, value);
            }
        }
        if (!std::isfinite(heat_min) || !std::isfinite(heat_max)) {
            heat_min = -1.0;
            heat_max = 1.0;
        }
        if (trace.color_range_set) {
            heat_min = trace.color_min;
            heat_max = trace.color_max;
        }
        if (heat_max - heat_min < 1e-12) {
            heat_max = heat_min + 1.0;
        }
        const auto stops = heat_colorscale(trace);
        const double cell_width =
            (right - left) /
            static_cast<double>(std::max<std::size_t>(
                trace.heatmap_columns, 1U));
        const double cell_height =
            (bottom - top) / static_cast<double>(std::max<std::size_t>(rows, 1U));
        for (std::size_t row = 0U; row < rows; ++row) {
            for (std::size_t column = 0U;
                 column < trace.heatmap_columns; ++column) {
                const double value =
                    trace.heatmap_z[row * trace.heatmap_columns + column];
                if (!std::isfinite(value)) {
                    continue;
                }
                const std::string color = interpolate_color(
                    stops, (value - heat_min) / (heat_max - heat_min));
                svg += "<rect x=\"" +
                       svg_number(left +
                                  static_cast<double>(column) * cell_width) +
                       "\" y=\"" +
                       svg_number(top + static_cast<double>(row) * cell_height) +
                       "\" width=\"" + svg_number(cell_width) +
                       "\" height=\"" + svg_number(cell_height) +
                       "\" fill=\"" + color + "\"/>";
            }
        }
    }

    for (const auto& shape : shapes) {
        const double y = map_y(shape.y0);
        svg += "<line x1=\"" + svg_number(left) + "\" y1=\"" +
               svg_number(y) + "\" x2=\"" + svg_number(right) + "\" y2=\"" +
               svg_number(y) + "\" stroke=\"" + escape_xml(shape.line_color) +
               "\" stroke-width=\"" + svg_number(shape.line_width) + "\"";
        if (!shape.line_dash.empty() && shape.line_dash != "solid") {
            svg += " stroke-dasharray=\"" +
                   std::string(shape.line_dash == "dot" ? "2,3" : "6,4") + "\"";
        }
        svg += "/>";
    }

    for (const auto& trace : traces) {
        if (trace.type != plotly_trace::kind::scatter ||
            trace.y_values.empty()) {
            continue;
        }
        std::vector<double> xs;
        xs.reserve(trace.y_values.size());
        for (std::size_t position = 0U; position < trace.y_values.size();
             ++position) {
            const double raw_x =
                numeric_x && trace.x_values.size() == trace.y_values.size()
                    ? trace.x_values[position]
                    : static_cast<double>(position);
            xs.push_back(map_x(raw_x));
        }
        std::vector<double> ys;
        ys.reserve(trace.y_values.size());
        for (const double value : trace.y_values) {
            ys.push_back(std::isfinite(value) ? map_y(value) : nan);
        }
        if (trace.fill == "tozeroy") {
            const double baseline = map_y(0.0);
            std::string points;
            for (std::size_t position = 0U; position < ys.size();
                 ++position) {
                if (!std::isfinite(ys[position])) {
                    continue;
                }
                if (!points.empty()) {
                    points.push_back(' ');
                }
                points += svg_number(xs[position]) + "," +
                          svg_number(ys[position]);
            }
            if (!points.empty()) {
                const std::string fill_color =
                    trace.fill_color.empty() ? trace.line_color
                                             : trace.fill_color;
                svg += "<polygon fill=\"" + escape_xml(fill_color) +
                       "\" stroke=\"none\" points=\"" + points + " " +
                       svg_number(xs.back()) + "," + svg_number(baseline) +
                       " " + svg_number(xs.front()) + "," +
                       svg_number(baseline) + "\"/>";
            }
        }
        append_polyline(svg, xs, ys, trace.line_color,
                        trace.line_width > 0.0 ? trace.line_width : 2.0,
                        trace.line_dash);
        if (trace.marker_size > 0.0) {
            for (std::size_t position = 0U; position < ys.size();
                 ++position) {
                if (!std::isfinite(ys[position])) {
                    continue;
                }
                if (trace.marker_symbol == "diamond") {
                    const double radius = trace.marker_size / 2.0;
                    svg += "<polygon fill=\"" +
                           escape_xml(trace.marker_color) +
                           "\" stroke=\"none\" points=\"" +
                           svg_number(xs[position]) + "," +
                           svg_number(ys[position] - radius) + " " +
                           svg_number(xs[position] + radius) + "," +
                           svg_number(ys[position]) + " " +
                           svg_number(xs[position]) + "," +
                           svg_number(ys[position] + radius) + " " +
                           svg_number(xs[position] - radius) + "," +
                           svg_number(ys[position]) + "\"/>";
                } else {
                    svg += "<circle cx=\"" + svg_number(xs[position]) +
                           "\" cy=\"" + svg_number(ys[position]) +
                           "\" r=\"" +
                           svg_number(trace.marker_size / 2.0) +
                           "\" fill=\"" + escape_xml(trace.marker_color) +
                           "\"/>";
                }
            }
        }
    }

    // Minimal axis tick captions (deterministic, values only).
    for (int step = 0; step <= 4; ++step) {
        const double value =
            minimum_y + span_y * static_cast<double>(step) / 4.0;
        const double y = map_y(value);
        svg += "<text x=\"" + svg_number(left - 8.0) + "\" y=\"" +
               svg_number(y + 3.0) +
               "\" text-anchor=\"end\" font-family=\"sans-serif\" "
               "font-size=\"9\" fill=\"" +
               escape_xml(font_color) + "\">" + svg_number(value) +
               "</text>";
    }
    svg += "</svg>";
    return svg;
}

std::string plotly_document::to_html() const {
    std::string json = to_json();
    std::string escaped_json;
    escaped_json.reserve(json.size());
    for (std::size_t position = 0U; position < json.size(); ++position) {
        if (json[position] == '<' && position + 1U < json.size() &&
            json[position + 1U] == '/') {
            escaped_json += "<\\/";
            ++position;
        } else {
            escaped_json.push_back(json[position]);
        }
    }
    std::string html;
    html += "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n";
    html += "<title>" + escape_xml(title) + "</title>\n";
    html += "<style>\n";
    html += "body { font-family: -apple-system, BlinkMacSystemFont, "
            "\"Segoe UI\", sans-serif; margin: 1.5rem auto; max-width: "
            "1100px; color: #1f2937; }\n";
    html += ".fincore-plotly-chart { border: 1px solid #e5e7eb; "
            "border-radius: 8px; overflow: hidden; }\n";
    html += "svg { display: block; width: 100%; height: auto; }\n";
    html += "</style>\n</head>\n<body data-backend=\"plotly\">\n";
    html += "<div class=\"fincore-plotly-chart\">\n";
    html += to_svg();
    html += "\n</div>\n";
    html += "<script type=\"application/json\" "
            "id=\"fincore-plotly-document\">" +
            escaped_json + "</script>\n";
    html += "</body></html>\n";
    return html;
}

void plotly_document::save_html(const std::string& path) const {
    if (path.empty()) {
        throw input_error{"plotly save_html path must not be empty"};
    }
    const std::string html = to_html();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw input_error{"plotly save_html could not open: " + path};
    }
    stream.write(html.data(), static_cast<std::streamsize>(html.size()));
    if (!stream) {
        throw input_error{"plotly save_html could not write: " + path};
    }
}

void plotly_document::save_image(const std::string& path) const {
    if (path.empty()) {
        throw input_error{"plotly save_image path must not be empty"};
    }
    const std::string svg = to_svg();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw input_error{"plotly save_image could not open: " + path};
    }
    stream.write(svg.data(), static_cast<std::streamsize>(svg.size()));
    if (!stream) {
        throw input_error{"plotly save_image could not write: " + path};
    }
}

plotly_backend::plotly_backend(std::string theme, const int height,
                               std::optional<int> width,
                               std::optional<std::string> template_name,
                               const bool show_legend)
    : theme_(std::move(theme)),
      height_(height),
      width_(width),
      template_option_(std::move(template_name)),
      show_legend_(show_legend) {
    if (height_ <= 0) {
        throw input_error{"plotly height must be positive"};
    }
    if (width_.has_value() && *width_ <= 0) {
        throw input_error{"plotly width must be positive"};
    }
    const bool dark = theme_ == "dark" || theme_ == "plotly_dark";
    colors_ = plotly_color_theme{
        std::string(dark ? plotly_background_dark
                         : plotly_background_light),
        std::string(dark ? plotly_grid_dark : plotly_grid_light),
        std::string(dark ? plotly_text_dark : plotly_text_light),
        std::string(plotly_positive),
        std::string(plotly_negative),
        std::string(plotly_neutral),
        std::string(plotly_highlight),
        std::string(plotly_warning),
        dark};
    default_template_ = dark ? "plotly_dark" : "plotly_white";
}

const std::string& plotly_backend::effective_template() const noexcept {
    return template_option_.has_value() ? *template_option_ : default_template_;
}

plotly_document plotly_backend::create_document(std::string title) const {
    plotly_document document;
    document.title = std::move(title);
    document.template_name = effective_template();
    document.height = height_;
    document.width = width_;
    document.show_legend = show_legend_;
    document.font_color = colors_.text;
    document.plot_background = colors_.background;
    document.paper_background = colors_.background;
    document.grid_color = colors_.grid;
    return document;
}

plotly_document plotly_backend::plot_returns(const series_chart& chart) {
    require_series(chart, "plot_returns");
    plotly_document document = create_document("Cumulative Returns");
    document.x_axis.title = "Date";
    document.y_axis.title = "Cumulative Return";
    document.hovermode = "x unified";
    for (const auto& series : chart.series) {
        plotly_trace trace = scatter_trace(series);
        if (trace.name.empty()) {
            trace.name = series.benchmark ? "Benchmark" : "Portfolio";
        }
        trace.line_color = series.benchmark ? colors_.neutral
                                           : colors_.highlight;
        trace.line_width = 2.0;
        trace.line_dash = series.dashed ? "dash" : "solid";
        trace.hovertemplate =
            "%{x}<br>Return: %{y:.2%}<extra></extra>";
        document.traces.push_back(std::move(trace));
    }
    plotly_shape zero;
    zero.y0 = 0.0;
    zero.y1 = 0.0;
    zero.line_color = colors_.grid;
    zero.line_width = 1.0;
    zero.line_dash = "dot";
    document.shapes.push_back(std::move(zero));
    figure_ = document;
    return document;
}

plotly_document plotly_backend::plot_drawdown(const series_chart& chart) {
    require_series(chart, "plot_drawdown");
    plotly_document document =
        create_document("Drawdown (Underwater Chart)");
    document.x_axis.title = "Date";
    document.y_axis.title = "Drawdown";
    document.hovermode = "x unified";
    for (const auto& series : chart.series) {
        plotly_trace trace = scatter_trace(series);
        if (trace.name.empty()) {
            trace.name = "Drawdown";
        }
        trace.line_color = colors_.negative;
        trace.line_width = 1.5;
        trace.fill = "tozeroy";
        trace.fill_color = "rgba(213, 0, 0, 0.3)";
        trace.hovertemplate =
            "%{x}<br>Drawdown: %{y:.2%}<extra></extra>";
        document.traces.push_back(std::move(trace));
    }
    figure_ = document;
    return document;
}

plotly_document plotly_backend::plot_rolling_sharpe(const series_chart& chart,
                                                    const std::size_t window) {
    require_series(chart, "plot_rolling_sharpe");
    // Chart data carries the window it was computed with; the explicit
    // argument is only a fallback for charts without a recorded window.
    const std::size_t effective_window =
        chart.window != 0U ? chart.window : (window == 0U ? 252U : window);
    plotly_document document = create_document(
        "Rolling Sharpe Ratio (" + std::to_string(effective_window) +
        "-day window)");
    document.x_axis.title = "Date";
    document.y_axis.title = "Sharpe Ratio";
    document.hovermode = "x unified";
    for (const auto& series : chart.series) {
        plotly_trace trace = scatter_trace(series);
        if (trace.name.empty()) {
            trace.name =
                series.benchmark ? "Benchmark Sharpe" : "Portfolio Sharpe";
        }
        trace.line_color =
            series.benchmark ? colors_.neutral : colors_.highlight;
        trace.line_width = 2.0;
        trace.line_dash = series.dashed ? "dash" : "solid";
        trace.hovertemplate = "%{x}<br>Sharpe: %{y:.2f}<extra></extra>";
        document.traces.push_back(std::move(trace));
    }
    plotly_shape zero;
    zero.y0 = 0.0;
    zero.y1 = 0.0;
    zero.line_color = colors_.grid;
    zero.line_width = 1.0;
    zero.line_dash = "solid";
    document.shapes.push_back(std::move(zero));
    figure_ = document;
    return document;
}

plotly_document plotly_backend::plot_monthly_heatmap(const heat_grid& grid) {
    if (grid.years.empty()) {
        throw input_error{"monthly heatmap requires at least one year"};
    }
    if (grid.values.size() != grid.years.size() * 12U) {
        throw input_error{
            "monthly heatmap values must be years x 12"};
    }
    plotly_document document = create_document("Monthly Returns Heatmap");
    document.x_axis.title = "Month";
    document.y_axis.title = "Year";
    document.y_axis.reversed = true;
    plotly_trace trace;
    trace.type = plotly_trace::kind::heatmap;
    trace.name = "Monthly Returns";
    trace.heatmap_x = grid.columns;
    if (trace.heatmap_x.size() != 12U) {
        trace.heatmap_x.assign(std::begin(month_labels),
                               std::end(month_labels));
    }
    trace.heatmap_y.reserve(grid.years.size());
    for (const int year : grid.years) {
        trace.heatmap_y.push_back(std::to_string(year));
    }
    trace.heatmap_z = grid.values;
    trace.heatmap_columns = 12U;
    trace.color_stops = {{0.0, "#D32F2F"}, {0.5, "#FFFFFF"}, {1.0, "#388E3C"}};
    trace.color_range_set = true;
    trace.color_min = grid.color_min;
    trace.color_max = grid.color_max;
    trace.colorbar_title = "Return (%)";
    trace.colorbar_tickformat = ".1f";
    trace.hovertemplate =
        "<b>%{y}</b><br>Month: %{x}<br>Return: %{z:.2f}%<extra></extra>";
    document.traces.push_back(std::move(trace));
    figure_ = document;
    return document;
}

plotly_document plotly_backend::plot_correlation_matrix(
    const correlation_matrix& matrix) {
    if (matrix.labels.empty()) {
        throw input_error{
            "correlation matrix requires at least one asset"};
    }
    if (matrix.values.size() != matrix.labels.size() * matrix.labels.size()) {
        throw input_error{
            "correlation matrix values must be labels x labels"};
    }
    plotly_document document = create_document("Asset Correlation Matrix");
    document.x_axis.side = "bottom";
    plotly_trace trace;
    trace.type = plotly_trace::kind::heatmap;
    trace.name = "Correlation";
    trace.heatmap_x = matrix.labels;
    trace.heatmap_y = matrix.labels;
    trace.heatmap_z = matrix.values;
    trace.heatmap_columns = matrix.labels.size();
    trace.colorscale = "RdBu";
    trace.zmid_zero = true;
    trace.colorbar_title = "Correlation";
    trace.hovertemplate =
        "Asset 1: %{y}<br>Asset 2: %{x}<br>Correlation: "
        "%{z:.2f}<extra></extra>";
    document.traces.push_back(std::move(trace));
    figure_ = document;
    return document;
}

plotly_document plotly_backend::plot_efficient_frontier(
    const frontier_chart_data& frontier) {
    if (frontier.volatilities.size() != frontier.returns.size()) {
        throw input_error{
            "efficient frontier volatilities and returns must match"};
    }
    plotly_document document = create_document("Efficient Frontier");
    document.x_axis.title = "Volatility (Annualized)";
    document.y_axis.title = "Expected Return (Annualized)";
    document.hovermode = "closest";

    if (!frontier.volatilities.empty()) {
        plotly_trace curve;
        curve.type = plotly_trace::kind::scatter;
        curve.name = "Efficient Frontier";
        curve.mode = "lines+markers";
        curve.line_color = colors_.highlight;
        curve.line_width = 3.0;
        curve.marker_size = 6.0;
        curve.x_values = frontier.volatilities;
        curve.y_values = frontier.returns;
        curve.custom_data = frontier.sharpe;
        curve.hovertemplate =
            "Vol: %{x:.2%}<br>Return: %{y:.2%}<br>Sharpe: "
            "%{customdata:.2f}<extra></extra>";
        document.traces.push_back(std::move(curve));

        plotly_trace maximum;
        maximum.type = plotly_trace::kind::scatter;
        maximum.name = "Max Sharpe";
        maximum.mode = "markers";
        maximum.marker_size = 15.0;
        maximum.marker_color = colors_.positive;
        maximum.marker_symbol = "star";
        maximum.x_values = {frontier.max_sharpe_volatility};
        maximum.y_values = {frontier.max_sharpe_return};
        maximum.hovertemplate =
            "Max Sharpe<br>Vol: %{x:.2%}<br>Return: %{y:.2%}<extra></extra>";
        document.traces.push_back(std::move(maximum));

        plotly_trace minimum;
        minimum.type = plotly_trace::kind::scatter;
        minimum.name = "Min Variance";
        minimum.mode = "markers";
        minimum.marker_size = 15.0;
        minimum.marker_color = colors_.warning;
        minimum.marker_symbol = "diamond";
        minimum.x_values = {frontier.min_variance_volatility};
        minimum.y_values = {frontier.min_variance_return};
        minimum.hovertemplate =
            "Min Variance<br>Vol: %{x:.2%}<br>Return: "
            "%{y:.2%}<extra></extra>";
        document.traces.push_back(std::move(minimum));
    }
    figure_ = document;
    return document;
}

void plotly_backend::set_figure(plotly_document document) {
    figure_ = std::move(document);
}

const plotly_document& plotly_backend::figure() const {
    if (!figure_.has_value()) {
        throw input_error{
            "No figure to display. Call a plot method first."};
    }
    return *figure_;
}

void plotly_backend::save_html(const std::string& path) const {
    figure().save_html(path);
}

void plotly_backend::save_image(const std::string& path) const {
    figure().save_image(path);
}

bool plotly_backend::show() const {
    static_cast<void>(figure());
    last_show_status_ = NOT_RUN_INTERACTIVE;
    return false;
}

}  // namespace fincore
