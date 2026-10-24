#include <fincore/report/renderers.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {
namespace {

constexpr double page_width = 595.0;
constexpr double page_height = 842.0;
constexpr double margin_left = 28.0;
constexpr double margin_right = 567.0;
constexpr double margin_top = 34.0;
constexpr double margin_bottom = 40.0;
constexpr double body_width = margin_right - margin_left;
constexpr double body_bottom = page_height - margin_bottom;

std::string number_text(const double value) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return std::signbit(value) ? "-inf" : "inf";
    }
    char buffer[64];
    const auto result =
        std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc{}) {
        return "nan";
    }
    return std::string(buffer, result.ptr);
}

/**
 * Encode UTF-8 text for a base-14 Helvetica font using WinAnsi.
 *
 * Latin-1 code points map one-to-one; everything else becomes `?` (declared
 * format variance from the Chromium-backed Python writer, which embeds a
 * Unicode font).
 */
std::string encode_text(const std::string_view text) {
    std::string output;
    output.reserve(text.size());
    for (std::size_t position = 0U; position < text.size();) {
        const auto character = static_cast<unsigned char>(text[position]);
        if (character < 0x80U) {
            if (character >= 0x20U) {
                output.push_back(static_cast<char>(character));
            } else {
                output.push_back(' ');
            }
            ++position;
            continue;
        }
        std::uint32_t code_point = 0U;
        std::size_t length = 0U;
        if ((character & 0xE0U) == 0xC0U) {
            code_point = character & 0x1FU;
            length = 2U;
        } else if ((character & 0xF0U) == 0xE0U) {
            code_point = character & 0x0FU;
            length = 3U;
        } else if ((character & 0xF8U) == 0xF0U) {
            code_point = character & 0x07U;
            length = 4U;
        } else {
            output.push_back('?');
            ++position;
            continue;
        }
        if (position + length > text.size()) {
            output.push_back('?');
            break;
        }
        bool valid = true;
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(text[position + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                valid = false;
                break;
            }
            code_point = (code_point << 6U) | (continuation & 0x3FU);
        }
        if (!valid) {
            output.push_back('?');
            ++position;
            continue;
        }
        if (code_point >= 0xA0U && code_point <= 0xFFU) {
            output.push_back(static_cast<char>(code_point));
        } else {
            output.push_back('?');
        }
        position += length;
    }
    return output;
}

std::string pdf_string(const std::string_view encoded) {
    std::string output;
    output.reserve(encoded.size());
    for (const char character : encoded) {
        if (character == '(' || character == ')' || character == '\\') {
            output.push_back('\\');
        }
        output.push_back(character);
    }
    return output;
}

std::size_t text_length(const std::string_view encoded) {
    // Approximate Helvetica advance width: half an em per glyph.
    return encoded.size();
}

