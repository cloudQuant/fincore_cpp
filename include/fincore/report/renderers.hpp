#pragma once

#include <fincore/report/models.hpp>
#include <fincore/viz/bokeh_document.hpp>
#include <fincore/viz/plotly_document.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/**
 * Stable native report CSS.
 *
 * This is the same presentation constant as the pinned
 * `fincore.report.styles.DEFAULT_HTML_CSS` (already `strip()`ed there): no
 * leading or trailing whitespace, no external font/asset reference.
 */
inline constexpr std::string_view DEFAULT_HTML_CSS =
    "body { font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", "
    "sans-serif; color: #1f2937; margin: 2rem auto; max-width: 1100px; }\n"
    "h1 { border-bottom: 2px solid #1d4ed8; padding-bottom: .4rem; }\n"
    "h2 { margin-top: 2rem; color: #1e3a8a; }\n"
    "table { border-collapse: collapse; width: 100%; margin: .8rem 0; }\n"
    "th, td { border: 1px solid #d1d5db; padding: .45rem .65rem; "
    "text-align: right; }\n"
    "th { background: #eff6ff; text-align: left; }\n"
    ".report-note { color: #4b5563; }\n"
    ".report-series { margin: 1rem 0; }";

/** One emitted artifact (in-memory content and/or a written path). */
struct report_artifact {
    std::string name;
    std::string path;
    bool owned = true;
    std::string content;
};

/**
 * Static native replacement for a Matplotlib axis.
 *
 * The Python renderer returns one axis per section series, keyed
 * `axis:<section>.<series>`. The native artifact carries the same prepared
 * values; there is no Matplotlib object and no kernel recomputation.
 */
struct report_static_axes {
    std::string name;
    std::string section_key;
    std::string series_key;
    std::string title;
    std::string label;
    std::string x_label;
    std::string y_label;
    std::string unit;
    std::vector<double> x;
    std::vector<double> y;
    bool owned = true;
};

/**
 * Owning native artifact bundle returned by report renderers.
 *
 * Mirrors the pinned `ArtifactBundle` shape: `metadata` always carries
 * `backend` and `report_digest`; `artifacts` carries named outputs.
 */
struct report_artifact_bundle {
    std::map<std::string, std::string> metadata;
    std::vector<report_artifact> artifacts;
    std::optional<plotly_document> plotly;
    std::optional<bokeh_document> bokeh;
    std::vector<report_static_axes> axes;

    const report_artifact* find(std::string_view name) const;
    const std::string& report_digest() const;
};

/**
 * Project prepared report series into one Plotly-compatible document.
 *
 * One scatter trace per `report_section::series` entry; the trace name is the
 * section legend or `"<section title>: <series key>"`. Values are copied, not
 * recomputed. `report_series::x` is numeric in the native model, so the
 * Python `DatetimeIndex` x-axis is a declared date-axis format variance.
 */
report_artifact_bundle render_plotly(const report_document& document);

/** Project prepared report series into one Bokeh-compatible document. */
report_artifact_bundle render_bokeh(const report_document& document);

/**
 * Native static projection of prepared report series.
 *
 * `supplied_axes` lists `"<section>.<series>"` keys the caller already owns;
 * those artifacts are marked `owned = false`, matching the Python `axes`
 * argument ownership rule.
 */
report_artifact_bundle render_matplotlib(
    const report_document& document,
    const std::vector<std::string>& supplied_axes = {});

/**
 * Deterministic native PDF 1.4 bytes for a report document.
 *
 * Includes text, headings, tables and simple line-series vector paths. Uses
 * base-14 Helvetica fonts; characters outside WinAnsi are replaced with `?`
 * (declared format variance from the Chromium-backed Python writer).
 */
std::string render_pdf_bytes(const report_document& document);

/**
 * Write the native PDF to `target` and return the artifact bundle.
 *
 * Parent directories are created like the Python writer. Byte identity with
 * the Python PDF backend is not claimed.
 */
report_artifact_bundle write_pdf(const report_document& document,
                                 const std::string& target);

}  // namespace fincore
