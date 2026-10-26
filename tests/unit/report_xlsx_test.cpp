#include <fincore/error.hpp>
#include <fincore/report/xlsx.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
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

fincore::report_document wide_document() {
    fincore::report_metric numeric_metric;
    numeric_metric.key = "finite";
    numeric_metric.label = "Finite value";
    numeric_metric.unit = "x";
    numeric_metric.number = 1.25;

    fincore::report_metric boolean_metric;
    boolean_metric.key = "enabled";
    boolean_metric.label = "Enabled";
    boolean_metric.boolean = true;

    fincore::report_table table;
    table.key = "wide_table";
    table.title = "Wide table";
    for (std::size_t index = 0; index < 703U; ++index) {
        table.columns.push_back("column_" + std::to_string(index + 1U));
    }
    std::vector<fincore::report_cell> values(
        table.columns.size(), fincore::report_cell::missing());
    values[0] = fincore::report_cell::number_cell(7.5);
    values[25] = fincore::report_cell::text_cell("Z-boundary");
    values[26] = fincore::report_cell::text_cell("AA-boundary");
    values[51] = fincore::report_cell::text_cell("=SUM(1,2)");
    values[52] = fincore::report_cell::text_cell("收益率📈");
    values[100] = fincore::report_cell::text_cell("formula-like =1+1");
    values[101] =
        fincore::report_cell::text_cell("control\x01" "character");
    values[102] = fincore::report_cell::text_cell("before\xEF\xBF\xBE" "after");
    values[103] = fincore::report_cell::text_cell(
        std::string{"bad"} + static_cast<char>(0xC0U) + "sequence");
    values[701] = fincore::report_cell::boolean_cell(true);
    values[702] = fincore::report_cell::text_cell("AAA-boundary & <tag>");
    table.rows.push_back(std::move(values));

    fincore::report_section wide;
    wide.key = "wide";
    wide.metrics = {numeric_metric, boolean_metric};
    wide.tables = {std::move(table)};

    fincore::report_metric second_metric;
    second_metric.key = "second_value";
    second_metric.label = "第二部分";
    second_metric.text = "second sheet";
    fincore::report_section second;
    second.key = "第二部分";
    second.metrics = {second_metric};

    fincore::report_document document;
    document.domain = "xlsx-verification";
    document.title = "Wide XLSX verification";
    document.sections = {std::move(wide), std::move(second)};
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
    EXPECT_NE(workbook.find("<cellStyle name=\"Normal\""),
              std::string::npos);
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

TEST(ReportXlsx, UsesSpreadsheetColumnNamesAcrossBoundaries) {
    const auto document = wide_document();
    const std::string workbook = fincore::render_xlsx(document);
    EXPECT_EQ(workbook, fincore::render_xlsx(document));

    for (const auto* reference : {"Z6", "AA6", "AZ6", "BA6", "CZ6",
                                  "ZZ6", "AAA6", "AAA7"}) {
        EXPECT_NE(workbook.find("r=\"" + std::string(reference) + "\""),
                  std::string::npos)
            << "missing worksheet cell " << reference;
    }
    EXPECT_NE(workbook.find(
                  "<c r=\"AA6\" t=\"inlineStr\"><is><t xml:space=\"preserve\">column_27"),
              std::string::npos);

    const char* artifact_path = std::getenv("FINCORE_XLSX_VERIFY_OUTPUT");
    if (artifact_path != nullptr && artifact_path[0] != '\0') {
        std::ofstream artifact(artifact_path, std::ios::binary);
        ASSERT_TRUE(artifact.good()) << "cannot write XLSX sample to "
                                     << artifact_path;
        artifact.write(workbook.data(),
                       static_cast<std::streamsize>(workbook.size()));
        ASSERT_TRUE(artifact.good()) << "failed writing XLSX sample to "
                                     << artifact_path;
    }
}

TEST(ReportXlsx, RejectsMalformedValues) {
    auto document = demo_document();
    document.sections[0].metrics[0].number =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);

    document = demo_document();
    document.sections[0].metrics[0].number =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);

    document = demo_document();
    document.sections[0].tables[0].rows[0].pop_back();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);

    document = demo_document();
    document.sections[0].series[0].y.pop_back();
    EXPECT_THROW(fincore::render_xlsx(document), fincore::input_error);
}

}  // namespace