std::vector<std::string> wrap_text(const std::string_view text,
                                   const double size, const double width) {
    const auto maximum_length = static_cast<std::size_t>(
        std::max(width / (size * 0.5), 8.0));
    const std::string encoded = encode_text(text);
    std::vector<std::string> lines;
    std::string current;
    std::size_t position = 0U;
    while (position < encoded.size()) {
        const std::size_t word_start = position;
        while (position < encoded.size() && encoded[position] != ' ') {
            ++position;
        }
        const std::string word =
            encoded.substr(word_start, position - word_start);
        if (word.empty()) {
            ++position;
            continue;
        }
        if (current.empty()) {
            current = word;
        } else if (current.size() + 1U + word.size() <= maximum_length) {
            current.push_back(' ');
            current += word;
        } else {
            lines.push_back(current);
            current = word;
        }
        while (current.size() > maximum_length) {
            lines.push_back(current.substr(0U, maximum_length));
            current = current.substr(maximum_length);
        }
        while (position < encoded.size() && encoded[position] == ' ') {
            ++position;
        }
    }
    if (!current.empty() || lines.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::string table_cell_text(const report_cell& cell) {
    switch (cell.type) {
        case report_cell::kind::number:
            return number_text(cell.number);
        case report_cell::kind::text:
            return cell.text;
        case report_cell::kind::boolean:
            return cell.boolean ? "true" : "false";
        case report_cell::kind::missing:
            return {};
    }
    return {};
}

std::string metric_text(const report_metric& metric) {
    std::string value = "n/a";
    if (metric.number.has_value()) {
        value = number_text(*metric.number);
    } else if (metric.boolean.has_value()) {
        value = *metric.boolean ? "true" : "false";
    } else if (metric.text.has_value()) {
        value = *metric.text;
    }
    std::string line = metric.label;
    if (line.empty()) {
        line = metric.key;
    }
    line += ": " + value;
    if (!metric.unit.empty()) {
        line += " " + metric.unit;
    }
    return line;
}

/**
 * Minimal deterministic PDF 1.4 layout engine: A4 pages, base-14 Helvetica,
 * text/headings, tables and simple vector line series. No third-party
 * dependency, no timestamp, no document ID, so identical input gives
 * identical bytes.
 */
class pdf_layout_engine {
  public:
    pdf_layout_engine() { pages_.emplace_back(); }

    void title(const std::string_view text) {
        paragraph(text, 2, 18.0, 24.0);
        blank(4.0);
    }

    void heading(const std::string_view text) {
        blank(8.0);
        paragraph(text, 2, 13.0, 17.0);
        blank(3.0);
    }

    void subheading(const std::string_view text) {
        blank(4.0);
        paragraph(text, 2, 10.0, 13.5);
    }

    void paragraph(const std::string_view text, const int font,
                   const double size, const double leading) {
        const auto lines = wrap_text(text, size, body_width);
        for (const auto& line : lines) {
            require(leading);
            write_text(margin_left, cursor_, font, size, line);
            cursor_ += leading;
        }
    }

    void note(const std::string_view text) {
        paragraph("Note: " + std::string(text), 1, 9.0, 12.0);
    }

    void space(const double height) { blank(height); }

    void table(const report_table& value) {
        if (value.columns.empty()) {
            return;
        }
        if (!value.title.empty()) {
            subheading(value.title);
        }
        std::vector<double> weights;
        weights.reserve(value.columns.size());
        double total_weight = 0.0;
        for (const auto& column : value.columns) {
            const double weight = static_cast<double>(
                std::clamp<std::size_t>(
                    text_length(encode_text(column)) + 4U, 8U, 40U));
            weights.push_back(weight);
            total_weight += weight;
        }
        if (total_weight <= 0.0) {
            total_weight = 1.0;
        }
        std::vector<double> widths;
        widths.reserve(weights.size());
        double assigned = 0.0;
        for (std::size_t position = 0; position < weights.size();
             ++position) {
            double width = body_width * weights[position] / total_weight;
            width = std::max(width, 30.0);
            widths.push_back(width);
            assigned += width;
        }
        if (assigned > body_width && !widths.empty()) {
            const double scale = body_width / assigned;
            for (auto& width : widths) {
                width *= scale;
            }
        }
        draw_table_row(value.columns, widths, 2);
        for (const auto& row : value.rows) {
            std::vector<std::string> cells;
            cells.reserve(value.columns.size());
            for (std::size_t column = 0U; column < value.columns.size();
                 ++column) {
                cells.push_back(column < row.size()
                                    ? table_cell_text(row[column])
                                    : std::string{});
            }
            if (cursor_ + 14.0 > body_bottom) {
                new_page();
                draw_table_row(value.columns, widths, 2);
            }
            draw_table_row(cells, widths, 1);
        }
        draw_rule(cursor_);
        cursor_ += 4.0;
    }

    void line_series(const report_series& series) {
        std::string caption = series.title.empty() ? series.key : series.title;
        if (!series.unit.empty()) {
            caption += " (" + series.unit + ")";
        }
        subheading(caption);
        require(96.0);
        const double top = cursor_;
        const double bottom = cursor_ + 88.0;
        const double left = margin_left + 40.0;
        const double right = margin_right;
        draw_box(left, top, right, bottom);

        double minimum_y = std::numeric_limits<double>::infinity();
        double maximum_y = -std::numeric_limits<double>::infinity();
        bool any_value = false;
        for (const double value : series.y) {
            if (std::isfinite(value)) {
                minimum_y = std::min(minimum_y, value);
                maximum_y = std::max(maximum_y, value);
                any_value = true;
            }
        }
        if (!any_value) {
            write_text(left + 6.0, top + 14.0, 1, 9.0, "No data");
            cursor_ = bottom + 10.0;
            return;
        }
        minimum_y = std::min(minimum_y, 0.0);
        maximum_y = std::max(maximum_y, 0.0);
        if (maximum_y - minimum_y < 1e-12) {
            maximum_y = minimum_y + 1.0;
        }
        const std::size_t count = series.y.size();
        bool numeric_x = count == series.x.size() && count > 1U;
        double minimum_x = 0.0;
        double maximum_x = static_cast<double>(count > 1U ? count - 1U : 1U);
        if (numeric_x) {
            minimum_x = std::numeric_limits<double>::infinity();
            maximum_x = -std::numeric_limits<double>::infinity();
            for (const double value : series.x) {
                if (std::isfinite(value)) {
                    minimum_x = std::min(minimum_x, value);
                    maximum_x = std::max(maximum_x, value);
                }
            }
            if (!std::isfinite(minimum_x) ||
                !std::isfinite(maximum_x) ||
                maximum_x - minimum_x < 1e-12) {
                numeric_x = false;
                minimum_x = 0.0;
                maximum_x = static_cast<double>(count - 1U);
            }
        }
        const double span_x = maximum_x - minimum_x;
        const double span_y = maximum_y - minimum_y;
        const auto map_x = [&](const std::size_t position) {
            const double raw =
                numeric_x ? series.x[position] : static_cast<double>(position);
            return left + (raw - minimum_x) / span_x * (right - left);
        };
        const auto map_y = [&](const double value) {
            return bottom - (value - minimum_y) / span_y * (bottom - top);
        };
        std::string path;
        bool first = true;
        for (std::size_t position = 0U; position < count; ++position) {
            if (!std::isfinite(series.y[position])) {
                continue;
            }
            path += number_text(map_x(position)) + " " +
                    number_text(page_height - map_y(series.y[position])) +
                    (first ? " m\n" : " l\n");
            first = false;
        }
        if (!path.empty()) {
            append("0 0.2 0.8 RG\n1 w\n");
            append(path);
            append("S\n");
        }
        write_text(margin_left, top + 4.0, 1, 7.0, number_text(maximum_y));
        write_text(margin_left, bottom - 4.0, 1, 7.0, number_text(minimum_y));
        cursor_ = bottom + 12.0;
    }

    std::string finish() {
        for (auto& page : pages_) {
            page += "BT\n/F1 8 Tf\n1 0 0 1 " + number_text(margin_left) +
                    " 14 Tm\n(Generated by fincore) Tj\nET\n";
        }
        return assemble();
    }

  private:
    void require(const double height) {
        if (cursor_ + height > body_bottom) {
            new_page();
        }
    }

    void new_page() {
        pages_.emplace_back();
        cursor_ = margin_top;
    }

    void blank(const double height) {
        require(height);
        cursor_ += height;
    }

    void append(const std::string_view content) {
        pages_.back() += content;
    }

    void write_text(const double x, const double top_y, const int font,
                    const double size, const std::string_view encoded) {
        if (encoded.empty()) {
            return;
        }
        append("BT\n");
        append(font == 2 ? "/F2 " : "/F1 ");
        append(number_text(size));
        append(" Tf\n1 0 0 1 ");
        append(number_text(x));
        append(" ");
        append(number_text(page_height - top_y));
        append(" Tm\n(");
        append(pdf_string(encoded));
        append(") Tj\nET\n");
    }

    void draw_rule(const double top_y) {
        append("0.7 0.7 0.7 RG\n0.5 w\n");
        append(number_text(margin_left));
        append(" ");
        append(number_text(page_height - top_y));
        append(" m\n");
        append(number_text(margin_right));
        append(" ");
        append(number_text(page_height - top_y));
        append(" l\nS\n");
    }

    void draw_box(const double left, const double top, const double right,
                  const double bottom) {
        append("0.6 0.6 0.6 RG\n0.5 w\n");
        const double y_top = page_height - top;
        const double y_bottom = page_height - bottom;
        append(number_text(left) + " " + number_text(y_top) + " m\n");
        append(number_text(right) + " " + number_text(y_top) + " l\n");
        append(number_text(right) + " " + number_text(y_bottom) + " l\n");
        append(number_text(left) + " " + number_text(y_bottom) + " l\n");
        append("s\n");
    }

    void draw_table_row(const std::vector<std::string>& cells,
                        const std::vector<double>& widths, const int font) {
        constexpr double row_height = 14.0;
        require(row_height);
        double x = margin_left;
        for (std::size_t column = 0U; column < cells.size(); ++column) {
            const double width = widths[column];
            const auto maximum_length = static_cast<std::size_t>(
                std::max((width - 6.0) / (9.0 * 0.5), 4.0));
            std::string encoded = encode_text(cells[column]);
            if (encoded.size() > maximum_length) {
                encoded = encoded.substr(0U, maximum_length);
            }
            write_text(x + 3.0, cursor_ + 10.0, font, 9.0, encoded);
            x += width;
        }
        cursor_ += row_height;
        draw_rule(cursor_);
    }

    std::string assemble() const {
        const std::size_t page_count = pages_.size();
        const std::size_t font_regular = 3U + page_count;
        const std::size_t font_bold = 4U + page_count;
        const std::size_t content_start = 5U + page_count;
        std::vector<std::string> objects;
        objects.reserve(4U + 2U * page_count);
        objects.push_back("<< /Type /Catalog /Pages 2 0 R >>");
        std::string kids = "[";
        for (std::size_t position = 0U; position < page_count; ++position) {
            if (position != 0U) {
                kids.push_back(' ');
            }
            kids += std::to_string(3U + position) + " 0 R";
        }
        kids.push_back(']');
        objects.push_back("<< /Type /Pages /Kids " + kids + " /Count " +
                          std::to_string(page_count) + " >>");
        for (std::size_t position = 0U; position < page_count; ++position) {
            objects.push_back(
                "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
                "/Resources << /Font << /F1 " +
                std::to_string(font_regular) + " 0 R /F2 " +
                std::to_string(font_bold) +
                " 0 R >> >> /Contents " +
                std::to_string(content_start + position) + " 0 R >>");
        }
        objects.push_back(
            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica "
            "/Encoding /WinAnsiEncoding >>");
        objects.push_back(
            "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold "
            "/Encoding /WinAnsiEncoding >>");
        for (const auto& page : pages_) {
            objects.push_back("<< /Length " + std::to_string(page.size()) +
                              " >>\nstream\n" + page + "endstream");
        }

        std::string output = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
        std::vector<std::size_t> offsets(objects.size() + 1U, 0U);
        for (std::size_t position = 0U; position < objects.size();
             ++position) {
            const std::size_t identifier = position + 1U;
            offsets[identifier] = output.size();
            output += std::to_string(identifier) + " 0 obj\n" +
                      objects[position] + "\nendobj\n";
        }
        const std::size_t xref_offset = output.size();
        const std::size_t object_count = objects.size() + 1U;
        output += "xref\n0 " + std::to_string(object_count) + "\n";
        output += "0000000000 65535 f \n";
        char entry[32];
        for (std::size_t identifier = 1U; identifier < object_count;
             ++identifier) {
            std::snprintf(entry, sizeof(entry), "%010zu 00000 n \n",
                          offsets[identifier]);
            output += entry;
        }
        output += "trailer\n<< /Size " + std::to_string(object_count) +
                  " /Root 1 0 R >>\nstartxref\n" +
                  std::to_string(xref_offset) + "\n%%EOF\n";
        return output;
    }

    std::vector<std::string> pages_;
    double cursor_ = margin_top;
};

}  // namespace

std::string render_pdf_bytes(const report_document& document) {
    pdf_layout_engine engine;
    if (!document.title.empty()) {
        engine.title(document.title);
    }
    if (!document.domain.empty()) {
        engine.paragraph("Domain: " + document.domain, 1, 9.0, 12.0);
        engine.space(2.0);
    }
    for (const auto& [key, value] : document.metadata) {
        engine.paragraph(key + ": " + value, 1, 8.0, 10.5);
    }
    for (const auto& section : document.sections) {
        engine.heading(section.title.empty() ? section.key : section.title);
        for (const auto& metric : section.metrics) {
            engine.paragraph(metric_text(metric), 1, 9.5, 13.0);
        }
        for (const auto& note : section.notes) {
            engine.note(note);
        }
        for (const auto& table : section.tables) {
            engine.table(table);
        }
        for (const auto& series : section.series) {
            engine.line_series(series);
        }
    }
    return engine.finish();
}

}  // namespace fincore
