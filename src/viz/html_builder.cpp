#include <fincore/viz/html_builder.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

std::string escape_html(const std::string_view text) {
    // Mirrors Python's `html.escape(text)` with quote=True.
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
                output += "&#x27;";
                break;
            default:
                output.push_back(character);
                break;
        }
    }
    return output;
}

std::string fixed_number(const double value, const int precision) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return std::signbit(value) ? "-inf" : "inf";
    }
    char buffer[64];
    const auto result =
        std::to_chars(buffer, buffer + sizeof(buffer), value,
                      std::chars_format::fixed, precision);
    if (result.ec != std::errc{}) {
        return "nan";
    }
    return std::string(buffer, result.ptr);
}

std::string css_class_for(const double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    if (value > 0.0) {
        return "positive";
    }
    if (value < 0.0) {
        return "negative";
    }
    return {};
}

std::string stats_value_text(const html_metric& metric) {
    if (metric.number.has_value()) {
        return fixed_number(*metric.number, 4);
    }
    // Python prints str(value) for non-float values; an absent value in a
    // numeric Series is a NaN float and prints `nan`.
    return metric.text.has_value() ? *metric.text : "nan";
}

std::string metric_card_value(const html_metric& metric,
                              std::string& css_class) {
    if (metric.number.has_value() && std::isfinite(*metric.number)) {
        css_class = css_class_for(*metric.number);
        return fixed_number(*metric.number, 4);
    }
    // The Python builder emits N/A for any non-finite or non-float entry.
    css_class.clear();
    return "N/A";
}

std::string series_table(const chart_series& series,
                         const std::string_view column_index,
                         const std::string_view column_value,
                         const std::size_t max_rows = 20U) {
    std::vector<std::pair<std::string, std::string>> rows;
    rows.reserve(std::min(series.values.size(), max_rows));
    const auto append_row = [&rows](const std::string& label,
                                    const double value) {
        const std::string date =
            label.size() > 10U ? label.substr(0U, 10U) : label;
        rows.emplace_back(date, fixed_number(value, 6));
    };
    if (series.values.size() > max_rows) {
        const std::size_t half = max_rows / 2U;
        for (std::size_t position = 0U; position < half; ++position) {
            append_row(detail::format_timestamp(series.index[position]),
                       series.values[position]);
        }
        for (std::size_t position = series.values.size() - half;
             position < series.values.size(); ++position) {
            append_row(detail::format_timestamp(series.index[position]),
                       series.values[position]);
        }
    } else {
        for (std::size_t position = 0U; position < series.values.size();
             ++position) {
            append_row(detail::format_timestamp(series.index[position]),
                       series.values[position]);
        }
    }
    std::string table = "<table><tr><th>" + escape_html(column_index) +
                        "</th><th>" + escape_html(column_value) +
                        "</th></tr>\n";
    for (std::size_t position = 0U; position < rows.size(); ++position) {
        if (position != 0U) {
            table.push_back('\n');
        }
        table += "<tr><td>" + escape_html(rows[position].first) + "</td><td>" +
                 escape_html(rows[position].second) + "</td></tr>";
    }
    table += "\n</table>";
    return table;
}

std::string lookup_key(const html_metric& metric) {
    return metric.key;
}

}  // namespace

html_metric html_metric::number_metric(std::string key, const double value) {
    html_metric metric;
    metric.key = std::move(key);
    metric.number = value;
    return metric;
}

html_metric html_metric::text_metric(std::string key, std::string value) {
    html_metric metric;
    metric.key = std::move(key);
    metric.text = std::move(value);
    return metric;
}

html_metric html_metric::missing(std::string key) {
    html_metric metric;
    metric.key = std::move(key);
    return metric;
}

std::string_view html_report_builder::css() {
    static const std::string value =
        "<style>\n"
        "body { font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", "
        "Roboto, sans-serif;\n"
        "       margin: 2em auto; max-width: 960px; color: #333; "
        "line-height: 1.6; }\n"
        "h1 { border-bottom: 2px solid #2563eb; padding-bottom: 0.3em; "
        "color: #1e40af; }\n"
        "h2 { color: #1e40af; margin-top: 1.5em; }\n"
        "table { border-collapse: collapse; width: 100%; margin: 1em 0; }\n"
        "th, td { border: 1px solid #d1d5db; padding: 8px 12px; "
        "text-align: right; }\n"
        "th { background: #eff6ff; font-weight: 600; text-align: left; }\n"
        "tr:nth-child(even) { background: #f9fafb; }\n"
        ".positive { color: #16a34a; }\n"
        ".negative { color: #dc2626; }\n"
        ".metric-card { display: inline-block; border: 1px solid #e5e7eb; "
        "border-radius: 8px;\n"
        "               padding: 12px 20px; margin: 6px; min-width: 140px; "
        "text-align: center; }\n"
        ".metric-card .value { font-size: 1.4em; font-weight: 700; }\n"
        ".metric-card .label { font-size: 0.85em; color: #6b7280; }\n"
        ".footer { margin-top: 2em; padding-top: 1em; border-top: 1px solid "
        "#e5e7eb;\n"
        "          font-size: 0.8em; color: #9ca3af; }\n"
        "</style>\n";
    return value;
}

html_report_builder& html_report_builder::add_title(std::string title) {
    sections_.push_back("<h1>" + escape_html(title) + "</h1>");
    return *this;
}

