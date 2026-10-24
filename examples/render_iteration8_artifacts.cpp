#include <fincore/report/html.hpp>
#include <fincore/report/models.hpp>
#include <fincore/report/renderers.hpp>
#include <fincore/report/xlsx.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool write_file(const std::filesystem::path& path, const std::string& data) {
  std::ofstream stream(path, std::ios::binary);
  if (!stream) {
    std::fprintf(stderr, "cannot open %s\n", path.string().c_str());
    return false;
  }
  stream.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!stream) {
    std::fprintf(stderr, "cannot write %s\n", path.string().c_str());
    return false;
  }
  return true;
}

fincore::report_document make_document() {
  fincore::report_document document;
  document.domain = "factor";
  document.title = "Iteration 8 Native Artifact Check";
  document.metadata = {{"source", "iteration8"},
                       {"language", "en"},
                       {"schema", "2"}};

  fincore::report_section section;
  section.key = "factor_summary";
  section.title = "Factor Summary";
  section.metrics.push_back(
      {"mean_ic", "Mean IC", "correlation", 0.041, std::nullopt, std::nullopt});
  section.metrics.push_back(
      {"hit_rate", "Hit Rate", "percent", 0.53, std::nullopt, std::nullopt});

  fincore::report_table table;
  table.key = "quantile_returns";
  table.title = "Mean Return by Quantile";
  table.columns = {"quantile", "mean_return_bps"};
  for (int quantile = 1; quantile <= 3; ++quantile) {
    table.rows.push_back(
        {fincore::report_cell::number_cell(static_cast<double>(quantile)),
         fincore::report_cell::number_cell(1.25 * quantile)});
  }
  section.tables.push_back(std::move(table));

  fincore::report_series series;
  series.key = "cumulative";
  series.title = "Cumulative Return";
  series.x_label = "Day";
  series.y_label = "Growth";
  series.unit = "index";
  series.x = {0.0, 1.0, 2.0, 3.0, 4.0};
  series.y = {1.0, 1.01, 1.03, 1.02, 1.05};
  section.series.push_back(std::move(series));
  section.units = {{"mean_ic", "correlation"}, {"cumulative", "index"}};
  section.legends = {{"cumulative", "Cumulative Return"}};
  section.notes.push_back("Native artifact value check.");

  document.sections.push_back(std::move(section));
  return document;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s OUTPUT_DIR\n", argv[0]);
    return 2;
  }
  const std::filesystem::path output{argv[1]};
  std::error_code error;
  std::filesystem::create_directories(output, error);
  if (error) {
    std::fprintf(stderr, "cannot create %s\n", output.string().c_str());
    return 2;
  }

  const auto document = make_document();
  const std::string html =
      fincore::render_html(document, {.language = "en",
                                      .chart_backend =
                                          fincore::report_chart_backend::inline_svg});
  const std::string echarts =
      fincore::render_html(document, {.language = "en",
                                      .chart_backend =
                                          fincore::report_chart_backend::echarts});
  const std::string xlsx = fincore::render_xlsx(document);
  const auto plotly = fincore::render_plotly(document);
  const auto bokeh = fincore::render_bokeh(document);
  const auto pdf = fincore::write_pdf(document, (output / "report.pdf").string());

  bool ok = true;
  ok &= write_file(output / "report.html", html);
  ok &= write_file(output / "report-echarts.html", echarts);
  ok &= write_file(output / "report.xlsx", xlsx);
  ok &= write_file(output / "report-plotly.json",
                   plotly.find("figure")->content);
  ok &= write_file(output / "report-bokeh.json",
                   bokeh.find("figure")->content);
  if (!ok || pdf.find("file") == nullptr) {
    return 1;
  }
  std::printf("report_digest=%s\n", document.semantic_digest().c_str());
  std::printf("wrote html=%zu echarts=%zu xlsx=%zu plotly=%zu bokeh=%zu pdf=%s\n",
              html.size(), echarts.size(), xlsx.size(),
              plotly.find("figure")->content.size(),
              bokeh.find("figure")->content.size(),
              (output / "report.pdf").string().c_str());
  return 0;
}
