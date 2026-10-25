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
    /// Logical name, such as "figure", "file" or
    /// `"axis:<section>.<series>"`.
    std::string name;
    /// Written file path; empty for artifacts that stay in memory.
    std::string path;
    /// True for self-owned in-memory artifacts; false for caller-supplied
    /// axes and for the file artifact returned by write_pdf.
    bool owned = true;
    /// Serialized figure or axis JSON, or written bytes for a file artifact.
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
    std::string name;         ///< Name in the form `"axis:<section>.<series>"`.
    std::string section_key;  ///< Source section key.
    std::string series_key;   ///< Source series key.
    /// `"<section title>: <legend or series key>"`.
    std::string title;
    /// Section legend for the series, or the series key when none is set.
    std::string label;
    std::string x_label;  ///< X-axis label copied from the series.
    /// Section unit for the series, or empty when none is defined.
    std::string y_label;
    std::string unit;      ///< Same value as `y_label`.
    std::vector<double> x;  ///< X values copied from the series.
    std::vector<double> y;  ///< Y values copied from the series.
    /// False when the caller supplied the matching axis key.
    bool owned = true;
};

/**
 * Owning native artifact bundle returned by report renderers.
 *
 * Mirrors the pinned `ArtifactBundle` shape: `metadata` always carries
 * `backend` and `report_digest`; `artifacts` carries named outputs.
 */
struct report_artifact_bundle {
    /// Bundle metadata; renderers set "backend" and "report_digest".
    std::map<std::string, std::string> metadata;
    std::vector<report_artifact> artifacts;  ///< Named outputs.
    /// Plotly figure when produced by render_plotly.
    std::optional<plotly_document> plotly;
    /// Bokeh document when produced by render_bokeh.
    std::optional<bokeh_document> bokeh;
    /// Static axes when produced by render_matplotlib.
    std::vector<report_static_axes> axes;

    /// Finds an artifact by logical name.
    /// @param name Artifact name to search for.
    /// @return Pointer to the first matching artifact, or nullptr when no
    ///     artifact has `name`.
    const report_artifact* find(std::string_view name) const;
    /// Returns the bundle's report digest.
    /// @return Value of the "report_digest" metadata entry.
    /// @throws input_error If `metadata` has no "report_digest" entry.
    const std::string& report_digest() const;
};

/**
 * Projects prepared report series into one Plotly-compatible document.
 *
 * One scatter trace per `report_section::series` entry; the trace name is the
 * section legend or `"<section title>: <series key>"`. Values are copied, not
 * recomputed. `report_series::x` is numeric in the native model, so the
 * Python `DatetimeIndex` x-axis is a declared date-axis format variance.
 *
 * @param document Prepared report document.
 * @return Bundle whose metadata identifies the plotly backend and whose
 *     "figure" artifact contains the figure JSON.
 * @throws input_error If a series has mismatched x and y lengths or the
 *     document fails semantic digest validation.
 */
report_artifact_bundle render_plotly(const report_document& document);

/**
 * Projects prepared report series into one Bokeh-compatible document.
 *
 * One line glyph per series, with the series key as the y field and the
 * section legend or `"<section title>: <series key>"` as the legend label.
 * Values are copied, not recomputed.
 *
 * @param document Prepared report document.
 * @return Bundle whose metadata identifies the bokeh backend and whose
 *     "figure" artifact contains the document JSON.
 * @throws input_error If a series has mismatched x and y lengths or the
 *     document fails semantic digest validation.
 */
report_artifact_bundle render_bokeh(const report_document& document);

/**
 * Projects prepared report series into static Matplotlib-compatible axes.
 *
 * `supplied_axes` lists `"<section>.<series>"` keys the caller already owns;
 * those artifacts are marked `owned = false`, matching the Python `axes`
 * argument ownership rule.
 *
 * @param document Prepared report document.
 * @param supplied_axes Axis keys in the form `"<section>.<series>"` already
 *     owned by the caller.
 * @return Bundle whose metadata identifies the matplotlib backend, with one
 *     `"axis:<section>.<series>"` artifact per series and matching entries in
 *     `axes`.
 * @throws input_error If the document fails semantic digest validation.
 */
report_artifact_bundle render_matplotlib(
    const report_document& document,
    const std::vector<std::string>& supplied_axes = {});

/**
 * Renders deterministic native PDF 1.4 bytes for a report document.
 *
 * Includes text, headings, tables and simple line-series vector paths. Uses
 * base-14 Helvetica fonts; characters outside WinAnsi are replaced with `?`
 * (declared format variance from the Chromium-backed Python writer).
 *
 * @param document Report document to render.
 * @return PDF 1.4 bytes ending with "%%EOF\n".
 */
std::string render_pdf_bytes(const report_document& document);

/**
 * Writes the native PDF to `target` and returns the artifact bundle.
 *
 * Parent directories are created like the Python writer, and an existing file
 * at `target` is truncated. Byte identity with the Python PDF backend is not
 * claimed.
 *
 * @param document Report document to render.
 * @param target Output path; must not be empty.
 * @return Bundle whose metadata identifies the pdf backend and whose "file"
 *     artifact records `target`, `owned = false` and the written bytes.
 * @throws input_error If `target` is empty, its parent directory cannot be
 *     created, the file cannot be opened or written, or the document fails
 *     semantic digest validation.
 */
report_artifact_bundle write_pdf(const report_document& document,
                                 const std::string& target);

}  // namespace fincore
