#include <fincore/factor/artifacts.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double quiet_nan = std::numeric_limits<double>::quiet_NaN();
constexpr double pi = 3.14159265358979323846;

// The classic locale is imposed rather than inherited: these bytes are compared
// and hashed, and a host whose default locale uses a comma as the decimal separator
// would otherwise change them. The negative-zero rewrite stops a value that rounded
// to zero from carrying a sign no other zero has.
std::string format_number(const double value) {
    if (!std::isfinite(value)) {
        return "0";
    }
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(6) << std::defaultfloat << value;
    std::string text = stream.str();
    if (text == "-0") {
        text = "0";
    }
    return text;
}

std::string escape_xml(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        switch (character) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&apos;";
                break;
            default:
                result.push_back(character);
                break;
        }
    }
    return result;
}

struct svg_frame {
    static constexpr double width = 1800.0;
    static constexpr double height = 600.0;
    static constexpr double left = 96.0;
    static constexpr double right = 48.0;
    static constexpr double top = 72.0;
    static constexpr double bottom = 64.0;

    std::string title;
    std::string x_label;
    std::string y_label;
    std::string body;

    std::string render() const {
        std::string output;
        output += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
                  format_number(width) + "\" height=\"" +
                  format_number(height) + "\" viewBox=\"0 0 " +
                  format_number(width) + " " + format_number(height) +
                  "\">\n";
        output += "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
        output += "<text x=\"" + format_number(width / 2.0) +
                  "\" y=\"32\" text-anchor=\"middle\" font-family=\"sans-serif\" "
                  "font-size=\"18\" fill=\"#222222\">" +
                  escape_xml(title) + "</text>\n";
        output += "<rect x=\"" + format_number(left) + "\" y=\"" +
                  format_number(top) + "\" width=\"" +
                  format_number(width - left - right) + "\" height=\"" +
                  format_number(height - top - bottom) +
                  "\" fill=\"none\" stroke=\"#444444\" stroke-width=\"1\"/>\n";
        output += body;
        if (!x_label.empty()) {
            output += "<text x=\"" + format_number(width / 2.0) + "\" y=\"" +
                      format_number(height - 16.0) +
                      "\" text-anchor=\"middle\" font-family=\"sans-serif\" "
                      "font-size=\"13\" fill=\"#333333\">" +
                      escape_xml(x_label) + "</text>\n";
        }
        if (!y_label.empty()) {
            output += "<text x=\"20\" y=\"" + format_number(height / 2.0) +
                      "\" transform=\"rotate(-90 20 " +
                      format_number(height / 2.0) +
                      ")\" text-anchor=\"middle\" font-family=\"sans-serif\" "
                      "font-size=\"13\" fill=\"#333333\">" +
                      escape_xml(y_label) + "</text>\n";
        }
        output += "</svg>\n";
        return output;
    }
};

double map_x(const double value, const double minimum, const double maximum) {
    const double span = maximum - minimum;
    const double fraction = span == 0.0 ? 0.5 : (value - minimum) / span;
    return svg_frame::left +
           fraction * (svg_frame::width - svg_frame::left - svg_frame::right);
}

double map_y(const double value, const double minimum, const double maximum) {
    const double span = maximum - minimum;
    const double fraction = span == 0.0 ? 0.5 : (value - minimum) / span;
    return svg_frame::height - svg_frame::bottom -
           fraction * (svg_frame::height - svg_frame::top - svg_frame::bottom);
}

std::string polyline(const std::vector<double>& xs,
                     const std::vector<double>& ys, const std::string& color,
                     const double width, const double opacity = 1.0) {
    std::string output = "<polyline fill=\"none\" stroke=\"" + color +
                         "\" stroke-width=\"" + format_number(width) +
                         "\" stroke-opacity=\"" + format_number(opacity) +
                         "\" points=\"";
    for (std::size_t index = 0; index < xs.size(); ++index) {
        if (index != 0U) {
            output.push_back(' ');
        }
        output += format_number(xs[index]);
        output.push_back(',');
        output += format_number(ys[index]);
    }
    output += "\"/>\n";
    return output;
}

const char* series_color(const std::size_t index) {
    static constexpr const char* colors[] = {
        "#4C72B0", "#DD8452", "#55A868", "#C44E52", "#8172B3",
        "#937860", "#DA8BC3", "#8C8C8C", "#CCB974", "#64B5CD"};
    return colors[index % 10U];
}

