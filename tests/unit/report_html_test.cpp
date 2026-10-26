#include <fincore/error.hpp>
#include <fincore/report/html.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

fincore::report_document demo_document() {
  fincore::report_metric metric;
  metric.key = "state";
  metric.label = "State";
  metric.text = "<script>alert(1)</script>";
  fincore::report_table table;
  table.key = "t";
  table.title = "Table";
  table.columns = {"name", "value"};
  table.rows = {
      {fincore::report_cell::text_cell("<b>"),
       fincore::report_cell::number_cell(1.5)},
      {fincore::report_cell::text_cell("b"), fincore::report_cell::missing()}};
  fincore::report_series series;
  series.key = "s";
  series.title = "Series";
  series.x_label = "t";
  series.y_label = "r";
  series.unit = "return";
  series.x = {0.0, 1.0, 2.0};
  series.y = {1.0, 1.5, 1.0};
  fincore::report_section section;
  section.key = "s1";
  section.title = "Section <1>";
  section.metrics = {metric};
  section.tables = {table};
  section.series = {series};
  section.notes = {"note & detail"};
  fincore::report_document document;
  document.domain = "demo";
  document.title = "Title <unsafe>";
  document.metadata = {{"b", "2"}, {"a", "1"}};
  document.sections = {section};
  return document;
}

fincore::report_document factor_document() {
  fincore::report_document document;
  document.domain = "factor";
  document.title = "Factor Report";
  document.metadata = {{"asset_count", "5"},
                       {"date_count", "40"},
                       {"factor_result_fingerprint", "abc123"},
                       {"forward_periods", "1D,5D"},
                       {"start", "2022-01-03"},
                       {"end", "2022-02-25"}};

  fincore::report_section summary;
  summary.key = "factor_summary";
  summary.title = "Factor summary";
  fincore::report_metric mean_ic;
  mean_ic.key = "mean_information_coefficient.1D";
  mean_ic.label = "Mean IC 1D";
  mean_ic.unit = "correlation";
  mean_ic.number = 0.123456;
  fincore::report_metric observations;
  observations.key = "observation_count";
  observations.label = "Observation count";
  observations.unit = "rows";
  observations.number = 1000.0;
  fincore::report_metric coverage;
  coverage.key = "coverage_ratio";
  coverage.label = "Coverage ratio";
  coverage.unit = "fraction";
  coverage.number = 0.875;
  summary.metrics = {mean_ic, observations, coverage};

  fincore::report_table table;
  table.key = "information_summary";
  table.title = "Information summary";
  table.columns = {"period", "IC Mean", "IC Std.", "count", "date"};
  table.rows = {{fincore::report_cell::text_cell("1D"),
                 fincore::report_cell::number_cell(0.123456789),
                 fincore::report_cell::number_cell(0.25),
                 fincore::report_cell::number_cell(1000.0),
                 fincore::report_cell::text_cell("2022-01-03")}};
  summary.tables = {table};
  fincore::report_table turnover;
  turnover.key = "turnover_summary";
  turnover.title = "Turnover summary";
  turnover.columns = {"measure", "1D"};
  turnover.rows = {
      {fincore::report_cell::text_cell("Quantile 1 Mean Turnover "),
       fincore::report_cell::number_cell(0.125)}};
  summary.tables.push_back(std::move(turnover));
  summary.notes = {"Values are computed from the prepared factor panel; the "
                   "report does not recalculate metrics.",
                   "Reference research output, not investment advice."};

  const auto add_series = [&summary](std::string key, std::string title,
                                     std::string y_label, std::string unit,
                                     double first_y, double second_y) {
    fincore::report_series series;
    series.key = std::move(key);
    series.title = std::move(title);
    series.x_label = "timestamp";
    series.y_label = std::move(y_label);
    series.unit = std::move(unit);
    series.x = {0.0, 1.0};
    series.y = {first_y, second_y};
    summary.series.push_back(std::move(series));
  };
  add_series("ic.1D", "1D IC", "IC", "correlation", 0.1, 0.2);
  add_series("ic.5D", "5D IC", "IC", "correlation", -0.1, 0.3);
  add_series("factor_cumulative_returns.1D", "Factor 1D", "growth",
             "growth_multiple", 1.0, 1.1);
  add_series("factor_cumulative_returns.5D", "Factor 5D", "growth",
             "growth_multiple", 0.4, 0.5);
  document.sections.push_back(std::move(summary));
  return document;
}

