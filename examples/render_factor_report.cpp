#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/report.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/xlsx.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool write_file(const std::filesystem::path &path, const std::string &data) {
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
  std::printf("wrote %s (%zu bytes)\n", path.string().c_str(), data.size());
  return true;
}

} // namespace

int main(int argc, char **argv) {
  bool use_echarts = false;
  const char *output_argument = nullptr;
  if (argc == 2) {
    output_argument = argv[1];
  } else if (argc == 3 && std::string{argv[1]} == "--echarts") {
    use_echarts = true;
    output_argument = argv[2];
  } else {
    std::fprintf(stderr,
                 "usage: %s [--echarts] OUTPUT.html\n"
                 "  --echarts embeds ECharts 5.5.0 for offline charts; "
                 "default is inline SVG.\n"
                 "  Both HTML files include an offline English/Chinese "
                 "language switch.\n",
                 argv[0]);
    return 2;
  }
  fincore::prepared_factor_data data;
  data.forward_labels = {"1D", "5D"};
  const auto start = fincore::utc_timestamp(2022, 1, 3);
  const auto day = 86'400'000'000'000LL;
  const char *assets[] = {"ALPHA", "BETA", "GAMMA", "DELTA", "EPSILON"};
  for (int date = 0; date < 40; ++date) {
    for (int asset = 0; asset < 5; ++asset) {
      fincore::factor_row row;
      row.date = start + date * day;
      row.asset = assets[asset];
      row.factor = 0.4 * std::sin((date + 1) * 0.21 + asset * 0.8) +
                   0.15 * std::cos((date + 1) * 0.07 + asset) + 0.02 * asset;
      row.quantile = asset + 1;
      row.forward_returns = {
          0.002 * std::sin((date + 1) * 0.13 + asset * 0.7) + 0.0008 * asset,
          0.01 * std::cos((date + 1) * 0.05 + asset * 0.4) - 0.001 * asset};
      data.rows.push_back(row);
    }
  }
  fincore::factor_analysis_config config;
  config.periods = {"1D", "5D"};
  config.turnover_periods = {1, 5};
  config.include_portfolio_inputs = false;
  const auto model = fincore::analyze_factor(data, config);
  const auto document = fincore::build_factor_report(model, "Factor Report");
  fincore::report_html_options english_options;
  english_options.language = "en";
  if (use_echarts) {
    english_options.chart_backend = fincore::report_chart_backend::echarts;
  }
  fincore::report_html_options chinese_options;
  chinese_options.language = "zh";
  if (use_echarts) {
    chinese_options.chart_backend = fincore::report_chart_backend::echarts;
  }

  const std::filesystem::path english_path{output_argument};
  auto chinese_path = english_path;
  chinese_path.replace_filename(english_path.stem().string() + ".zh" +
                                english_path.extension().string());
  auto xlsx_path = english_path;
  xlsx_path.replace_extension(".xlsx");

  const std::string english_html =
      fincore::render_html(document, english_options);
  const std::string chinese_html =
      fincore::render_html(document, chinese_options);
  const std::string workbook = fincore::render_xlsx(document);
  const bool english_written = write_file(english_path, english_html);
  const bool chinese_written = write_file(chinese_path, chinese_html);
  const bool workbook_written = write_file(xlsx_path, workbook);
  return english_written && chinese_written && workbook_written ? 0 : 1;
}