std::pair<int, int> year_month(const timestamp value) {
    const std::string text = detail::format_timestamp(value);
    if (text.size() < 7U) {
        return {0, 1};
    }
    try {
        return {std::stoi(text.substr(0U, 4U)),
                std::stoi(text.substr(5U, 2U))};
    } catch (const std::exception&) {
        return {0, 1};
    }
}

double percentile(const std::vector<double>& sorted, const double fraction) {
    if (sorted.empty()) {
        return quiet_nan;
    }
    if (sorted.size() == 1U) {
        return sorted.front();
    }
    const double position =
        fraction * static_cast<double>(sorted.size() - 1U);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    const double weight = position - static_cast<double>(lower);
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

double finite_abs_percentile(const std::vector<double>& values,
                             const double fraction = 0.95) {
    std::vector<double> finite;
    for (const double value : values) {
        if (std::isfinite(value)) {
            finite.push_back(std::abs(value));
        }
    }
    if (finite.empty()) {
        return 1.0;
    }
    std::sort(finite.begin(), finite.end());
    const double result = percentile(finite, fraction);
    return result > 0.0 ? result : 1.0;
}

/// Acklam's standard-normal inverse CDF.
double normal_ppf(const double probability) {
    static constexpr double a[] = {
        -3.969683028665376e+01, 2.209460984245205e+02,
        -2.759285104469687e+02, 1.383577518672690e+02,
        -3.066479806614716e+01, 2.506628277459239e+00};
    static constexpr double b[] = {
        -5.447609879822406e+01, 1.615858368580409e+02,
        -1.556989798598866e+02, 6.680131188771972e+01,
        -1.328068155288572e+01};
    static constexpr double c[] = {
        -7.784894002430293e-03, -3.223964580411365e-01,
        -2.400758277161838e+00, -2.549732539343734e+00,
        4.374664141464968e+00, 2.938163982698783e+00};
    static constexpr double d[] = {
        7.784695709041462e-03, 3.224671290700398e-01,
        2.445134137142996e+00, 3.754408661907416e+00};
    constexpr double lower = 0.02425;
    constexpr double upper = 1.0 - lower;
    if (probability <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    if (probability >= 1.0) {
        return std::numeric_limits<double>::infinity();
    }
    if (probability < lower) {
        const double q = std::sqrt(-2.0 * std::log(probability));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q +
                c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    if (probability > upper) {
        const double q = std::sqrt(-2.0 * std::log(1.0 - probability));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q +
                 c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    }
    const double q = probability - 0.5;
    const double r = q * q;
    return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r +
            a[5]) *
           q /
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r +
            1.0);
}

std::vector<timestamp> unique_dates(const std::vector<timestamp>& dates) {
    std::vector<timestamp> result = dates;
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<double> aggregate_by_date(const quantile_date_return_table& table,
                                      const std::size_t quantile_index,
                                      const std::size_t label_index,
                                      const std::vector<timestamp>& dates) {
    const std::size_t quantiles = table.quantiles.size();
    const std::size_t labels = table.labels.size();
    std::vector<double> result(dates.size(), quiet_nan);
    if (quantiles == 0U || labels == 0U) {
        return result;
    }
    std::vector<std::vector<double>> buckets(dates.size());
    for (std::size_t row = 0; row < table.dates.size(); ++row) {
        const auto found = std::lower_bound(dates.begin(), dates.end(),
                                            table.dates[row]);
        if (found == dates.end() || *found != table.dates[row]) {
            continue;
        }
        const auto position =
            static_cast<std::size_t>(found - dates.begin());
        const double value =
            table.means[(row * quantiles + quantile_index) * labels +
                        label_index];
        if (std::isfinite(value)) {
            buckets[position].push_back(value);
        }
    }
    for (std::size_t index = 0; index < dates.size(); ++index) {
        if (!buckets[index].empty()) {
            result[index] =
                std::accumulate(buckets[index].begin(), buckets[index].end(),
                                0.0) /
                static_cast<double>(buckets[index].size());
        }
    }
    return result;
}

std::size_t label_position(const quantile_date_return_table& table,
                           const std::string& period) {
    const auto found = std::find(table.labels.begin(), table.labels.end(),
                                 period);
    if (found == table.labels.end()) {
        throw factor_data_error{"period label is not present in the returns table"};
    }
    return static_cast<std::size_t>(found - table.labels.begin());
}

}  // namespace

style_context plotting_context(std::string context, const double font_scale,
                               std::map<std::string, std::string> rc) {
    if (!rc.contains("lines.linewidth")) {
        rc["lines.linewidth"] = "1.5";
    }
    return style_context{std::move(context), font_scale, std::move(rc)};
}

style_context axes_style(std::string style,
                         std::map<std::string, std::string> rc) {
    return style_context{std::move(style), 1.5, std::move(rc)};
}

render_customization customize(const bool set_context) {
    render_customization customization;
    customization.set_context = set_context;
    customization.context = plotting_context();
    customization.axes = axes_style();
    return customization;
}

quantile_cumulative_chart build_cumulative_returns_by_quantile(
    const quantile_date_return_table& returns, std::string period) {
    const std::size_t label = label_position(returns, period);
    const std::vector<timestamp> dates = unique_dates(returns.dates);
    std::vector<int> quantiles = returns.quantiles;
    std::sort(quantiles.begin(), quantiles.end());
    quantiles.erase(std::unique(quantiles.begin(), quantiles.end()),
                    quantiles.end());
    std::reverse(quantiles.begin(), quantiles.end());

    quantile_cumulative_chart chart;
    chart.period = std::move(period);
    chart.title = "Cumulative Return by Quantile\n(" + chart.period +
                  " Period Forward Return)";
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const int quantile : quantiles) {
        cumulative_quantile_series series;
        series.quantile = quantile;
        series.dates = dates;
        const auto position =
            std::find(returns.quantiles.begin(), returns.quantiles.end(),
                      quantile);
        const auto quantile_index =
            static_cast<std::size_t>(position - returns.quantiles.begin());
        const std::vector<double> means =
            aggregate_by_date(returns, quantile_index, label, dates);
        double growth = 1.0;
        series.values.reserve(dates.size());
        for (const double mean : means) {
            growth *= 1.0 + (std::isfinite(mean) ? mean : 0.0);
            series.values.push_back(growth);
            minimum = std::min(minimum, growth);
            maximum = std::max(maximum, growth);
        }
        chart.series.push_back(std::move(series));
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        minimum = 0.0;
        maximum = 1.0;
    }
    if (minimum == maximum) {
        minimum -= 1.0;
        maximum += 1.0;
    }
    chart.y_min = minimum;
    chart.y_max = maximum;
    return chart;
}