std::size_t occurrences(const std::string &text,
                        const std::string_view needle) {
  std::size_t count = 0U;
  std::size_t position = 0U;
  while ((position = text.find(needle, position)) != std::string::npos) {
    ++count;
    position += needle.size();
  }
  return count;
}

TEST(ReportHtml, RendersSafeDeterministicDocument) {
  const auto html = fincore::render_html(demo_document());
  EXPECT_EQ(html.rfind("<!doctype html>", 0), 0U);
  EXPECT_NE(html.find("<html lang=\"en\">"), std::string::npos);
  EXPECT_NE(html.find("data-en=\"Title &lt;unsafe&gt;\""), std::string::npos);
  EXPECT_NE(html.find("Section &lt;1&gt;"), std::string::npos);
  EXPECT_NE(html.find("&lt;script&gt;alert(1)&lt;/script&gt;"),
            std::string::npos);
  EXPECT_EQ(html.find("<script>alert(1)</script>"), std::string::npos);
  EXPECT_NE(html.find("&lt;b&gt;"), std::string::npos);
  EXPECT_NE(html.find("1.5"), std::string::npos);
  EXPECT_NE(html.find("&mdash;"), std::string::npos);
  EXPECT_NE(html.find("<polyline"), std::string::npos);
  EXPECT_NE(html.find("note &amp; detail"), std::string::npos);
  EXPECT_NE(html.find("<dt>a</dt><dd>1</dd>"), std::string::npos);
  EXPECT_NE(
      html.find("data-en=\"Schema\" data-zh=\"模式版本\">Schema</span> 2"),
      std::string::npos);
  EXPECT_NE(html.find("class=\"hero\""), std::string::npos);
  EXPECT_NE(
      html.find(
          "data-en=\"FINCORE REPORT\" data-zh=\"FINCORE 报告\">FINCORE REPORT"),
      std::string::npos);
  EXPECT_NE(html.find("data-en=\"Generated by fincore\" data-zh=\"由 fincore "
                      "生成\">Generated by fincore"),
            std::string::npos);
  EXPECT_EQ(html.find("FACTOR RESEARCH"), std::string::npos);
  EXPECT_NE(html.find("max-width:1280px"), std::string::npos);
  EXPECT_NE(html.find("PingFang SC"), std::string::npos);
  EXPECT_NE(html.find("grid-template-columns:repeat(2,minmax(0,1fr))"),
            std::string::npos);
  EXPECT_NE(html.find("position:sticky"), std::string::npos);
  EXPECT_NE(html.find("setFincoreLanguage"), std::string::npos);
  EXPECT_EQ(html, fincore::render_html(demo_document()));

  fincore::report_html_options chinese;
  chinese.language = "zh";
  const auto chinese_html = fincore::render_html(demo_document(), chinese);
  EXPECT_NE(chinese_html.find("<html lang=\"zh\">"), std::string::npos);
  EXPECT_NE(chinese_html.find("说明"), std::string::npos);
  EXPECT_NE(chinese_html.find("元数据"), std::string::npos);
  EXPECT_NE(chinese_html.find("由 fincore 生成"), std::string::npos);
  EXPECT_NE(
      chinese_html.find("data-en=\"Schema\" data-zh=\"模式版本\">模式版本"),
      std::string::npos);
  EXPECT_NE(chinese_html.find("data-en=\"Metadata\" data-zh=\"元数据\">元数据"),
            std::string::npos);
  EXPECT_NE(chinese_html.find("data-en=\"Notes\" data-zh=\"说明\">说明"),
            std::string::npos);
  EXPECT_NE(chinese_html.find("<footer class=\"report-footer\""),
            std::string::npos);
  const auto footer_position =
      chinese_html.find("<footer class=\"report-footer\"");
  EXPECT_LT(footer_position, chinese_html.find("</main>"));
  EXPECT_NE(
      chinese_html.find(
          "data-en=\"FINCORE REPORT\" data-zh=\"FINCORE 报告\">FINCORE 报告"),
      std::string::npos);
  EXPECT_NE(chinese_html.find("Section &lt;1&gt;"), std::string::npos);
  EXPECT_NE(chinese_html.find("Table"), std::string::npos);
}

