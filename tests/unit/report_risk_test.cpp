#include <fincore/calendar.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/risk.hpp>
#include <fincore/risk/report.hpp>
#include <fincore/risk/walk_forward.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

fincore::dataframe risk_fixture(int count = 300) {
    std::vector<fincore::timestamp> index;
    std::vector<double> values;
    for (int position = 0; position < count; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 1) +
                        position * 86'400'000'000'000LL);
        values.push_back(0.01 * std::sin(position * 0.7));
    }
    return fincore::make_series(index, "returns", values);
}

fincore::risk_model_spec small_spec() {
    fincore::risk_model_spec spec;
    spec.distribution = "normal";
    spec.window = 5U;
    spec.refit_cadence = 2U;
    spec.confidence_level = 0.90;
    return spec;
}

TEST(ReportRisk, ProjectsValidationReport) {
    const auto result =
        fincore::walk_forward_var(risk_fixture(12), small_spec());
    const auto report = fincore::build_risk_validation_report(result);
    const auto document = fincore::build_risk_report(report);
    EXPECT_EQ(document.domain, "risk");
    EXPECT_EQ(document.title, "Risk Validation Report");
    ASSERT_EQ(document.sections.size(), 1U);
    const auto& section = document.sections[0];
    EXPECT_EQ(section.key, "risk_validation");
    ASSERT_EQ(section.tables.size(), 2U);
    EXPECT_EQ(section.tables[0].key, "forecast_events");
    ASSERT_EQ(section.tables[0].rows.size(), 7U);
    EXPECT_EQ(section.tables[1].key, "refits");
    ASSERT_EQ(section.tables[1].rows.size(), 4U);
    ASSERT_FALSE(section.notes.empty());
    EXPECT_EQ(section.notes[0], report.disclosure);
    bool has_status = false;
    bool has_window = false;
    bool has_zone = false;
    for (const auto& metric : section.metrics) {
        if (metric.key == "status") {
            has_status = true;
            ASSERT_TRUE(metric.text.has_value());
            EXPECT_EQ(*metric.text, "ok");
        }
        if (metric.key == "diagnostics.window") {
            has_window = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_DOUBLE_EQ(*metric.number, 5.0);
        }
        if (metric.key == "backtest.traffic_light") {
            has_zone = true;
            ASSERT_TRUE(metric.text.has_value());
            EXPECT_EQ(*metric.text, "yellow");
        }
    }
    EXPECT_TRUE(has_status);
    EXPECT_TRUE(has_window);
    EXPECT_TRUE(has_zone);
    EXPECT_EQ(document.metadata.at("inputs_digest"),
              report.inputs_digest);

    const std::string json = document.to_json();
    EXPECT_NE(json.find("\"domain\": \"risk\""), std::string::npos);
    EXPECT_NE(json.find("\"forecast_events\""), std::string::npos);
    EXPECT_NE(json.find("\"backtest.kupiec_lr\""), std::string::npos);
    EXPECT_EQ(json, document.to_json());

    const std::string html = fincore::render_html(document);
    EXPECT_NE(html.find("Risk validation"), std::string::npos);
    EXPECT_NE(html.find("Forecast events"), std::string::npos);
    EXPECT_NE(html.find("yellow"), std::string::npos);
}

TEST(ReportRisk, ProjectsInsufficientReportWithoutBacktest) {
    auto spec = small_spec();
    spec.window = 300U;
    const auto result =
        fincore::walk_forward_var(risk_fixture(), spec);
    const auto report = fincore::build_risk_validation_report(result);
    const auto document = fincore::build_risk_report(report);
    const auto& section = document.sections[0];
    EXPECT_TRUE(section.tables[0].rows.empty());
    EXPECT_TRUE(section.tables[1].rows.empty());
    bool has_reason = false;
    for (const auto& metric : section.metrics) {
        if (metric.key == "diagnostics.reason") {
            has_reason = true;
            ASSERT_TRUE(metric.text.has_value());
            EXPECT_FALSE(metric.text->empty());
        }
        EXPECT_NE(metric.key, "backtest.kupiec_lr");
    }
    EXPECT_TRUE(has_reason);
}

}  // namespace