events_distribution_chart build_events_distribution(
    const std::vector<timestamp>& dates, const std::vector<double>& values,
    const std::size_t num_bars) {
    if (num_bars == 0U) {
        throw factor_data_error{"num_bars must be a positive integer"};
    }
    if (!values.empty() && values.size() != dates.size()) {
        throw factor_data_error{
            "events dates and values must have equal lengths"};
    }

    events_distribution_chart chart;
    chart.requested_bars = num_bars;
    if (dates.empty()) {
        return chart;
    }
    const timestamp start = *std::min_element(dates.begin(), dates.end());
    const timestamp end = *std::max_element(dates.begin(), dates.end());
    if (start == end) {
        std::size_t count = 0U;
        for (std::size_t index = 0; index < dates.size(); ++index) {
            if (values.empty() || std::isfinite(values[index])) {
                ++count;
            }
        }
        chart.bucket_starts.push_back(start);
        chart.counts.push_back(static_cast<double>(count));
        return chart;
    }

    const std::int64_t interval =
        (end - start) / static_cast<std::int64_t>(num_bars);
    if (interval <= 0) {
        throw factor_data_error{"event bucket interval must be positive"};
    }
    chart.bucket_starts.reserve(num_bars + 1U);
    chart.counts.assign(num_bars + 1U, 0.0);
    for (std::size_t bucket = 0U; bucket <= num_bars; ++bucket) {
        chart.bucket_starts.push_back(
            start + static_cast<timestamp>(bucket) * interval);
    }
    for (std::size_t index = 0; index < dates.size(); ++index) {
        if (!values.empty() && !std::isfinite(values[index])) {
            continue;
        }
        const std::int64_t offset = dates[index] - start;
        std::size_t bucket =
            static_cast<std::size_t>(offset / interval);
        if (bucket > num_bars) {
            bucket = num_bars;
        }
        // The final bucket also covers the closed right endpoint.
        if (dates[index] == end) {
            bucket = num_bars;
        }
        chart.counts[bucket] += 1.0;
    }
    return chart;
}

