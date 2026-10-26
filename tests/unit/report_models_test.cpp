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
    section.units = {
        {"m1", "ratio"}, {"series", "return"}, {"t1", "rows"}};
    section.legends = {{"series", "Strategy"}};
    fincore::report_document document;
    document.domain = "demo";
    document.title = "Demo";
    document.metadata = {{"b", "2"}, {"a", "1"}};
    document.sections = {section};
    document.offline_assets = {{"chart.js", "window.chart = true;"}};
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
        "  \"offline_assets\": {\n"
        "    \"chart.js\": \"window.chart = true;\"\n"
        "  },\n"
        "  \"schema_version\": 2,\n"
        "  \"sections\": [\n"
        "    {\n"
        "      \"key\": \"s1\",\n"
        "      \"legends\": {\n"
        "        \"series\": \"Strategy\"\n"
        "      },\n"
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
        "      \"title\": \"Section\",\n"
        "      \"units\": {\n"
        "        \"m1\": \"ratio\",\n"
        "        \"series\": \"return\",\n"
        "        \"t1\": \"rows\"\n"
        "      }\n"
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

    document = demo_document();
    document.sections[0].metrics[0].number =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(document.to_json(), fincore::input_error);

    fincore::report_document empty;
    empty.domain = "empty";
    empty.title = "Empty";
    EXPECT_EQ(empty.to_json(),
              "{\n  \"domain\": \"empty\",\n  \"metadata\": {},\n  "
              "\"offline_assets\": {},\n  \"schema_version\": 2,\n  "
              "\"sections\": [],\n  \"title\": \"Empty\"\n}\n");
}

TEST(ReportModels, AppendedFieldsPreservePriorAggregateInitializers) {
    const fincore::report_section section{
        "s", "title", {}, {}, {}, {}};
    const fincore::report_document document{
        "domain", "title", {section}, {}, 1};

    EXPECT_EQ(section.units.size(), 0U);
    EXPECT_EQ(section.legends.size(), 0U);
    EXPECT_EQ(document.offline_assets.size(), 0U);
    EXPECT_EQ(document.schema_version, 1);
}

TEST(ReportModels, IncludesSectionMetadataAndOfflineAssets) {
    const auto document = demo_document();
    const auto json = document.to_json();

    EXPECT_NE(json.find("\"units\""), std::string::npos);
    EXPECT_NE(json.find("\"legends\""), std::string::npos);
    EXPECT_NE(json.find("\"offline_assets\""), std::string::npos);
    EXPECT_NE(json.find("window.chart = true;"), std::string::npos);
    EXPECT_EQ(document.schema_version, fincore::report_document_schema_version);
    EXPECT_EQ(document.schema_version, 2);
}

TEST(ReportModels, SemanticPayloadHashesAssetContentAndIsDeterministic) {
    auto document = demo_document();
    auto reordered = demo_document();
    reordered.metadata = {{"a", "1"}, {"b", "2"}};
    reordered.offline_assets = {{"chart.js", "window.chart = true;"}};
    reordered.sections[0].units = {
        {"series", "return"}, {"m1", "ratio"}, {"t1", "rows"}};
    reordered.sections[0].legends = {{"series", "Strategy"}};

    EXPECT_EQ(document.to_json(), reordered.to_json());
    EXPECT_EQ(document.semantic_payload(), reordered.semantic_payload());
    EXPECT_EQ(document.semantic_digest(), reordered.semantic_digest());

    const auto payload = document.semantic_payload();
    EXPECT_NE(payload.find("chart.js"), std::string::npos);
    EXPECT_EQ(payload.find("window.chart = true;"), std::string::npos);

    auto changed_asset = document;
    changed_asset.offline_assets["chart.js"] = "window.chart = false;";
    EXPECT_NE(document.semantic_digest(), changed_asset.semantic_digest());
}

TEST(ReportModels, OwnsAppendedMetadataAndSupportsSectionLookup) {
    std::map<std::string, std::string> units{{"m1", "ratio"}};
    std::map<std::string, std::string> assets{{"asset.js", "first"}};
    fincore::report_section section;
    section.key = "s1";
    section.metrics.push_back({"m1", "Metric", "ratio", 1.0, {}, {}});
    section.units = units;
    fincore::report_document document;
    document.domain = "d";
    document.title = "t";
    document.sections.push_back(section);
    document.offline_assets = assets;

    units["m1"] = "changed";
    assets["asset.js"] = "changed";
    EXPECT_EQ(document.section("s1").units.at("m1"), "ratio");
    EXPECT_EQ(document.offline_assets.at("asset.js"), "first");

    document.section("s1").units["m1"] = "percent";
    EXPECT_EQ(document.sections[0].units.at("m1"), "percent");
    EXPECT_THROW(document.section("missing"), fincore::input_error);
    EXPECT_THROW(std::as_const(document).section("missing"), fincore::input_error);
}

TEST(ReportModels, RejectsDuplicateSectionsAndUnknownMetadataReferences) {
    auto document = demo_document();
    document.sections.push_back(document.sections.front());
    EXPECT_THROW(document.to_json(), fincore::input_error);
    EXPECT_THROW(document.semantic_payload(), fincore::input_error);
    EXPECT_THROW(document.section("s1"), fincore::input_error);

    document = demo_document();
    document.sections[0].units["unknown"] = "ratio";
    EXPECT_THROW(document.to_json(), fincore::input_error);
    EXPECT_THROW(document.section("s1"), fincore::input_error);

    document = demo_document();
    document.sections[0].legends["unknown"] = "legend";
    EXPECT_THROW(document.semantic_payload(), fincore::input_error);

    auto section = document.sections[0];
    section.units["unknown"] = "ratio";
    EXPECT_THROW(section.semantic_payload(), fincore::input_error);
}

TEST(ReportModels, RejectsUnsupportedDocumentSchemaVersions) {
    auto document = demo_document();
    document.schema_version = 1;
    EXPECT_THROW(document.to_json(), fincore::input_error);
    EXPECT_THROW(document.semantic_payload(), fincore::input_error);
    EXPECT_THROW(document.section("s1"), fincore::input_error);

    document.schema_version = 3;
    EXPECT_THROW(document.to_json(), fincore::input_error);
    EXPECT_THROW(document.semantic_digest(), fincore::input_error);
}

TEST(ReportModels, DistinguishesMissingPositiveAndNegativeZero) {
    auto negative_zero = demo_document();
    negative_zero.sections[0].metrics[0].number = -0.0;
    const auto negative_json = negative_zero.to_json();
    EXPECT_NE(negative_json.find("-0.0"), std::string::npos);
    EXPECT_NE(negative_json.find("\"kind\": \"missing\""),
              std::string::npos);

    auto positive_zero = demo_document();
    positive_zero.sections[0].metrics[0].number = 0.0;
    EXPECT_NE(positive_zero.to_json().find("0.0"), std::string::npos);
    EXPECT_NE(negative_zero.semantic_digest(), positive_zero.semantic_digest());

    auto missing = demo_document();
    missing.sections[0].metrics[0].number.reset();
    EXPECT_NE(positive_zero.semantic_digest(), missing.semantic_digest());
    EXPECT_NE(missing.to_json().find(
                  "\"key\": \"m1\",\n          \"kind\": \"missing\""),
              std::string::npos);
}

}  // namespace
