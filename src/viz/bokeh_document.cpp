#include <fincore/viz/bokeh_document.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

constexpr std::string_view bokeh_positive = "#00C853";
constexpr std::string_view bokeh_negative = "#D50000";
constexpr std::string_view bokeh_neutral = "#212121";
constexpr std::string_view bokeh_highlight = "#2962FF";
constexpr std::string_view bokeh_warning = "#FF6D00";

// These helpers exist once per backend file rather than in a shared unit, and the
// two copies must stay identical: the figure comparison is made against the same
// reference for both, so a formatting change landing in one backend alone would
// show up as a difference rather than as a shared convention.
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

bokeh_glyph line_glyph(const chart_series& series, std::string field,
                       std::string color) {
    bokeh_glyph glyph;
    glyph.type = bokeh_glyph::kind::line;
    glyph.x_field = "date";
    glyph.y_field = std::move(field);
    glyph.x_labels = date_labels(series.index);
    glyph.y_values = series.values;
    if (glyph.x_labels.size() != glyph.y_values.size()) {
        throw input_error{
            "bokeh series labels and values must have equal lengths"};
    }
    glyph.color = std::move(color);
    glyph.line_width = 2.0;
    glyph.line_dash = series.dashed ? "dashed" : "solid";
    glyph.legend_label = series.label;
    return glyph;
}

void write_glyph(detail::json_writer& writer, const bokeh_glyph& glyph) {
    writer.begin_object();
    switch (glyph.type) {
        case bokeh_glyph::kind::line:
            writer.key("type");
            writer.value_string("line");
            break;
        case bokeh_glyph::kind::varea:
            writer.key("type");
            writer.value_string("varea");
            break;
        case bokeh_glyph::kind::rect:
            writer.key("type");
            writer.value_string("rect");
            break;
    }
    if (!glyph.x_field.empty()) {
        writer.key("x_field");
        writer.value_string(glyph.x_field);
    }
    if (!glyph.y_field.empty()) {
        writer.key("y_field");
        writer.value_string(glyph.y_field);
    }
    if (glyph.type == bokeh_glyph::kind::rect) {
        if (glyph.rect_x.size() != glyph.rect_y.size() ||
            glyph.rect_x.size() != glyph.rect_values.size()) {
            throw input_error{
                "bokeh rect glyph requires equal x, y and value lengths"};
        }
        writer.key("x");
        write_string_array(writer, glyph.rect_x);
        writer.key("y");
        write_string_array(writer, glyph.rect_y);
        writer.key("values");
        write_number_array(writer, glyph.rect_values, "bokeh rect value");
    } else {
        if (!glyph.x_labels.empty()) {
            if (glyph.x_labels.size() != glyph.y_values.size()) {
                throw input_error{
                    "bokeh glyph labels and values must have equal lengths"};
            }
            writer.key("x");
            write_string_array(writer, glyph.x_labels);
        } else if (!glyph.x_values.empty()) {
            if (glyph.x_values.size() != glyph.y_values.size()) {
                throw input_error{
                    "bokeh glyph x and y must have equal lengths"};
            }
            writer.key("x");
            write_number_array(writer, glyph.x_values, "bokeh x");
        }
        writer.key("y");
        write_number_array(writer, glyph.y_values, "bokeh y");
    }
    if (!glyph.color.empty() || glyph.line_width > 0.0 ||
        (!glyph.line_dash.empty() && glyph.line_dash != "solid")) {
        writer.key("line");
        writer.begin_object();
        if (!glyph.color.empty()) {
            writer.key("color");
            writer.value_string(glyph.color);
        }
        if (glyph.line_width > 0.0) {
            writer.key("width");
            writer.value_number(glyph.line_width, "bokeh line width");
        }
        if (!glyph.line_dash.empty()) {
            writer.key("dash");
            writer.value_string(glyph.line_dash);
        }
        writer.end_object();
    }
    if (!glyph.fill_color.empty()) {
        writer.key("fill_color");
        writer.value_string(glyph.fill_color);
        writer.key("fill_alpha");
        writer.value_number(glyph.fill_alpha, "bokeh fill alpha");
    }
    if (!glyph.legend_label.empty()) {
        writer.key("legend_label");
        writer.value_string(glyph.legend_label);
    }
    if (!glyph.palette.empty()) {
        writer.key("palette");
        writer.value_string(glyph.palette);
        if (glyph.mapper_range_set) {
            writer.key("mapper_low");
            writer.value_number(glyph.mapper_low, "bokeh mapper low");
            writer.key("mapper_high");
            writer.value_number(glyph.mapper_high, "bokeh mapper high");
        }
    }
    writer.end_object();
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

const std::vector<std::string>& rd_yl_gn11() {
    static const std::vector<std::string> palette{
        "#a50026", "#d73027", "#f46d43", "#fdae61", "#fee08b", "#ffffbf",
        "#d9ef8b", "#a6d96a", "#66bd63", "#1a9850", "#006837"};
    return palette;
}

std::string palette_color(const double normalized) {
    const auto& palette = rd_yl_gn11();
    const double clamped = std::clamp(normalized, 0.0, 1.0);
    const double position =
        clamped * static_cast<double>(palette.size() - 1U);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = std::min(lower + 1U, palette.size() - 1U);
    const double weight = position - static_cast<double>(lower);
    const auto first = parse_hex_color(palette[lower]);
    const auto second = parse_hex_color(palette[upper]);
    const auto mix = [weight](const int a, const int b) {
        return static_cast<int>(
            std::lround(a + (b - a) * weight));
    };
    return rgb_text({mix(first.red, second.red),
                     mix(first.green, second.green),
                     mix(first.blue, second.blue)});
}

}  // namespace