TEST(ReportHtml, HandlesShortSeriesAndMalformedTables) {
  auto document = demo_document();
  document.sections[0].series[0].x = {0.0};
  document.sections[0].series[0].y = {1.0};
  // A one-point series is still a consistent document, so the "No data" text is
  // a rendering decision for series shorter than two points rather than a
  // validation failure; the malformed and non-finite cases below throw instead.
  EXPECT_NE(fincore::render_html(document).find("No data"), std::string::npos);
  const fincore::report_html_options chinese_no_data{
      "zh", fincore::report_chart_backend::inline_svg};
  const auto chinese_empty = fincore::render_html(document, chinese_no_data);
  EXPECT_NE(chinese_empty.find("data-en=\"No data\" data-zh=\"无数据\">无数据"),
            std::string::npos);

  document = demo_document();
  document.sections[0].series[0].y[0] =
      std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(fincore::render_html(document), fincore::input_error);

  document = demo_document();
  document.sections[0].tables[0].rows[0].pop_back();
  EXPECT_THROW(fincore::render_html(document), fincore::input_error);

  document = factor_document();
  document.sections[0].metrics[0].number =
      std::numeric_limits<double>::quiet_NaN();
  const fincore::report_html_options chinese{
      "zh", fincore::report_chart_backend::echarts};
  EXPECT_THROW(fincore::render_html(document, chinese), fincore::input_error);
}

TEST(ReportHtml, RendersPinnedOfflineEChartsWithSafeCompletePayload) {
  auto document = demo_document();
  auto &series = document.sections[0].series[0];
  series.title = "</script><img src=x onerror=alert(1)> &\u4f60\u597d";
  series.x_label = "X <axis>&";
  series.y_label = "Y \u2028 axis";

  const fincore::report_html_options options{
      "zh", fincore::report_chart_backend::echarts};
  const std::string html = fincore::render_html(document, options);

  EXPECT_NE(html.find("echarts"), std::string::npos);
  EXPECT_NE(html.find("5.5.0"), std::string::npos);
  EXPECT_EQ(html.find("<script src="), std::string::npos);
  EXPECT_NE(html.find("id=\"fincore-chart-0\""), std::string::npos);
  EXPECT_NE(html.find("data:[[0.0,1.0],[1.0,1.5],[2.0,1.0]]"),
            std::string::npos);
  // Two different escapings are pinned here: markup text goes through HTML
  // entities (&lt;), while the series payload is JSON-escaped, so <, > and &
  // become \u003c, \u003e and \u0026 and no code point above ASCII reaches the
  // script source unescaped. That is what keeps the tooltip from closing the
  // element.
  EXPECT_NE(html.find("name:\"\\u003c/script\\u003e\\u003cimg"),
            std::string::npos);
  EXPECT_NE(html.find("\\u0026\\u4f60\\u597d"), std::string::npos);
  EXPECT_NE(html.find("X \\u003caxis\\u003e\\u0026"), std::string::npos);
  EXPECT_NE(html.find("Y \\u2028 axis"), std::string::npos);
  EXPECT_NE(html.find("entry.chart.setOption(entry.options[active],true)"),
            std::string::npos);
  EXPECT_EQ(html.find("</script><img"), std::string::npos);
  EXPECT_EQ(html, fincore::render_html(document, options));

  const auto default_html = fincore::render_html(document);
  EXPECT_NE(default_html.find("<polyline"), std::string::npos);
  EXPECT_EQ(default_html.find("fincore-chart-0"), std::string::npos);
}