std::string events_distribution_chart::to_svg() const {
    svg_frame frame;
    frame.title = title;
    frame.x_label = x_label;
    frame.y_label = y_label;
    if (counts.empty()) {
        return frame.render();
    }
    double maximum = 0.0;
    for (const double count : counts) {
        maximum = std::max(maximum, count);
    }
    if (maximum <= 0.0) {
        maximum = 1.0;
    }
    const double area_width =
        svg_frame::width - svg_frame::left - svg_frame::right;
    const double area_height =
        svg_frame::height - svg_frame::top - svg_frame::bottom;
    const double bar_width =
        area_width / static_cast<double>(counts.size()) * 0.8;
    for (std::size_t index = 0; index < counts.size(); ++index) {
        const double height = area_height * counts[index] / maximum;
        const double x = svg_frame::left +
                         (static_cast<double>(index) + 0.1) *
                             area_width / static_cast<double>(counts.size());
        const double y = svg_frame::height - svg_frame::bottom - height;
        frame.body += "<rect x=\"" + format_number(x) + "\" y=\"" +
                      format_number(y) + "\" width=\"" +
                      format_number(bar_width) + "\" height=\"" +
                      format_number(height) +
                      "\" fill=\"#4C72B0\"/>\n";
    }
    return frame.render();
}

std::string quantile_cumulative_chart::to_svg() const {
    svg_frame frame;
    frame.title = title;
    frame.y_label = y_label;
    std::vector<double> xs;
    std::vector<double> ys;
    for (const cumulative_quantile_series& series : series) {
        for (const double value : series.values) {
            if (!std::isfinite(value)) {
                continue;
            }
            ys.push_back(value);
        }
    }
    double x_min = 0.0;
    double x_max = 1.0;
    if (!series.empty() && !series.front().dates.empty()) {
        x_min = static_cast<double>(series.front().dates.front());
        x_max = static_cast<double>(series.front().dates.back());
    }
    if (x_min == x_max) {
        x_max = x_min + 1.0;
    }
    for (std::size_t index = 0; index < series.size(); ++index) {
        xs.clear();
        ys.clear();
        for (std::size_t row = 0; row < series[index].dates.size(); ++row) {
            const double value = series[index].values[row];
            if (!std::isfinite(value)) {
                continue;
            }
            xs.push_back(map_x(static_cast<double>(series[index].dates[row]),
                               x_min, x_max));
            ys.push_back(map_y(value, y_min, y_max));
        }
        if (!xs.empty()) {
            frame.body += polyline(xs, ys, series_color(index), 2.0);
        }
    }
    if (reference >= y_min && reference <= y_max) {
        const double y = map_y(reference, y_min, y_max);
        frame.body += "<line x1=\"" + format_number(svg_frame::left) +
                      "\" y1=\"" + format_number(y) + "\" x2=\"" +
                      format_number(svg_frame::width - svg_frame::right) +
                      "\" y2=\"" + format_number(y) +
                      "\" stroke=\"#000000\" stroke-width=\"1\"/>\n";
    }
    return frame.render();
}

std::vector<qq_chart> build_ic_qq(const factor_ic_table& ic) {
    std::vector<qq_chart> charts;
    if (ic.dates.empty() || ic.labels.empty()) {
        return charts;
    }
    const std::size_t rows = ic.values.size() / ic.labels.size();
    for (std::size_t label = 0; label < ic.labels.size(); ++label) {
        qq_chart chart;
        qq_series series;
        series.period = ic.labels[label];
        std::vector<double> values;
        values.reserve(rows);
        for (std::size_t row = 0; row < rows; ++row) {
            const double value = ic.values[row * ic.labels.size() + label];
            values.push_back(std::isfinite(value) ? value : 0.0);
        }
        if (values.empty()) {
            continue;
        }
        std::sort(values.begin(), values.end());
        const double mean =
            std::accumulate(values.begin(), values.end(), 0.0) /
            static_cast<double>(values.size());
        double variance = 0.0;
        for (const double value : values) {
            variance += (value - mean) * (value - mean);
        }
        const double scale = std::sqrt(variance / static_cast<double>(values.size()));
        series.mean = mean;
        series.scale = scale;
        for (std::size_t index = 0; index < values.size(); ++index) {
            const double probability =
                static_cast<double>(index + 1U) /
                static_cast<double>(values.size() + 1U);
            series.theoretical_quantiles.push_back(normal_ppf(probability));
            const bool standardize = mean != 0.0 && scale != 1.0 && scale > 0.0;
            series.observed_quantiles.push_back(
                standardize ? (values[index] - mean) / scale : values[index]);
        }
        chart.series.push_back(std::move(series));
        charts.push_back(std::move(chart));
    }
    return charts;
}

