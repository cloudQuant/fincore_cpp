#pragma once

#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

/// Chart renderer used by render_html.
enum class report_chart_backend {
  inline_svg, ///< Draw each series as an inline SVG polyline.
  echarts,    ///< Embed the pinned offline ECharts 5.5.0 script and chart
              ///< payloads.
};

/// HTML rendering options.
struct report_html_options {
  std::string language = "en"; ///< UI language: "en" or "zh".
  /// Chart renderer for report series.
  report_chart_backend chart_backend = report_chart_backend::inline_svg;
};

/// Renders a report document as one self-contained HTML page.
///
/// Text is HTML-escaped, styles are embedded, and no external asset URL is
/// referenced; the ECharts backend embeds the pinned chart script. Output is
/// deterministic for the same document and options.
///
/// @param document Prepared report document.
/// @param options Language and chart backend selection.
/// @return Complete HTML text starting with "<!doctype html>".
/// @throws input_error If the language or chart backend is unknown, a numeric
///     or series value is non-finite, a table row does not match its column
///     count, or an ECharts series has mismatched x and y lengths.
std::string render_html(const report_document &document,
                        const report_html_options &options = {});

} // namespace fincore