printf_tick_formatter::printf_tick_formatter(std::string format)
    : format_(std::move(format)) {
    if (format_.find('\0') != std::string::npos) {
        throw input_error{"printf format must not contain an embedded NUL"};
    }
    static_cast<void>(printf_tick_formatter::format(0.0));
}

std::string printf_tick_formatter::format(const double value) const {
    std::string specification;
    specification.push_back('%');
    bool specification_open = false;
    bool seen_flags = false;
    bool seen_width = false;
    bool seen_precision = false;
    std::size_t conversions = 0U;
    char conversion = '\0';
    for (std::size_t position = 0U; position < format_.size(); ++position) {
        const char character = format_[position];
        if (!specification_open) {
            if (character == '%') {
                if (position + 1U < format_.size() &&
                    format_[position + 1U] == '%') {
                    specification += "%%";
                    ++position;
                    continue;
                }
                specification_open = true;
                seen_flags = false;
                seen_width = false;
                seen_precision = false;
                continue;
            }
            throw input_error{
                "printf format must be exactly one conversion"};
        }
        if (!seen_flags &&
            (character == '-' || character == '+' || character == ' ' ||
             character == '0' || character == '#')) {
            specification.push_back(character);
            continue;
        }
        if (!seen_width && !seen_precision && character >= '0' &&
            character <= '9') {
            seen_flags = true;
            seen_width = true;
            specification.push_back(character);
            continue;
        }
        if (!seen_precision && character == '.') {
            seen_precision = true;
            seen_flags = true;
            specification += ".";
            continue;
        }
        if (seen_precision && character >= '0' && character <= '9') {
            specification.push_back(character);
            continue;
        }
        if (character == 'f' || character == 'F' || character == 'e' ||
            character == 'E' || character == 'g' || character == 'G' ||
            character == 'd' || character == 'i') {
            conversion = character;
            ++conversions;
            if (conversions > 1U) {
                throw input_error{
                    "printf format must be exactly one conversion"};
            }
            specification.push_back(character == 'd' || character == 'i'
                                        ? 'f'
                                        : character);
            specification_open = false;
            continue;
        }
        throw input_error{"unsupported printf conversion in tick format"};
    }
    if (specification_open) {
        throw input_error{"printf format is incomplete"};
    }
    if (conversions != 1U) {
        throw input_error{"printf format must be exactly one conversion"};
    }
    if (conversion == 'd' || conversion == 'i') {
        // Python's %d truncates toward zero; integer presentation only.
        const std::string integer_specification =
            "%" + specification.substr(1U, specification.size() - 2U) + "lld";
        const long long truncated =
            static_cast<long long>(std::trunc(value));
        const int needed = std::snprintf(nullptr, 0,
                                         integer_specification.c_str(),
                                         truncated);
        if (needed <= 0) {
            return {};
        }
        std::vector<char> buffer(static_cast<std::size_t>(needed) + 1U);
        std::snprintf(buffer.data(), buffer.size(),
                      integer_specification.c_str(), truncated);
        return std::string(buffer.data(),
                           static_cast<std::size_t>(needed));
    }
    const int needed = std::snprintf(nullptr, 0, specification.c_str(), value);
    if (needed <= 0) {
        return {};
    }
    std::vector<char> buffer(static_cast<std::size_t>(needed) + 1U);
    std::snprintf(buffer.data(), buffer.size(), specification.c_str(), value);
    return std::string(buffer.data(), static_cast<std::size_t>(needed));
}