std::string qq_chart::to_svg() const {
    svg_frame frame;
    frame.title = "IC Q-Q";
    frame.x_label = x_label;
    frame.y_label = y_label;
    if (series.empty()) {
        return frame.render();
    }
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const qq_series& entry : series) {
        for (std::size_t index = 0; index < entry.observed_quantiles.size();
             ++index) {
            minimum = std::min({minimum, entry.theoretical_quantiles[index],
                                entry.observed_quantiles[index]});
            maximum = std::max({maximum, entry.theoretical_quantiles[index],
                                entry.observed_quantiles[index]});
        }
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        minimum = -1.0;
        maximum = 1.0;
    }
    if (minimum == maximum) {
        minimum -= 1.0;
        maximum += 1.0;
    }
    const double pad = (maximum - minimum) * 0.05;
    minimum -= pad;
    maximum += pad;
    frame.body += "<line x1=\"" + format_number(map_x(minimum, minimum, maximum)) +
                  "\" y1=\"" + format_number(map_y(minimum, minimum, maximum)) +
                  "\" x2=\"" + format_number(map_x(maximum, minimum, maximum)) +
                  "\" y2=\"" + format_number(map_y(maximum, minimum, maximum)) +
                  "\" stroke=\"#000000\" stroke-width=\"1\"/>\n";
    for (std::size_t entry_index = 0; entry_index < series.size();
         ++entry_index) {
        const qq_series& entry = series[entry_index];
        for (std::size_t index = 0; index < entry.observed_quantiles.size();
             ++index) {
            frame.body += "<circle cx=\"" +
                          format_number(map_x(entry.theoretical_quantiles[index],
                                              minimum, maximum)) +
                          "\" cy=\"" +
                          format_number(map_y(entry.observed_quantiles[index],
                                              minimum, maximum)) +
                          "\" r=\"2.5\" fill=\"" +
                          series_color(entry_index) + "\"/>\n";
        }
    }
    return frame.render();
}

spread_time_series_chart build_mean_quantile_returns_spread_time_series(
    const std::vector<timestamp>& dates, const std::vector<double>& spreads,
    std::string period, const std::vector<double>& standard_errors,
    const double bandwidth) {
    if (dates.size() != spreads.size()) {
        throw factor_data_error{
            "spread dates and values must have equal lengths"};
    }
    if (!standard_errors.empty() && standard_errors.size() != spreads.size()) {
        throw factor_data_error{
            "spread standard errors must align with values"};
    }
    spread_time_series_chart chart;
    chart.period = std::move(period);
    chart.title = "Top Minus Bottom Quantile Mean Return (" + chart.period +
                  " Period Forward Return)";
    chart.dates = dates;
    chart.bandwidth = bandwidth;
    chart.spreads.reserve(spreads.size());
    for (const double value : spreads) {
        chart.spreads.push_back(std::isfinite(value) ? value * DECIMAL_TO_BPS
                                                     : quiet_nan);
    }
    chart.rolling_mean.assign(chart.spreads.size(), quiet_nan);
    constexpr std::size_t window = 22U;
    for (std::size_t index = window - 1U; index < chart.spreads.size();
         ++index) {
        double total = 0.0;
        bool finite = true;
        for (std::size_t offset = index - window + 1U; offset <= index;
             ++offset) {
            if (!std::isfinite(chart.spreads[offset])) {
                finite = false;
                break;
            }
            total += chart.spreads[offset];
        }
        if (finite) {
            chart.rolling_mean[index] =
                total / static_cast<double>(window);
        }
    }
    if (!standard_errors.empty()) {
        chart.band_lower.reserve(spreads.size());
        chart.band_upper.reserve(spreads.size());
        for (std::size_t index = 0; index < spreads.size(); ++index) {
            if (!std::isfinite(chart.spreads[index]) ||
                !std::isfinite(standard_errors[index])) {
                chart.band_lower.push_back(quiet_nan);
                chart.band_upper.push_back(quiet_nan);
                continue;
            }
            const double error =
                standard_errors[index] * DECIMAL_TO_BPS * bandwidth;
            chart.band_lower.push_back(chart.spreads[index] - error);
            chart.band_upper.push_back(chart.spreads[index] + error);
        }
    }
    chart.y_limit = finite_abs_percentile(chart.spreads);
    return chart;
}

