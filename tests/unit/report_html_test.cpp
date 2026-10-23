#include <fincore/error.hpp>
#include <fincore/report/html.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string>
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
    table.rows = {{fincore::report_cell::text_cell("<b>"),
                   fincore::report_cell::number_cell(1.5)},
                  {fincore::report_cell::text_cell("b"),
                   fincore::report_cell::missing()}};
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

TEST(ReportHtml, RendersSafeDeterministicDocument) {
    const auto html = fincore::render_html(demo_document());
    EXPECT_EQ(html.rfind("<!doctype html>", 0), 0U);
    EXPECT_NE(html.find("<html lang=\"en\">"), std::string::npos);
    EXPECT_NE(html.find("<title>Title &lt;unsafe&gt;</title>"),
              std::string::npos);
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
    EXPECT_NE(html.find("schema 1"), std::string::npos);
    EXPECT_EQ(html, fincore::render_html(demo_document()));

    fincore::report_html_options chinese;
    chinese.language = "zh";
    const auto chinese_html = fincore::render_html(demo_document(), chinese);
    EXPECT_NE(chinese_html.find("<html lang=\"zh\">"), std::string::npos);
    EXPECT_NE(chinese_html.find("说明"), std::string::npos);
    EXPECT_NE(chinese_html.find("元数据"), std::string::npos);
    EXPECT_NE(chinese_html.find("由 fincore 生成"), std::string::npos);
}

TEST(ReportHtml, HandlesShortSeriesAndMalformedTables) {
    auto document = demo_document();
    document.sections[0].series[0].x = {0.0};
    document.sections[0].series[0].y = {1.0};
    EXPECT_NE(fincore::render_html(document).find("No data"),
              std::string::npos);

    document = demo_document();
    document.sections[0].series[0].y[0] =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::render_html(document), fincore::input_error);

    document = demo_document();
    document.sections[0].tables[0].rows[0].pop_back();
    EXPECT_THROW(fincore::render_html(document), fincore::input_error);
}

}  // namespace