std::string bokeh_document::to_json() const {
    std::string output;
    detail::json_writer writer(output);
    writer.begin_object();
    writer.key("title");
    writer.value_string(title);
    writer.key("height");
    writer.value_integer(height);
    writer.key("width");
    writer.value_integer(width);
    writer.key("sizing_mode");
    writer.value_string(sizing_mode);
    writer.key("x_axis_type");
    writer.value_string(x_axis_type);
    writer.key("x_axis_label");
    writer.value_string(x_axis_label);
    writer.key("y_axis_label");
    writer.value_string(y_axis_label);
    writer.key("background_fill_color");
    writer.value_string(background_fill_color);
    writer.key("border_fill_color");
    writer.value_string(border_fill_color);
    writer.key("grid_line_color");
    writer.value_string(grid_line_color);
    writer.key("text_color");
    writer.value_string(text_color);
    if (!axis_location.empty()) {
        writer.key("axis_location");
        writer.value_string(axis_location);
    }
    if (!toolbar_location.empty()) {
        writer.key("toolbar_location");
        writer.value_string(toolbar_location);
    }
    writer.key("grid_visible");
    writer.value_boolean(grid_visible);
    writer.key("axis_visible");
    writer.value_boolean(axis_visible);
    if (!major_label_text_font_size.empty()) {
        writer.key("major_label_text_font_size");
        writer.value_string(major_label_text_font_size);
    }
    if (major_label_standoff != 0) {
        writer.key("major_label_standoff");
        writer.value_integer(major_label_standoff);
    }
    if (!x_factors.empty() || !y_factors.empty()) {
        writer.key("ranges");
        writer.begin_object();
        writer.key("x_factors");
        write_string_array(writer, x_factors);
        writer.key("y_factors");
        write_string_array(writer, y_factors);
        writer.end_object();
    }
    if (!color_mapper_palette.empty()) {
        writer.key("color_mapper");
        writer.begin_object();
        writer.key("palette");
        writer.value_string(color_mapper_palette);
        if (mapper_range_set) {
            writer.key("low");
            writer.value_number(mapper_low, "bokeh mapper low");
            writer.key("high");
            writer.value_number(mapper_high, "bokeh mapper high");
        }
        writer.end_object();
        writer.key("color_bar");
        writer.value_boolean(color_bar);
        if (color_bar) {
            writer.key("color_bar_ticker");
            writer.begin_object();
            writer.key("type");
            writer.value_string(color_bar_ticker);
            writer.key("desired_num_ticks");
            writer.value_integer(color_bar_desired_ticks);
            writer.end_object();
        }
    }
    writer.key("glyphs");
    writer.begin_array();
    for (const auto& glyph : glyphs) {
        write_glyph(writer, glyph);
    }
    writer.end_array();
    writer.key("tools");
    writer.begin_object();
    writer.key("hovers");
    writer.begin_array();
    for (const auto& hover : hovers) {
        writer.begin_object();
        writer.key("tooltips");
        writer.begin_array();
        for (const auto& [name, template_text] : hover.tooltips) {
            writer.begin_array();
            writer.value_string(name);
            writer.value_string(template_text);
            writer.end_array();
        }
        writer.end_array();
        writer.key("mode");
        writer.value_string(hover.mode);
        writer.key("formatters");
        writer.begin_object();
        for (const auto& [field, formatter] : hover.formatters) {
            writer.key(field);
            writer.value_string(formatter);
        }
        writer.end_object();
        writer.end_object();
    }
    writer.end_array();
    writer.key("spans");
    writer.begin_array();
    for (const auto& span : spans) {
        writer.begin_object();
        writer.key("location");
        writer.value_number(span.location, "bokeh span location");
        writer.key("dimension");
        writer.value_string(span.dimension);
        writer.key("line_color");
        writer.value_string(span.line_color);
        writer.key("line_alpha");
        writer.value_number(span.line_alpha, "bokeh span alpha");
        writer.end_object();
    }
    writer.end_array();
    writer.end_object();
    writer.key("legend");
    writer.begin_object();
    writer.key("location");
    writer.value_string(legend_location);
    writer.key("click_policy");
    writer.value_string(legend_click_policy);
    writer.end_object();
    writer.end_object();
    output.push_back('\n');
    return output;
}

