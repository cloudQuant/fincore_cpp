#include <fincore/error.hpp>
#include <fincore/report/models.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

fincore::report_document demo_document() {
    fincore::report_metric number;
    number.key = "m1";
    number.label = "M";
    number.unit = "ratio";
    number.number = 1.5;
    fincore::report_metric text;
    text.key = "m2";
    text.label = "State";
    text.text = "ok";
    fincore::report_metric missing;
    missing.key = "m3";
    missing.label = "Missing";
    fincore::report_table table;
    table.key = "t1";
    table.title = "Table";
    table.columns = {"date", "value"};
    table.rows = {
        {fincore::report_cell::text_cell("2022-01-03"),
         fincore::report_cell::number_cell(0.01)},
        {fincore::report_cell::text_cell("2022-01-04"),
         fincore::report_cell::missing()}};
    fincore::report_series series;
    series.key = "series";
    series.title = "Series";
    series.x_label = "t";
    series.y_label = "r";
    series.unit = "return";
    series.x = {0.0, 1.0};
    series.y = {1.0, 2.0};
    fincore::report_section section;
    section.key = "s1";
    section.title = "Section";
    section.metrics = {number, text, missing};
    section.tables = {table};
    section.series = {series};
    section.notes = {"n"};
    fincore::report_document document;
    document.domain = "demo";
    document.title = "Demo";
    document.metadata = {{"b", "2"}, {"a", "1"}};
    document.sections = {section};
    return document;
}

TEST(ReportModels, SerializesDeterministically) {
    const auto document = demo_document();
    const std::string expected =
        "{\n"
        "  \"domain\": \"demo\",\n"
        "  \"metadata\": {\n"
        "    \"a\": \"1\",\n"
        "    \"b\": \"2\"\n"
        "  },\n"
        "  \"schema_version\": 1,\n"
        "  \"sections\": [\n"
        "    {\n"
        "      \"key\": \"s1\",\n"
        "      \"metrics\": [\n"
        "        {\n"
        "          \"key\": \"m1\",\n"
        "          \"kind\": \"number\",\n"
        "          \"label\": \"M\",\n"
        "          \"unit\": \"ratio\",\n"
        "          \"value\": 1.5\n"
        "        },\n"
        "        {\n"
        "          \"key\": \"m2\",\n"
        "          \"kind\": \"text\",\n"
        "          \"label\": \"State\",\n"
        "          \"unit\": \"\",\n"
        "          \"value\": \"ok\"\n"
        "        },\n"
        "        {\n"
        "          \"key\": \"m3\",\n"
        "          \"kind\": \"missing\",\n"
        "          \"label\": \"Missing\",\n"
        "          \"unit\": \"\",\n"
        "          \"value\": null\n"
        "        }\n"
        "      ],\n"
        "      \"notes\": [\n"
        "        \"n\"\n"
        "      ],\n"
        "      \"series\": [\n"
        "        {\n"
        "          \"key\": \"series\",\n"
        "          \"title\": \"Series\",\n"
        "          \"unit\": \"return\",\n"
        "          \"x\": [\n"
        "            0.0,\n"
        "            1.0\n"
        "          ],\n"
        "          \"x_label\": \"t\",\n"
        "          \"y\": [\n"
        "            1.0,\n"
        "            2.0\n"
        "          ],\n"
        "          \"y_label\": \"r\"\n"
        "        }\n"
        "      ],\n"
        "      \"tables\": [\n"
        "        {\n"
        "          \"columns\": [\n"
        "            \"date\",\n"
        "            \"value\"\n"
        "          ],\n"
        "          \"key\": \"t1\",\n"
        "          \"rows\": [\n"
        "            [\n"
        "              \"2022-01-03\",\n"
        "              0.01\n"
        "            ],\n"
        "            [\n"
        "              \"2022-01-04\",\n"
        "              null\n"
        "            ]\n"
        "          ],\n"
        "          \"title\": \"Table\"\n"
        "        }\n"
        "      ],\n"
        "      \"title\": \"Section\"\n"
        "    }\n"
        "  ],\n"
        "  \"title\": \"Demo\"\n"
        "}\n";
    EXPECT_EQ(document.to_json(), expected);
    EXPECT_EQ(document.to_json(), document.to_json());
    EXPECT_EQ(document.to_json().back(), '\n');
}

TEST(ReportModels, RejectsMalformedAndNonFiniteValues) {
    auto document = demo_document();
    document.sections[0].metrics[0].number =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(document.to_json(), fincore::input_error);

    document = demo_document();
    document.sections[0].tables[0].rows[0].pop_back();
    EXPECT_THROW(document.to_json(), fincore::input_error);

    document = demo_document();
    document.sections[0].series[0].y.pop_back();
    EXPECT_THROW(document.to_json(), fincore::input_error);

    fincore::report_document empty;
    empty.domain = "empty";
    empty.title = "Empty";
    EXPECT_EQ(empty.to_json(),
              "{\n  \"domain\": \"empty\",\n  \"metadata\": {},\n  "
              "\"schema_version\": 1,\n  \"sections\": [],\n  \"title\": "
              "\"Empty\"\n}\n");
}

}  // namespace
