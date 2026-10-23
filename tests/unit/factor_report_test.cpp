#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/report.hpp>
#include <fincore/report/html.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

fincore::prepared_factor_data tiny_prepared() {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    for (int date = 0; date < 3; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset + 1;
            row.forward_returns = {0.01 * (asset + 1)};
            data.rows.push_back(row);
        }
    }
    return data;
}

TEST(FactorReport, ProjectsModelToSharedDocument) {
    const auto model = fincore::analyze_factor(tiny_prepared());
    const auto report = fincore::build_factor_report(model, "Tiny Factor");

    EXPECT_EQ(report.domain, "factor");
    EXPECT_EQ(report.title, "Tiny Factor");
    EXPECT_EQ(report.schema_version, 1);
    ASSERT_EQ(report.metadata.count("factor_result_fingerprint"), 1U);
    EXPECT_EQ(report.metadata.at("factor_result_fingerprint").size(), 64U);
    EXPECT_EQ(report.metadata.at("forward_periods"), "1D");
    EXPECT_EQ(report.metadata.at("asset_count"), "3");
    ASSERT_EQ(report.sections.size(), 4U);
    EXPECT_EQ(report.sections[0].key, "factor_summary");
    EXPECT_EQ(report.sections[1].key, "factor_information");
    EXPECT_EQ(report.sections[2].key, "factor_returns");
    EXPECT_EQ(report.sections[3].key, "factor_turnover");
    bool has_mean_ic = false;
    for (const auto& metric : report.sections[0].metrics) {
        if (metric.key == "mean_information_coefficient.1D") {
            has_mean_ic = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_NEAR(*metric.number, 1.0, 1e-12);
        }
    }
    EXPECT_TRUE(has_mean_ic);
    EXPECT_EQ(report.sections[0].tables.size(), 4U);
    EXPECT_EQ(report.sections[0].series.size(), 1U);
    EXPECT_EQ(report.sections[0].notes.size(), 2U);
    EXPECT_EQ(report.sections[1].tables.size(), 1U);
    EXPECT_EQ(report.sections[1].series.size(), 1U);
    EXPECT_EQ(report.sections[2].tables.size(), 3U);
    EXPECT_EQ(report.sections[2].notes.size(), 2U);
    EXPECT_EQ(report.sections[3].tables.size(), 3U);
    ASSERT_FALSE(report.sections[2].tables[0].rows.empty());
    EXPECT_DOUBLE_EQ(report.sections[2].tables[0].rows[2][1].number,
                     0.01 * 10'000.0);

    const std::string json = report.to_json();
    EXPECT_EQ(json, report.to_json());
    EXPECT_NE(json.find("\"domain\": \"factor\""), std::string::npos);
    EXPECT_NE(json.find("\"factor_summary\""), std::string::npos);
    EXPECT_NE(json.find("\"information_coefficient\""), std::string::npos);
    EXPECT_NE(json.find("\"mean_returns_by_quantile\""), std::string::npos);
    EXPECT_NE(json.find("\"alpha_beta\""), std::string::npos);
    EXPECT_NE(json.find("\"mean_turnover_by_quantile\""), std::string::npos);
    EXPECT_NE(json.find("\"mean_information_coefficient.1D\""),
              std::string::npos);
    EXPECT_NE(json.find("\"factor_cumulative_returns.1D\""),
              std::string::npos);
    EXPECT_EQ(json.back(), '\n');

    const auto html = fincore::render_html(report);
    EXPECT_EQ(html.rfind("<!doctype html>", 0), 0U);
    EXPECT_NE(html.find("Factor returns"), std::string::npos);
    EXPECT_NE(html.find("<svg"), std::string::npos);
    fincore::report_html_options chinese;
    chinese.language = "zh";
    const auto chinese_html = fincore::render_html(report, chinese);
    EXPECT_NE(chinese_html.find("说明"), std::string::npos);
    EXPECT_NE(chinese_html.find("<html lang=\"zh\">"), std::string::npos);
}

}  // namespace