std::string bokeh_document::to_svg() const {
    const double canvas_width = static_cast<double>(std::max(width, 320));
    const double canvas_height = static_cast<double>(std::max(height, 240));
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
           escape_xml(border_fill_color) + "\"/>";
    svg += "<rect x=\"" + svg_number(left) + "\" y=\"" + svg_number(top) +
           "\" width=\"" + svg_number(right - left) + "\" height=\"" +
           svg_number(bottom - top) + "\" fill=\"" +
           escape_xml(background_fill_color) + "\" stroke=\"" +
           escape_xml(grid_line_color) + "\"/>";
    if (!title.empty()) {
        svg += "<text x=\"" + svg_number(canvas_width / 2.0) +
               "\" y=\"30\" text-anchor=\"middle\" font-family=\"sans-serif\" "
               "font-size=\"16\" font-weight=\"bold\" fill=\"" +
               escape_xml(text_color) + "\">" + escape_xml(title) +
               "</text>";
    }

    double minimum_y = std::numeric_limits<double>::infinity();
    double maximum_y = -std::numeric_limits<double>::infinity();
    std::size_t maximum_points = 0U;
    for (const auto& glyph : glyphs) {
        if (glyph.type == bokeh_glyph::kind::rect) {
            continue;
        }
        maximum_points = std::max(maximum_points, glyph.y_values.size());
        for (const double value : glyph.y_values) {
            if (std::isfinite(value)) {
                minimum_y = std::min(minimum_y, value);
                maximum_y = std::max(maximum_y, value);
            }
        }
        if (glyph.type == bokeh_glyph::kind::varea) {
            minimum_y = std::min(minimum_y, 0.0);
            maximum_y = std::max(maximum_y, 0.0);
        }
    }
    for (const auto& span : spans) {
        minimum_y = std::min(minimum_y, span.location);
        maximum_y = std::max(maximum_y, span.location);
    }
    if (!std::isfinite(minimum_y) || !std::isfinite(maximum_y)) {
        minimum_y = 0.0;
        maximum_y = 1.0;
    }
    if (maximum_y - minimum_y < 1e-12) {
        minimum_y -= 1.0;
        maximum_y += 1.0;
    }
    bool numeric_x = false;
    double minimum_x = std::numeric_limits<double>::infinity();
    double maximum_x = -std::numeric_limits<double>::infinity();
    for (const auto& glyph : glyphs) {
        if (glyph.type == bokeh_glyph::kind::rect ||
            !glyph.x_labels.empty() ||
            glyph.x_values.size() != glyph.y_values.size()) {
            continue;
        }
        for (const double value : glyph.x_values) {
            if (std::isfinite(value)) {
                numeric_x = true;
                minimum_x = std::min(minimum_x, value);
                maximum_x = std::max(maximum_x, value);
            }
        }
    }
    if (!numeric_x || maximum_x - minimum_x < 1e-12) {
        numeric_x = false;
        minimum_x = 0.0;
        maximum_x = static_cast<double>(
            maximum_points > 1U ? maximum_points - 1U : 1U);
    }
    const double span_y = maximum_y - minimum_y;
    const double span_x = maximum_x - minimum_x;
    const auto map_y = [&](const double value) {
        return bottom - (value - minimum_y) / span_y * (bottom - top);
    };
    const auto map_x = [&](const double value) {
        return left + (value - minimum_x) / span_x * (right - left);
    };

    // Heatmap rectangles first.
    for (const auto& glyph : glyphs) {
        if (glyph.type != bokeh_glyph::kind::rect) {
            continue;
        }
        double minimum_value = std::numeric_limits<double>::infinity();
        double maximum_value = -std::numeric_limits<double>::infinity();
        for (const double value : glyph.rect_values) {
            if (std::isfinite(value)) {
                minimum_value = std::min(minimum_value, value);
                maximum_value = std::max(maximum_value, value);
            }
        }
        if (!std::isfinite(minimum_value) || !std::isfinite(maximum_value)) {
            minimum_value = -10.0;
            maximum_value = 10.0;
        }
        if (glyph.mapper_range_set) {
            minimum_value = glyph.mapper_low;
            maximum_value = glyph.mapper_high;
        }
        if (maximum_value - minimum_value < 1e-12) {
            maximum_value = minimum_value + 1.0;
        }
        const auto column_count =
            static_cast<double>(std::max<std::size_t>(x_factors.size(), 1U));
        const auto row_count =
            static_cast<double>(std::max<std::size_t>(y_factors.size(), 1U));
        const double cell_width = (right - left) / column_count;
        const double cell_height = (bottom - top) / row_count;
        for (std::size_t position = 0U; position < glyph.rect_values.size();
             ++position) {
            const double value = glyph.rect_values[position];
            if (!std::isfinite(value)) {
                continue;
            }
            const auto column_position =
                std::find(x_factors.begin(), x_factors.end(),
                          glyph.rect_x[position]);
            const auto row_position =
                std::find(y_factors.begin(), y_factors.end(),
                          glyph.rect_y[position]);
            const auto column =
                column_position == x_factors.end()
                    ? 0U
                    : static_cast<std::size_t>(
                          column_position - x_factors.begin());
            const auto row = row_position == y_factors.end()
                                 ? 0U
                                 : static_cast<std::size_t>(
                                       row_position - y_factors.begin());
            svg += "<rect x=\"" +
                   svg_number(left + static_cast<double>(column) * cell_width) +
                   "\" y=\"" +
                   svg_number(top + static_cast<double>(row) * cell_height) +
                   "\" width=\"" + svg_number(cell_width) + "\" height=\"" +
                   svg_number(cell_height) + "\" fill=\"" +
                   palette_color((value - minimum_value) /
                                 (maximum_value - minimum_value)) +
                   "\"/>";
        }
    }

    for (const auto& span : spans) {
        const double y = map_y(span.location);
        svg += "<line x1=\"" + svg_number(left) + "\" y1=\"" +
               svg_number(y) + "\" x2=\"" + svg_number(right) + "\" y2=\"" +
               svg_number(y) + "\" stroke=\"" + escape_xml(span.line_color) +
               "\" stroke-opacity=\"" + svg_number(span.line_alpha) +
               "\" stroke-width=\"1\"/>";
    }

    for (const auto& glyph : glyphs) {
        if (glyph.type == bokeh_glyph::kind::rect || glyph.y_values.empty()) {
            continue;
        }
        std::vector<double> xs;
        std::vector<double> ys;
        xs.reserve(glyph.y_values.size());
        ys.reserve(glyph.y_values.size());
        for (std::size_t position = 0U; position < glyph.y_values.size();
             ++position) {
            const double raw_x =
                numeric_x && glyph.x_values.size() == glyph.y_values.size()
                    ? glyph.x_values[position]
                    : static_cast<double>(position);
            xs.push_back(map_x(raw_x));
            ys.push_back(std::isfinite(glyph.y_values[position])
                             ? map_y(glyph.y_values[position])
                             : nan);
        }
        if (glyph.type == bokeh_glyph::kind::varea) {
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
                svg += "<polygon fill=\"" + escape_xml(glyph.fill_color) +
                       "\" fill-opacity=\"" +
                       svg_number(glyph.fill_alpha) +
                       "\" stroke=\"none\" points=\"" + points + " " +
                       svg_number(xs.back()) + "," + svg_number(baseline) +
                       " " + svg_number(xs.front()) + "," +
                       svg_number(baseline) + "\"/>";
            }
        }
        std::string points;
        for (std::size_t position = 0U; position < ys.size(); ++position) {
            if (!std::isfinite(ys[position])) {
                continue;
            }
            if (!points.empty()) {
                points.push_back(' ');
            }
            points += svg_number(xs[position]) + "," +
                      svg_number(ys[position]);
        }
        if (points.empty()) {
            continue;
        }
        svg += "<polyline fill=\"none\" stroke=\"" + escape_xml(glyph.color) +
               "\" stroke-width=\"" + svg_number(glyph.line_width) + "\"";
        if (glyph.line_dash == "dashed") {
            svg += " stroke-dasharray=\"6,4\"";
        }
        svg += " points=\"" + points + "\"/>";
    }

    for (int step = 0; step <= 4; ++step) {
        const double value =
            minimum_y + span_y * static_cast<double>(step) / 4.0;
        const double y = map_y(value);
        svg += "<text x=\"" + svg_number(left - 8.0) + "\" y=\"" +
               svg_number(y + 3.0) +
               "\" text-anchor=\"end\" font-family=\"sans-serif\" "
               "font-size=\"9\" fill=\"" +
               escape_xml(text_color) + "\">" + svg_number(value) +
               "</text>";
    }
    svg += "</svg>";
    return svg;
}

