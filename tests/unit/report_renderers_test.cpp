#include <fincore/error.hpp>
#include <fincore/report/models.hpp>
#include <fincore/report/renderers.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

fincore::report_document sample_document() {
    fincore::report_metric alpha;
    alpha.key = "alpha";
    alpha.label = "Alpha";
    alpha.unit = "ratio";
    alpha.number = 1.5;

    fincore::report_table table;
    table.key = "summary";
    table.title = "Summary";
    table.columns = {"name", "value", "active"};
    table.rows = {
        {fincore::report_cell::text_cell("Revenue"),
         fincore::report_cell::number_cell(0.25),
         fincore::report_cell::boolean_cell(true)},
        {fincore::report_cell::text_cell("Cost"),
         fincore::report_cell::missing(),
         fincore::report_cell::boolean_cell(false)}};

    fincore::report_series strategy;
    strategy.key = "strategy";
    strategy.title = "Strategy";
    strategy.x_label = "period";
    strategy.y_label = "return";
    strategy.unit = "ratio";
    strategy.x = {0.0, 1.0, 2.0};
    strategy.y = {0.1, 0.2, 0.3};

    fincore::report_series benchmark;
    benchmark.key = "benchmark";
    benchmark.title = "Benchmark";
    benchmark.x_label = "period";
    benchmark.y_label = "return";
    benchmark.unit = "ratio";
    benchmark.x = {0.0, 1.0, 2.0};
    benchmark.y = {0.05, 0.1, 0.15};

    fincore::report_section section;
    section.key = "performance";
    section.title = "Performance";
    section.metrics = {alpha};
    section.tables = {table};
    section.series = {strategy, benchmark};
    section.notes = {"Prepared model values only."};
    section.units = {{"alpha", "ratio"},
                     {"strategy", "ratio"},
                     {"benchmark", "ratio"}};
    section.legends = {{"strategy", "Strategy"}};

    fincore::report_document document;
    document.domain = "portfolio";
    document.title = "Sample Report";
    document.metadata = {{"source", "fixture"}};
    document.sections = {section};
    return document;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

}  // namespace

TEST(ReportRenderers, PlotlyConsumesPreparedSeries) {
    const auto document = sample_document();
    const auto bundle = fincore::render_plotly(document);
    ASSERT_TRUE(bundle.plotly.has_value());
    EXPECT_EQ(bundle.metadata.at("backend"), "plotly");
    EXPECT_EQ(bundle.report_digest(), document.semantic_digest());

    const auto& figure = *bundle.plotly;
    EXPECT_EQ(figure.title, "Sample Report");
    ASSERT_EQ(figure.traces.size(), 2U);
    EXPECT_EQ(figure.traces[0].name, "Strategy");
    EXPECT_EQ(figure.traces[1].name, "Performance: benchmark");
    EXPECT_EQ(figure.traces[0].y_values,
              std::vector<double>({0.1, 0.2, 0.3}));
    EXPECT_EQ(figure.traces[1].x_values,
              std::vector<double>({0.0, 1.0, 2.0}));
    EXPECT_EQ(figure.traces[0].mode, "lines");

    const auto* artifact = bundle.find("figure");
    ASSERT_NE(artifact, nullptr);
    EXPECT_EQ(artifact->content, figure.to_json());
    EXPECT_NE(artifact->content.find("\"title\": \"Sample Report\""),
              std::string::npos);
    EXPECT_EQ(bundle.find("missing"), nullptr);
}

TEST(ReportRenderers, BokehConsumesPreparedSeries) {
    const auto document = sample_document();
    const auto bundle = fincore::render_bokeh(document);
    ASSERT_TRUE(bundle.bokeh.has_value());
    EXPECT_EQ(bundle.metadata.at("backend"), "bokeh");
    EXPECT_EQ(bundle.report_digest(), document.semantic_digest());

    const auto& figure = *bundle.bokeh;
    ASSERT_EQ(figure.glyphs.size(), 2U);
    EXPECT_EQ(figure.title, "Sample Report");
    EXPECT_EQ(figure.glyphs[0].legend_label, "Strategy");
    EXPECT_EQ(figure.glyphs[1].legend_label, "Performance: benchmark");
    EXPECT_EQ(figure.glyphs[0].y_values,
              std::vector<double>({0.1, 0.2, 0.3}));
    EXPECT_EQ(figure.glyphs[1].x_values,
              std::vector<double>({0.0, 1.0, 2.0}));

    const auto* artifact = bundle.find("figure");
    ASSERT_NE(artifact, nullptr);
    EXPECT_EQ(artifact->content, figure.to_json());
}