html_report_builder& html_report_builder::add_heading(std::string text,
                                                      const int level) {
    const int clamped = std::min(std::max(level, 1), 6);
    const std::string tag = "h" + std::to_string(clamped);
    sections_.push_back("<" + tag + ">" + escape_html(text) + "</" + tag +
                        ">");
    return *this;
}

html_report_builder& html_report_builder::add_stats_table(
    const std::vector<html_metric>& stats) {
    std::string table = "<table>\n";
    for (std::size_t position = 0U; position < stats.size(); ++position) {
        if (position != 0U) {
            table.push_back('\n');
        }
        const auto& metric = stats[position];
        const std::string css =
            metric.number.has_value() ? css_class_for(*metric.number)
                                      : std::string{};
        table += "<tr><th>" + escape_html(metric.key) + "</th><td class=\"" +
                 css + "\">" + stats_value_text(metric) + "</td></tr>";
    }
    table += "\n</table>";
    sections_.push_back(std::move(table));
    return *this;
}

html_report_builder& html_report_builder::add_metric_cards(
    const std::vector<html_metric>& stats) {
    std::vector<std::string> keys;
    keys.reserve(stats.size());
    for (const auto& metric : stats) {
        keys.push_back(lookup_key(metric));
    }
    return add_metric_cards(stats, keys);
}

html_report_builder& html_report_builder::add_metric_cards(
    const std::vector<html_metric>& stats,
    const std::vector<std::string>& keys) {
    std::string container = "<div>";
    for (std::size_t position = 0U; position < keys.size(); ++position) {
        if (position != 0U) {
            container.push_back('\n');
        }
        const html_metric* found = nullptr;
        for (const auto& metric : stats) {
            if (metric.key == keys[position]) {
                found = &metric;
                break;
            }
        }
        const html_metric missing = html_metric::missing(keys[position]);
        const html_metric& metric = found == nullptr ? missing : *found;
        std::string css;
        const std::string formatted = metric_card_value(metric, css);
        container += "<div class=\"metric-card\">";
        container += "<div class=\"value " + css + "\">" + formatted + "</div>";
        container += "<div class=\"label\">" + escape_html(metric.key) +
                     "</div>";
        container += "</div>";
    }
    container += "</div>";
    sections_.push_back(std::move(container));
    return *this;
}

html_report_builder& html_report_builder::add_html(std::string raw_html) {
    sections_.push_back(std::move(raw_html));
    return *this;
}

html_report_builder& html_report_builder::plot_returns(
    const chart_series& series) {
    add_heading("Cumulative Returns");
    add_html(series_table(series, "Date", "Cum Return"));
    return *this;
}

html_report_builder& html_report_builder::plot_drawdown(
    const chart_series& series) {
    add_heading("Drawdown");
    add_html(series_table(series, "Date", "Drawdown"));
    return *this;
}

html_report_builder& html_report_builder::plot_rolling_sharpe(
    const chart_series& sharpe, const chart_series* benchmark,
    const std::size_t window) {
    add_heading("Rolling Sharpe Ratio (" + std::to_string(window) +
                "-day window)");
    add_html(series_table(sharpe, "Date", "Sharpe"));
    if (benchmark != nullptr) {
        add_heading("Benchmark Rolling Sharpe Ratio", 3);
        add_html(series_table(*benchmark, "Date", "Sharpe"));
    }
    return *this;
}

html_report_builder& html_report_builder::plot_monthly_heatmap(
    const heat_grid& grid) {
    if (grid.values.size() != grid.years.size() * 12U) {
        throw input_error{"monthly heatmap values must be years x 12"};
    }
    add_heading("Monthly Returns");
    std::string table = "<table class=\"dataframe monthly\">\n";
    table += "  <thead>\n    <tr><th></th>";
    for (const auto& column : grid.columns) {
        table += "<th>" + escape_html(column) + "</th>";
    }
    table += "</tr>\n  </thead>\n  <tbody>\n";
    for (std::size_t year = 0U; year < grid.years.size(); ++year) {
        table += "    <tr><th>" + std::to_string(grid.years[year]) +
                 "</th>";
        for (std::size_t month = 0U; month < 12U; ++month) {
            const std::size_t cell = year * 12U + month;
            const bool present =
                cell < grid.present.size() ? grid.present[cell]
                                           : std::isfinite(grid.values[cell]);
            if (!present || !std::isfinite(grid.values[cell])) {
                table += "<td>NaN</td>";
            } else {
                table += "<td>" + fixed_number(grid.values[cell], 2) +
                         "%</td>";
            }
        }
        table += "</tr>\n";
    }
    table += "  </tbody>\n</table>";
    add_html(std::move(table));
    return *this;
}

std::string html_report_builder::build() const {
    std::string body;
    for (std::size_t position = 0U; position < sections_.size(); ++position) {
        if (position != 0U) {
            body.push_back('\n');
        }
        body += sections_[position];
    }
    static constexpr std::string_view footer =
        "<div class=\"footer\">Generated by <strong>fincore</strong></div>";
    std::string html;
    html += "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n";
    html += "<title>Performance Report</title>\n";
    html += css();
    html += "\n</head>\n<body>\n";
    html += body;
    html += "\n";
    html += footer;
    html += "\n</body></html>";
    return html;
}

void html_report_builder::save(const std::string& path) const {
    if (path.empty()) {
        throw input_error{"html report save path must not be empty"};
    }
    const std::string html = build();
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw input_error{"html report save could not open: " + path};
    }
    stream.write(html.data(), static_cast<std::streamsize>(html.size()));
    if (!stream) {
        throw input_error{"html report save could not write: " + path};
    }
}

}  // namespace fincore