std::string bokeh_document::to_html() const {
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
    html += ".fincore-bokeh-chart { border: 1px solid #e5e7eb; "
            "border-radius: 8px; overflow: hidden; }\n";
    html += "svg { display: block; width: 100%; height: auto; }\n";
    html += "</style>\n</head>\n<body data-backend=\"bokeh\">\n";
    html += "<div class=\"fincore-bokeh-chart\">\n";
    html += to_svg();
    html += "\n</div>\n";
    html += "<script type=\"application/json\" "
            "id=\"fincore-bokeh-document\">" +
            escaped_json + "</script>\n";
    html += "</body></html>\n";
    return html;
}

void bokeh_document::save_html(const std::string& path) const {
    if (path.empty()) {
        throw input_error{"bokeh save_html path must not be empty"};
    }
    const std::string html = to_html();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw input_error{"bokeh save_html could not open: " + path};
    }
    stream.write(html.data(), static_cast<std::streamsize>(html.size()));
    if (!stream) {
        throw input_error{"bokeh save_html could not write: " + path};
    }
}

bokeh_backend::bokeh_backend(std::string theme, const int height,
                             const int width, std::string sizing_mode)
    : theme_(std::move(theme)),
      height_(height),
      width_(width),
      sizing_mode_(std::move(sizing_mode)) {
    if (height_ <= 0 || width_ <= 0) {
        throw input_error{"bokeh height and width must be positive"};
    }
    if (theme_ == "dark") {
        background_fill_color_ = "#1E1E1E";
        grid_line_color_ = "#424242";
        text_color_ = "#E0E0E0";
    } else {
        background_fill_color_ = "#FFFFFF";
        grid_line_color_ = "#E0E0E0";
        text_color_ = "#424242";
    }
}