TEST(ReportHtml, LocalizesStandardFactorReportWithoutChangingItsData) {
  const auto document = factor_document();
  const fincore::report_html_options chinese{
      "zh", fincore::report_chart_backend::echarts};
  const fincore::report_html_options english{
      "en", fincore::report_chart_backend::echarts};
  const auto zh_html = fincore::render_html(document, chinese);
  const auto en_html = fincore::render_html(document, english);

  EXPECT_NE(zh_html.find("lang-zh"), std::string::npos);
  EXPECT_NE(zh_html.find("data-zh=\"因子分析报告\">因子分析报告"),
            std::string::npos);
  EXPECT_NE(zh_html.find("data-zh=\"因子概览\">因子概览"), std::string::npos);
  EXPECT_NE(zh_html.find("1D IC 均值"), std::string::npos);
  EXPECT_NE(zh_html.find("0.1235"), std::string::npos);
  EXPECT_NE(zh_html.find("1,000"), std::string::npos);
  EXPECT_NE(zh_html.find("87.5%"), std::string::npos);
  EXPECT_NE(zh_html.find("IC 统计"), std::string::npos);
  EXPECT_NE(zh_html.find("IC 均值"), std::string::npos);
  EXPECT_NE(zh_html.find("0.123457"), std::string::npos);
  EXPECT_NE(zh_html.find("1,000"), std::string::npos);
  EXPECT_NE(zh_html.find("2022-01-03"), std::string::npos);
  EXPECT_NE(zh_html.find("分位 1 平均换手率"), std::string::npos);
  EXPECT_NE(zh_html.find("因子结果指纹"), std::string::npos);
  EXPECT_NE(zh_html.find("abc123"), std::string::npos);
  EXPECT_NE(zh_html.find("指标来自已准备的因子面板"), std::string::npos);
  EXPECT_NE(zh_html.find("1D 信息系数"), std::string::npos);
  EXPECT_NE(zh_html.find("因子累计收益 1D"), std::string::npos);
  EXPECT_NE(zh_html.find("name:\"\\u65f6\\u95f4\\u6233\""), std::string::npos);
  EXPECT_NE(zh_html.find("name:\"\\u589e\\u957f\\u500d\\u6570\""),
            std::string::npos);
  EXPECT_NE(zh_html.find("formatUtcDate(value)"), std::string::npos);
  EXPECT_NE(zh_html.find("formatUtcDate(first.value[0])"), std::string::npos);
  EXPECT_NE(zh_html.find("isNaN(date.getTime())?String(value):"),
            std::string::npos);
  EXPECT_NE(zh_html.find("scale:true,min:\"dataMin\",max:\"dataMax\""),
            std::string::npos);
  EXPECT_NE(zh_html.find("window.echarts.format.encodeHTML(item.seriesName)"),
            std::string::npos);
  EXPECT_NE(zh_html.find("data-chart-label-en=\"1D IC\""), std::string::npos);
  EXPECT_NE(zh_html.find("data-chart-label-zh=\"1D 信息系数\""),
            std::string::npos);
  EXPECT_EQ(occurrences(zh_html, "class=\"echarts-chart\""), 4U);
  EXPECT_EQ(occurrences(en_html, "class=\"echarts-chart\""), 4U);

  const std::vector<std::string> payloads = {
      "data:[[0.0,0.1],[1.0,0.2]]", "data:[[0.0,-0.1],[1.0,0.3]]",
      "data:[[0.0,1.0],[1.0,1.1]]", "data:[[0.0,0.4],[1.0,0.5]]"};
  for (const auto &payload : payloads) {
    EXPECT_EQ(occurrences(zh_html, payload), 2U);
    EXPECT_EQ(occurrences(en_html, payload), 2U);
  }
  EXPECT_EQ(document.sections[0].tables[0].rows[0][1].number, 0.123456789);
  EXPECT_EQ(zh_html.find("<script src="), std::string::npos);
  EXPECT_EQ(zh_html, fincore::render_html(document, chinese));

  auto customized = document;
  customized.title = "Custom factor portfolio report";
  customized.sections[0].title = "Custom <section> title";
  customized.sections[0].metrics[0].label = "User-defined IC label";
  fincore::report_table custom_table;
  custom_table.key = "custom_table";
  custom_table.title = "Custom statistics";
  custom_table.columns = {"measure"};
  custom_table.rows = {{fincore::report_cell::text_cell("min")}};
  customized.sections[0].tables.push_back(custom_table);
  auto &custom_series = customized.sections[0].series[0];
  custom_series.key = "user_series";
  custom_series.title = "IC Mean";
  custom_series.x_label = "timestamp";
  custom_series.y_label = "growth";
  custom_series.unit = "correlation";
  const auto custom_html = fincore::render_html(customized, chinese);
  EXPECT_NE(custom_html.find("Custom factor portfolio report"),
            std::string::npos);
  EXPECT_NE(custom_html.find("Custom &lt;section&gt; title"),
            std::string::npos);
  EXPECT_NE(custom_html.find("User-defined IC label"), std::string::npos);
  EXPECT_NE(custom_html.find("Custom statistics"), std::string::npos);
  EXPECT_NE(custom_html.find("<td>min</td>"), std::string::npos);
  EXPECT_NE(custom_html.find("IC Mean"), std::string::npos);
  EXPECT_NE(custom_html.find("timestamp"), std::string::npos);
  const auto custom_chart_start = custom_html.find("id=\"fincore-chart-0\"");
  const auto custom_chart_end =
      custom_html.find("id=\"fincore-chart-1\"", custom_chart_start);
  ASSERT_NE(custom_chart_start, std::string::npos);
  ASSERT_NE(custom_chart_end, std::string::npos);
  EXPECT_EQ(
      custom_html
          .substr(custom_chart_start, custom_chart_end - custom_chart_start)
          .find("Date(Number(value)/1e6)"),
      std::string::npos);
  EXPECT_NE(custom_html.find("growth"), std::string::npos);
  EXPECT_NE(custom_html.find("correlation"), std::string::npos);
  EXPECT_EQ(custom_html.find("data-zh=\"最小值\""), std::string::npos);
}