TEST(ReportRenderers, MatplotlibAxesKeepOwnershipAndValues) {
    const auto document = sample_document();
    // Only the key the caller supplied loses ownership; the other axis is built
    // by the renderer and stays owned, which is the whole difference here.
    const auto bundle = fincore::render_matplotlib(
        document, {"performance.benchmark"});
    EXPECT_EQ(bundle.metadata.at("backend"), "matplotlib");
    EXPECT_EQ(bundle.report_digest(), document.semantic_digest());
    ASSERT_EQ(bundle.axes.size(), 2U);
    EXPECT_EQ(bundle.axes[0].name, "axis:performance.strategy");
    EXPECT_EQ(bundle.axes[0].title, "Performance: Strategy");
    EXPECT_EQ(bundle.axes[0].label, "Strategy");
    EXPECT_EQ(bundle.axes[0].y_label, "ratio");
    EXPECT_EQ(bundle.axes[0].y, std::vector<double>({0.1, 0.2, 0.3}));
    EXPECT_TRUE(bundle.axes[0].owned);

    EXPECT_EQ(bundle.axes[1].name, "axis:performance.benchmark");
    EXPECT_EQ(bundle.axes[1].label, "benchmark");
    EXPECT_EQ(bundle.axes[1].y, std::vector<double>({0.05, 0.1, 0.15}));
    EXPECT_FALSE(bundle.axes[1].owned);
    ASSERT_EQ(bundle.artifacts.size(), 2U);
    const auto* benchmark_artifact =
        bundle.find("axis:performance.benchmark");
    ASSERT_NE(benchmark_artifact, nullptr);
    EXPECT_FALSE(benchmark_artifact->owned);
}

TEST(ReportRenderers, DefaultHtmlCssIsStableNativeText) {
    // Bare CSS rather than a style element: no <style> wrapper, no blank lines,
    // and both ends trimmed (first byte 'b', last '}'), which is the shape the
    // byte-compared document depends on.
    const std::string_view css = fincore::DEFAULT_HTML_CSS;
    ASSERT_FALSE(css.empty());
    EXPECT_EQ(css.front(), 'b');
    EXPECT_EQ(css.back(), '}');
    EXPECT_NE(css.find("th, td { border: 1px solid #d1d5db;"),
              std::string_view::npos);
    EXPECT_NE(css.find(".report-note"), std::string_view::npos);
    EXPECT_NE(css.find(".report-series"), std::string_view::npos);
    EXPECT_EQ(css.find("<style>"), std::string_view::npos);
    EXPECT_EQ(css.find("\n\n"), std::string_view::npos);
}

TEST(ReportRenderers, PdfBytesAreDeterministicAndWellFormed) {
    const auto document = sample_document();
    const std::string bytes = fincore::render_pdf_bytes(document);
    EXPECT_EQ(bytes.rfind("%PDF-1.4\n", 0), 0U);
    EXPECT_EQ(bytes.substr(bytes.size() - 6U), "%%EOF\n");
    EXPECT_NE(bytes.find("<< /Type /Catalog /Pages 2 0 R >>"),
              std::string::npos);
    EXPECT_NE(bytes.find("/Type /Pages"), std::string::npos);
    EXPECT_NE(bytes.find("(Sample Report)"), std::string::npos);
    EXPECT_NE(bytes.find("(Alpha: 1.5 ratio)"), std::string::npos);
    EXPECT_NE(bytes.find("(Revenue)"), std::string::npos);
    EXPECT_NE(bytes.find("(Generated by fincore)"), std::string::npos);
    EXPECT_NE(bytes.find("endstream"), std::string::npos);
    EXPECT_EQ(fincore::render_pdf_bytes(document), bytes);

    const auto marker = bytes.rfind("startxref\n");
    ASSERT_NE(marker, std::string::npos);
    // Reading the recorded offset back and finding "xref" there is what makes the
    // file structurally valid rather than merely well-shaped at both ends.
    const auto offset_start = marker + std::string("startxref\n").size();
    const auto offset_end = bytes.find('\n', offset_start);
    ASSERT_NE(offset_end, std::string::npos);
    const auto offset =
        static_cast<std::size_t>(std::stoul(bytes.substr(
            offset_start, offset_end - offset_start)));
    ASSERT_LT(offset, bytes.size());
    EXPECT_EQ(bytes.compare(offset, 4U, "xref"), 0);

    fincore::report_document empty;
    const std::string empty_bytes = fincore::render_pdf_bytes(empty);
    EXPECT_EQ(empty_bytes.rfind("%PDF-1.4\n", 0), 0U);
    EXPECT_EQ(empty_bytes.substr(empty_bytes.size() - 6U), "%%EOF\n");
}

TEST(ReportRenderers, WritePdfCreatesParentsAndOwningBundle) {
    const auto document = sample_document();
    const auto directory = std::filesystem::temp_directory_path() /
                           "fincore_i8_pdf_test";
    std::filesystem::remove_all(directory);
    const auto target = directory / "nested" / "report.pdf";
    const auto bundle = fincore::write_pdf(document, target.string());
    EXPECT_EQ(bundle.metadata.at("backend"), "pdf");
    EXPECT_EQ(bundle.report_digest(), document.semantic_digest());
    ASSERT_TRUE(std::filesystem::exists(target));
    const std::string written = read_file(target);
    EXPECT_EQ(written, fincore::render_pdf_bytes(document));
    const auto* artifact = bundle.find("file");
    ASSERT_NE(artifact, nullptr);
    EXPECT_EQ(artifact->path, target.string());
    EXPECT_FALSE(artifact->owned);
    EXPECT_EQ(artifact->content, written);
    EXPECT_THROW(fincore::write_pdf(document, ""), fincore::input_error);
    std::filesystem::remove_all(directory);
}