bokeh_document bokeh_backend::create_document(std::string title) const {
    bokeh_document document;
    document.title = std::move(title);
    document.height = height_;
    document.width = width_;
    document.sizing_mode = sizing_mode_;
    document.background_fill_color = background_fill_color_;
    document.border_fill_color = background_fill_color_;
    document.grid_line_color = grid_line_color_;
    document.text_color = text_color_;
    return document;
}

bokeh_document bokeh_backend::plot_returns(const series_chart& chart) const {
    require_series(chart, "plot_returns");
    bokeh_document document = create_document("Cumulative Returns");
    document.x_axis_label = "Date";
    document.y_axis_label = "Cumulative Return";
    for (const auto& series : chart.series) {
        bokeh_glyph glyph = line_glyph(
            series, "returns",
            series.benchmark ? std::string(bokeh_neutral)
                             : std::string(bokeh_highlight));
        if (glyph.legend_label.empty()) {
            glyph.legend_label =
                series.benchmark ? "Benchmark" : "Portfolio";
        }
        glyph.line_dash = series.benchmark ? "dashed" : "solid";
        document.glyphs.push_back(std::move(glyph));
    }
    bokeh_hover hover;
    hover.tooltips = {{"Date", "@date{%F}"}, {"Return", "@returns{0.00%}"}};
    hover.formatters = {{"@date", "datetime"}};
    hover.mode = "vline";
    document.hovers.push_back(std::move(hover));
    document.legend_location = "top_left";
    document.legend_click_policy = "hide";
    return document;
}