TEST(ReportHtml, EChartsRejectsMalformedSeriesAndUnknownBackend) {
  auto document = demo_document();
  document.sections[0].series[0].x.pop_back();
  const fincore::report_html_options echarts{
      "en", fincore::report_chart_backend::echarts};
  EXPECT_THROW(fincore::render_html(document, echarts), fincore::input_error);

  document = demo_document();
  document.sections[0].series[0].y[0] = std::numeric_limits<double>::infinity();
  EXPECT_THROW(fincore::render_html(document, echarts), fincore::input_error);

  document = demo_document();
  document.sections[0].series[0].x = {0.0};
  document.sections[0].series[0].y = {1.0};
  const auto short_series = fincore::render_html(document, echarts);
  EXPECT_NE(short_series.find("No data"), std::string::npos);
  EXPECT_EQ(short_series.find("fincore-chart-0"), std::string::npos);

  const fincore::report_html_options unknown{
      "en", static_cast<fincore::report_chart_backend>(99)};
  EXPECT_THROW(fincore::render_html(demo_document(), unknown),
               fincore::input_error);

  const fincore::report_html_options unknown_language{
      "fr", fincore::report_chart_backend::echarts};
  EXPECT_THROW(fincore::render_html(demo_document(), unknown_language),
               fincore::input_error);
}

TEST(ReportHtml, TimestampTooltipEscapesCanonicalSeriesTitleAndBoundsAxis) {
  auto document = factor_document();
  document.sections[0].series[0].title =
      "<img src=x onerror=alert(1)> hostile title";
  const fincore::report_html_options options{
      "en", fincore::report_chart_backend::echarts};

  const auto html = fincore::render_html(document, options);
  EXPECT_EQ(html.find("<img src=x"), std::string::npos);
  EXPECT_NE(html.find("data:[[0.0,0.1],[1.0,0.2]]"), std::string::npos);
  EXPECT_NE(html.find("window.echarts.format.encodeHTML(item.seriesName)"),
            std::string::npos);
  EXPECT_EQ(occurrences(html, "scale:true,min:\"dataMin\",max:\"dataMax\""),
            8U);
  EXPECT_NE(html.find("isNaN(date.getTime())?String(value):"),
            std::string::npos);
}

TEST(ReportHtml, EChartsChartIdentifiersRemainUniqueAcrossSections) {
  auto document = demo_document();
  auto second = document.sections[0];
  second.key = "second";
  document.sections.push_back(second);
  const fincore::report_html_options options{
      "en", fincore::report_chart_backend::echarts};

  const auto html = fincore::render_html(document, options);
  EXPECT_NE(html.find("id=\"fincore-chart-0\""), std::string::npos);
  EXPECT_NE(html.find("id=\"fincore-chart-1\""), std::string::npos);
}

} // namespace
