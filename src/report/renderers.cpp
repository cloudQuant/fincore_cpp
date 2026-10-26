#include <fincore/report/renderers.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>
#include <fincore/viz/bokeh_document.hpp>
#include <fincore/viz/plotly_document.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

std::string legend_name(const report_section& section,
                        const std::string& series_key) {
    const auto found = section.legends.find(series_key);
    if (found != section.legends.end()) {
        return found->second;
    }
    return section.title + ": " + series_key;
}

// A non-finite coordinate is written as null in place rather than dropped, so the
// two arrays stay index-aligned and a renderer can tell a gap apart from a shifted
// series; the chart builders, which flatten to a single line, remove such points
// instead.
std::string static_axis_content(const report_static_axes& axis) {
    std::string output;
    detail::json_writer writer(output);
    writer.begin_object();
    writer.key("name");
    writer.value_string(axis.name);
    writer.key("x");
    writer.begin_array();
    for (const double value : axis.x) {
        if (std::isfinite(value)) {
            writer.value_number(value, "static axis x");
        } else {
            writer.value_null();
        }
    }
    writer.end_array();
    writer.key("y");
    writer.begin_array();
    for (const double value : axis.y) {
        if (std::isfinite(value)) {
            writer.value_number(value, "static axis y");
        } else {
            writer.value_null();
        }
    }
    writer.end_array();
    writer.end_object();
    output.push_back('\n');
    return output;
}

}  // namespace

const report_artifact* report_artifact_bundle::find(
    const std::string_view name) const {
    for (const auto& artifact : artifacts) {
        if (artifact.name == name) {
            return &artifact;
        }
    }
    return nullptr;
}

const std::string& report_artifact_bundle::report_digest() const {
    const auto found = metadata.find("report_digest");
    if (found == metadata.end()) {
        throw input_error{"report artifact bundle has no report_digest"};
    }
    return found->second;
}

report_artifact_bundle render_plotly(const report_document& document) {
    plotly_backend backend;
    plotly_document figure = backend.create_document(document.title);
    for (const auto& section : document.sections) {
        for (const auto& series : section.series) {
            plotly_trace trace;
            trace.type = plotly_trace::kind::scatter;
            trace.mode = "lines";
            trace.name = legend_name(section, series.key);
            trace.x_values = series.x;
            trace.y_values = series.y;
            if (trace.x_values.size() != trace.y_values.size()) {
                throw input_error{
                    "report series x and y must have equal lengths"};
            }
            figure.traces.push_back(std::move(trace));
        }
    }
    report_artifact_bundle bundle;
    bundle.metadata = {{"backend", "plotly"},
                       {"report_digest", document.semantic_digest()}};
    bundle.plotly = figure;
    report_artifact artifact;
    artifact.name = "figure";
    artifact.owned = true;
    artifact.content = figure.to_json();
    bundle.artifacts.push_back(std::move(artifact));
    return bundle;
}

report_artifact_bundle render_bokeh(const report_document& document) {
    bokeh_backend backend;
    bokeh_document figure = backend.create_document(document.title);
    for (const auto& section : document.sections) {
        for (const auto& series : section.series) {
            bokeh_glyph glyph;
            glyph.type = bokeh_glyph::kind::line;
            glyph.x_field = "index";
            glyph.y_field = series.key;
            glyph.x_values = series.x;
            glyph.y_values = series.y;
            glyph.legend_label = legend_name(section, series.key);
            if (glyph.x_values.size() != glyph.y_values.size()) {
                throw input_error{
                    "report series x and y must have equal lengths"};
            }
            figure.glyphs.push_back(std::move(glyph));
        }
    }
    report_artifact_bundle bundle;
    bundle.metadata = {{"backend", "bokeh"},
                       {"report_digest", document.semantic_digest()}};
    bundle.bokeh = figure;
    report_artifact artifact;
    artifact.name = "figure";
    artifact.owned = true;
    artifact.content = figure.to_json();
    bundle.artifacts.push_back(std::move(artifact));
    return bundle;
}

report_artifact_bundle render_matplotlib(
    const report_document& document,
    const std::vector<std::string>& supplied_axes) {
    const std::set<std::string> supplied(supplied_axes.begin(),
                                         supplied_axes.end());
    report_artifact_bundle bundle;
    bundle.metadata = {{"backend", "matplotlib"},
                       {"report_digest", document.semantic_digest()}};
    for (const auto& section : document.sections) {
        for (const auto& series : section.series) {
            const std::string key = section.key + "." + series.key;
            const auto legend = section.legends.find(series.key);
            const std::string label =
                legend == section.legends.end() ? series.key : legend->second;
            const auto unit = section.units.find(series.key);
            report_static_axes axis;
            axis.name = "axis:" + key;
            axis.section_key = section.key;
            axis.series_key = series.key;
            axis.title = section.title + ": " + label;
            axis.label = label;
            axis.x_label = series.x_label;
            axis.y_label = unit == section.units.end() ? std::string{}
                                                       : unit->second;
            axis.unit = axis.y_label;
            axis.x = series.x;
            axis.y = series.y;
            axis.owned = supplied.count(key) == 0U;
            report_artifact artifact;
            artifact.name = axis.name;
            artifact.owned = axis.owned;
            artifact.content = static_axis_content(axis);
            bundle.artifacts.push_back(std::move(artifact));
            bundle.axes.push_back(std::move(axis));
        }
    }
    return bundle;
}

report_artifact_bundle write_pdf(const report_document& document,
                                 const std::string& target) {
    if (target.empty()) {
        throw input_error{"pdf target path must not be empty"};
    }
    const std::filesystem::path output(target);
    if (output.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(output.parent_path(), error);
        if (error) {
            throw input_error{"pdf target directory could not be created: " +
                              output.parent_path().string()};
        }
    }
    const std::string bytes = render_pdf_bytes(document);
    std::ofstream stream(target, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw input_error{"pdf target could not be opened: " + target};
    }
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw input_error{"pdf target could not be written: " + target};
    }
    report_artifact_bundle bundle;
    bundle.metadata = {{"backend", "pdf"},
                       {"report_digest", document.semantic_digest()}};
    report_artifact artifact;
    artifact.name = "file";
    artifact.path = target;
    artifact.owned = false;
    artifact.content = bytes;
    bundle.artifacts.push_back(std::move(artifact));
    return bundle;
}

}  // namespace fincore