bokeh_document bokeh_backend::plot_drawdown(const series_chart& chart) const {
    require_series(chart, "plot_drawdown");
    bokeh_document document = create_document("Drawdown (Underwater Chart)");
    document.x_axis_label = "Date";
    document.y_axis_label = "Drawdown";
    for (const auto& series : chart.series) {
        bokeh_glyph area;
        area.type = bokeh_glyph::kind::varea;
        area.x_field = "date";
        area.y_field = "drawdown";
        area.x_labels = date_labels(series.index);
        area.y_values = series.values;
        area.fill_color = bokeh_negative;
        area.fill_alpha = 0.3;
        document.glyphs.push_back(std::move(area));

        bokeh_glyph line = line_glyph(series, "drawdown",
                                      std::string(bokeh_negative));
        line.line_width = 1.5;
        line.legend_label.clear();
        document.glyphs.push_back(std::move(line));
    }
    bokeh_hover hover;
    hover.tooltips = {{"Date", "@date{%F}"},
                      {"Drawdown", "@drawdown{0.00%}"}};
    hover.formatters = {{"@date", "datetime"}};
    document.hovers.push_back(std::move(hover));
    return document;
}

bokeh_document bokeh_backend::plot_rolling_sharpe(
    const series_chart& chart, const std::size_t window) const {
    require_series(chart, "plot_rolling_sharpe");
    const std::size_t effective_window =
        chart.window != 0U ? chart.window : (window == 0U ? 252U : window);
    bokeh_document document = create_document(
        "Rolling Sharpe Ratio (" + std::to_string(effective_window) +
        "-day window)");
    document.x_axis_label = "Date";
    document.y_axis_label = "Sharpe Ratio";
    for (const auto& series : chart.series) {
        bokeh_glyph glyph = line_glyph(
            series, "sharpe",
            series.benchmark ? std::string(bokeh_neutral)
                             : std::string(bokeh_highlight));
        if (glyph.legend_label.empty()) {
            glyph.legend_label = series.benchmark ? "Benchmark Sharpe"
                                                  : "Portfolio Sharpe";
        }
        glyph.line_dash = series.benchmark ? "dashed" : "solid";
        document.glyphs.push_back(std::move(glyph));
    }
    bokeh_span zero;
    zero.location = 0.0;
    zero.dimension = "width";
    zero.line_color = grid_line_color_;
    zero.line_alpha = 0.5;
    document.spans.push_back(std::move(zero));
    bokeh_hover hover;
    hover.tooltips = {{"Date", "@date{%F}"}, {"Sharpe", "@sharpe{0.00}"}};
    hover.formatters = {{"@date", "datetime"}};
    document.hovers.push_back(std::move(hover));
    document.legend_location = "top_left";
    document.legend_click_policy = "hide";
    return document;
}