std::string spread_time_series_chart::to_svg() const {
    svg_frame frame;
    frame.title = title;
    frame.y_label = y_label;
    if (dates.empty()) {
        return frame.render();
    }
    const double x_min = static_cast<double>(dates.front());
    const double x_max = dates.size() == 1U
                             ? x_min + 1.0
                             : static_cast<double>(dates.back());
    const double y_min = -y_limit;
    const double y_max = y_limit;
    if (!band_lower.empty()) {
        std::string polygon = "<polygon fill=\"#4C72B0\" fill-opacity=\"0.25\" points=\"";
        for (std::size_t index = 0; index < dates.size(); ++index) {
            if (!std::isfinite(band_upper[index])) {
                continue;
            }
            polygon += format_number(
                           map_x(static_cast<double>(dates[index]), x_min,
                                 x_max)) +
                       "," +
                       format_number(map_y(band_upper[index], y_min, y_max)) +
                       " ";
        }
        for (std::size_t index = dates.size(); index-- > 0U;) {
            if (!std::isfinite(band_lower[index])) {
                continue;
            }
            polygon += format_number(
                           map_x(static_cast<double>(dates[index]), x_min,
                                 x_max)) +
                       "," +
                       format_number(map_y(band_lower[index], y_min, y_max)) +
                       " ";
        }
        polygon += "\"/>\n";
        frame.body += polygon;
    }
    std::vector<double> xs;
    std::vector<double> ys;
    for (std::size_t index = 0; index < dates.size(); ++index) {
        if (!std::isfinite(spreads[index])) {
            continue;
        }
        xs.push_back(map_x(static_cast<double>(dates[index]), x_min, x_max));
        ys.push_back(map_y(spreads[index], y_min, y_max));
    }
    if (!xs.empty()) {
        frame.body += polyline(xs, ys, "#55A868", 0.8, 0.4);
    }
    xs.clear();
    ys.clear();
    for (std::size_t index = 0; index < dates.size(); ++index) {
        if (!std::isfinite(rolling_mean[index])) {
            continue;
        }
        xs.push_back(map_x(static_cast<double>(dates[index]), x_min, x_max));
        ys.push_back(map_y(rolling_mean[index], y_min, y_max));
    }
    if (!xs.empty()) {
        frame.body += polyline(xs, ys, "#DD8452", 1.5, 0.7);
    }
    frame.body += "<line x1=\"" + format_number(svg_frame::left) +
                  "\" y1=\"" + format_number(map_y(0.0, y_min, y_max)) +
                  "\" x2=\"" +
                  format_number(svg_frame::width - svg_frame::right) +
                  "\" y2=\"" + format_number(map_y(0.0, y_min, y_max)) +
                  "\" stroke=\"#000000\" stroke-width=\"1\"/>\n";
    return frame.render();
}

std::vector<monthly_ic_heatmap_chart> build_monthly_ic_heatmap(
    const factor_time_information_table& monthly) {
    std::vector<monthly_ic_heatmap_chart> charts;
    const std::size_t labels = monthly.labels.size();
    if (labels == 0U || monthly.dates.empty()) {
        return charts;
    }
    std::vector<std::string> groups;
    if (!monthly.groups.empty()) {
        groups = monthly.groups;
        std::sort(groups.begin(), groups.end());
        groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
    } else {
        groups.emplace_back();
    }
    for (const std::string& group : groups) {
        for (std::size_t label = 0; label < labels; ++label) {
            monthly_ic_heatmap_chart chart;
            chart.period = monthly.labels[label];
            chart.group = group;
            chart.title = "Monthly Mean " + chart.period + " Period IC";
            chart.month_columns.reserve(12U);
            for (const std::string_view month : month_labels) {
                chart.month_columns.emplace_back(month);
            }
            std::map<int, std::vector<double>> by_year;
            for (std::size_t row = 0; row < monthly.dates.size(); ++row) {
                if (!monthly.groups.empty() &&
                    monthly.groups[row] != group) {
                    continue;
                }
                const auto [year, month] = year_month(monthly.dates[row]);
                auto& values = by_year[year];
                if (values.empty()) {
                    values.assign(12U, quiet_nan);
                }
                const double value =
                    monthly.means[row * labels + label];
                if (std::isfinite(value) && month >= 1 && month <= 12) {
                    values[static_cast<std::size_t>(month - 1)] = value;
                }
            }
            for (const auto& [year, values] : by_year) {
                chart.years.push_back(year);
                for (const double value : values) {
                    chart.values.push_back(value);
                    chart.present.push_back(std::isfinite(value));
                }
            }
            charts.push_back(std::move(chart));
        }
    }
    return charts;
}

