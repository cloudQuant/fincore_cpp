#include <fincore/error.hpp>
#include <fincore/report/xlsx.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

namespace {

fincore::report_document demo_document() {
    fincore::report_metric metric;
    metric.key = "ratio";
    metric.label = "Ratio";
    metric.unit = "x";
    metric.number = 1.5;
    fincore::report_table table;
    table.key = "t";
    table.title = "Table <1>";
    table.columns = {"name", "value"};
    table.rows = {{fincore::report_cell::text_cell("A&B"),
                   fincore::report_cell::number_cell(0.25)},
                  {fincore::report_cell::boolean_cell(true),
                   fincore::report_cell::missing()}};
    fincore::report_series series;
    series.key = "s";
    series.title = "Series";
    series.x_label = "t";
    series.y_label = "r";
    series.x = {0.0, 1.0};
    series.y = {2.0, 3.0};
    fincore::report_section first;
    first.key = "first";
    first.title = "First";
    first.metrics = {metric};
    first.tables = {table};
    first.series = {series};
    fincore::report_section second;
    second.key = "second";
    second.title = "Second";
    second.metrics = {metric};
    fincore::report_document document;
    document.domain = "demo";
    document.title = "Demo";
    document.sections = {first, second};
    return document;
}

TEST(ReportXlsx, RendersDeterministicWorkbook) {
    const auto document = demo_document();
    const std::string workbook = fincore::render_xlsx(document);
    ASSERT_GT(workbook.size(), 4U);
    EXPECT_EQ(workbook.substr(0, 4), std::string("PK\x03\x04", 4));
    EXPECT_EQ(workbook, fincore::render_xlsx(document));
    EXPECT_NE(workbook.find("xl/workbook.xml"), std::string::npos);
    EXPECT_NE(workbook.find("xl/worksheets/sheet1.xml"), std::string::npos);
    EXPECT_NE(workbook.find("xl/worksheets/sheet2.xml"), std::string::npos);
    EXPECT_NE(workbook.find("inlineStr"), std::string::npos);
    EXPECT_NE(workbook.find("Table &lt;1&gt;"), std::string::npos);
    EXPECT_NE(workbook.find("A&amp;B"), std::string::npos);
    EXPECT_NE(workbook.find("<v>1.5</v>"), std::string::npos);
    EXPECT_NE(workbook.find("<v>0.25</v>"), std::string::npos);
    EXPECT_NE(workbook.find("t=\"b\""), std::string::npos);
    EXPECT_NE(workbook.find("name=\"first\""), std::string::npos);

    fincore::report_document empty;
    empty.domain = "demo";
    empty.title = "Empty";
    const auto empty_workbook = fincore::render_xlsx(empty);
    EXPECT_EQ(empty_workbook.substr(0, 4), std::string("PK\x03\x04", 4));
    EXPECT_NE(empty_workbook.find("name=\"report\""), std::string::npos);
}

TEST(ReportXlsx, RejectsMalformedValues) {
    auto document = demo_document();
    document.sections[0].metrics[0].number =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);

    document = demo_document();
    document.sections[0].tables[0].rows[0].pop_back();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);

    document = demo_document();
    document.sections[0].series[0].y.pop_back();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);
}

}  // namespace