bokeh_document bokeh_backend::plot_monthly_heatmap(
    const heat_grid& grid) const {
    if (grid.years.empty()) {
        throw input_error{"monthly heatmap requires at least one year"};
    }
    if (grid.values.size() != grid.years.size() * 12U) {
        throw input_error{"monthly heatmap values must be years x 12"};
    }
    bokeh_document document = create_document("Monthly Returns Heatmap");
    document.x_axis_type = "auto";
    document.axis_location = "above";
    document.toolbar_location = "none";
    document.grid_visible = false;
    document.axis_visible = false;
    document.major_label_text_font_size = "10px";
    document.major_label_standoff = 2;
    document.x_factors.assign(std::begin(month_labels),
                              std::end(month_labels));
    document.y_factors.reserve(grid.years.size());
    for (auto iterator = grid.years.rbegin(); iterator != grid.years.rend();
         ++iterator) {
        document.y_factors.push_back(std::to_string(*iterator));
    }
    bokeh_glyph rectangles;
    rectangles.type = bokeh_glyph::kind::rect;
    rectangles.rect_x.reserve(grid.values.size());
    rectangles.rect_y.reserve(grid.values.size());
    rectangles.rect_values.reserve(grid.values.size());
    for (std::size_t year = 0U; year < grid.years.size(); ++year) {
        for (std::size_t month = 0U; month < 12U; ++month) {
            rectangles.rect_x.emplace_back(month_labels[month]);
            rectangles.rect_y.push_back(std::to_string(grid.years[year]));
            rectangles.rect_values.push_back(
                grid.values[year * 12U + month]);
        }
    }
    rectangles.palette = "RdYlGn11";
    rectangles.mapper_range_set = true;
    rectangles.mapper_low = grid.color_min;
    rectangles.mapper_high = grid.color_max;
    document.glyphs.push_back(std::move(rectangles));

    bokeh_hover hover;
    hover.tooltips = {{"Year", "@year"},
                      {"Month", "@month"},
                      {"Return", "@value{0.0}%"}};
    document.hovers.push_back(std::move(hover));
    document.color_mapper_palette = "RdYlGn11";
    document.mapper_range_set = true;
    document.mapper_low = grid.color_min;
    document.mapper_high = grid.color_max;
    document.color_bar = true;
    document.color_bar_ticker = "BasicTicker";
    document.color_bar_desired_ticks = 10;
    return document;
}

bool bokeh_backend::show(const bokeh_document& document) const {
    static_cast<void>(document);
    last_show_status_ = "NOT_RUN_INTERACTIVE";
    return false;
}

void bokeh_backend::save_html(const bokeh_document& document,
                              const std::string& path) const {
    document.save_html(path);
}

}  // namespace fincore