std::string monthly_ic_heatmap_chart::to_svg() const {
    svg_frame frame;
    frame.title = title;
    const double area_width =
        svg_frame::width - svg_frame::left - svg_frame::right;
    const double area_height =
        svg_frame::height - svg_frame::top - svg_frame::bottom;
    if (years.empty()) {
        return frame.render();
    }
    const double cell_width = area_width / 12.0;
    const double cell_height = area_height / static_cast<double>(years.size());
    double maximum = 0.0;
    for (const double value : values) {
        if (std::isfinite(value)) {
            maximum = std::max(maximum, std::abs(value));
        }
    }
    if (maximum == 0.0) {
        maximum = 1.0;
    }
    for (std::size_t year_index = 0; year_index < years.size();
         ++year_index) {
        for (std::size_t month = 0; month < 12U; ++month) {
            const std::size_t index = year_index * 12U + month;
            if (index >= values.size() || !present[index] ||
                !std::isfinite(values[index])) {
                continue;
            }
            const double fraction = values[index] / maximum;
            const int red = fraction >= 0.0
                                ? 255
                                : static_cast<int>(255.0 * (1.0 + fraction));
            const int blue = fraction <= 0.0
                                 ? 255
                                 : static_cast<int>(255.0 * (1.0 - fraction));
            const std::string color =
                "rgb(" + std::to_string(red) + ",120," +
                std::to_string(blue) + ")";
            frame.body += "<rect x=\"" +
                          format_number(svg_frame::left +
                                        static_cast<double>(month) * cell_width) +
                          "\" y=\"" +
                          format_number(svg_frame::top +
                                        static_cast<double>(year_index) *
                                            cell_height) +
                          "\" width=\"" + format_number(cell_width) +
                          "\" height=\"" + format_number(cell_height) +
                          "\" fill=\"" + color + "\"/>\n";
        }
    }
    return frame.render();
}

violin_chart build_quantile_returns_violin(
    const quantile_date_return_table& returns, std::string period) {
    const std::size_t label = label_position(returns, period);
    violin_chart chart;
    std::vector<int> quantiles = returns.quantiles;
    std::sort(quantiles.begin(), quantiles.end());
    quantiles.erase(std::unique(quantiles.begin(), quantiles.end()),
                    quantiles.end());
    const std::size_t labels = returns.labels.size();
    for (const int quantile : quantiles) {
        violin_group group;
        group.quantile = quantile;
        group.period = period;
        const auto position =
            std::find(returns.quantiles.begin(), returns.quantiles.end(),
                      quantile);
        const auto quantile_index =
            static_cast<std::size_t>(position - returns.quantiles.begin());
        for (std::size_t row = 0; row < returns.dates.size(); ++row) {
            const double value =
                returns.means[(row * returns.quantiles.size() +
                               quantile_index) *
                                  labels +
                              label];
            if (std::isfinite(value)) {
                group.samples.push_back(value * DECIMAL_TO_BPS);
            }
        }
        if (group.samples.empty()) {
            continue;
        }
        std::vector<double> sorted = group.samples;
        std::sort(sorted.begin(), sorted.end());
        group.first_quartile = percentile(sorted, 0.25);
        group.median = percentile(sorted, 0.50);
        group.third_quartile = percentile(sorted, 0.75);
        if (sorted.size() >= 2U) {
            const double mean =
                std::accumulate(sorted.begin(), sorted.end(), 0.0) /
                static_cast<double>(sorted.size());
            double variance = 0.0;
            for (const double value : sorted) {
                variance += (value - mean) * (value - mean);
            }
            const double deviation =
                std::sqrt(variance / static_cast<double>(sorted.size()));
            if (deviation > 0.0) {
                const double bandwidth =
                    deviation *
                    std::pow(static_cast<double>(sorted.size()), -0.2);
                constexpr std::size_t points = 64U;
                for (std::size_t index = 0; index < points; ++index) {
                    const double fraction =
                        static_cast<double>(index) /
                        static_cast<double>(points - 1U);
                    const double x =
                        sorted.front() +
                        fraction * (sorted.back() - sorted.front());
                    double density = 0.0;
                    for (const double value : sorted) {
                        const double z = (x - value) / bandwidth;
                        density += std::exp(-0.5 * z * z);
                    }
                    density /= static_cast<double>(sorted.size()) *
                               bandwidth * std::sqrt(2.0 * pi);
                    group.density_x.push_back(x);
                    group.density_y.push_back(density);
                }
            }
        }
        chart.groups.push_back(std::move(group));
    }
    return chart;
}

std::string violin_chart::to_svg() const {
    svg_frame frame;
    frame.title = title;
    frame.y_label = y_label;
    if (groups.empty()) {
        return frame.render();
    }
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const violin_group& group : groups) {
        for (const double value : group.samples) {
            minimum = std::min(minimum, value);
            maximum = std::max(maximum, value);
        }
    }
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) {
        return frame.render();
    }
    if (minimum == maximum) {
        minimum -= 1.0;
        maximum += 1.0;
    }
    const double area_width =
        svg_frame::width - svg_frame::left - svg_frame::right;
    const double column_width =
        area_width / static_cast<double>(groups.size());
    for (std::size_t index = 0; index < groups.size(); ++index) {
        const violin_group& group = groups[index];
        if (group.density_x.empty()) {
            continue;
        }
        double maximum_density = 0.0;
        for (const double density : group.density_y) {
            maximum_density = std::max(maximum_density, density);
        }
        if (maximum_density <= 0.0) {
            continue;
        }
        const double center =
            svg_frame::left + (static_cast<double>(index) + 0.5) * column_width;
        std::string polygon =
            "<polygon fill=\"" + std::string(series_color(index)) +
            "\" fill-opacity=\"0.45\" points=\"";
        for (std::size_t point = 0; point < group.density_x.size(); ++point) {
            const double half_width =
                0.45 * column_width * group.density_y[point] / maximum_density;
            polygon += format_number(center + half_width) + "," +
                       format_number(map_y(group.density_x[point], minimum,
                                           maximum)) +
                       " ";
        }
        for (std::size_t point = group.density_x.size(); point-- > 0U;) {
            const double half_width =
                0.45 * column_width * group.density_y[point] / maximum_density;
            polygon += format_number(center - half_width) + "," +
                       format_number(map_y(group.density_x[point], minimum,
                                           maximum)) +
                       " ";
        }
        polygon += "\"/>\n";
        frame.body += polygon;
    }
    return frame.render();
}

// --- GridFigure ----------------------------------------------------------

grid_figure::grid_figure(const std::size_t rows, const std::size_t cols)
    : rows_(rows), cols_(cols) {
    if (rows == 0U) {
        throw factor_data_error{"rows must be a positive integer"};
    }
    if (cols == 0U) {
        throw factor_data_error{"cols must be a positive integer"};
    }
}

void grid_figure::require_open() const {
    if (closed_) {
        throw std::runtime_error{"GridFigure is closed"};
    }
}

std::uint64_t grid_figure::create_new_figure() const {
    require_open();
    return 1U;
}

grid_cell grid_figure::next_row() {
    require_open();
    if (current_col_ != 0U) {
        ++current_row_;
        current_col_ = 0U;
    }
    if (current_row_ >= rows_) {
        throw std::out_of_range{"GridFigure row exceeds the grid"};
    }
    grid_cell cell;
    cell.row = current_row_;
    cell.col = 0U;
    cell.full_row = true;
    cell.index = cells_.size();
    cells_.push_back(cell);
    ++current_row_;
    return cell;
}

grid_cell grid_figure::next_cell() {
    require_open();
    if (current_col_ >= cols_) {
        ++current_row_;
        current_col_ = 0U;
    }
    if (current_row_ >= rows_) {
        throw std::out_of_range{"GridFigure cell exceeds the grid"};
    }
    grid_cell cell;
    cell.row = current_row_;
    cell.col = current_col_;
    cell.full_row = false;
    cell.index = cells_.size();
    cells_.push_back(cell);
    ++current_col_;
    return cell;
}

void grid_figure::close() noexcept {
    closed_ = true;
    cells_.clear();
}

void close_owned_figures(owned_figures& figures) {
    figures.closed.assign(figures.figures.size(), true);
}

std::vector<std::uint64_t> show_owned_figures(const owned_figures& figures) {
    return figures.figures;
}

}  // namespace fincore
