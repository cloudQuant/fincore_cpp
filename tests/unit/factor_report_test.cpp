#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/report.hpp>

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
    ASSERT_EQ(report.metadata.count("fingerprint"), 1U);
    EXPECT_EQ(report.metadata.at("fingerprint").size(), 64U);
    ASSERT_EQ(report.sections.size(), 2U);
    EXPECT_EQ(report.sections[0].key, "factor_summary");
    EXPECT_EQ(report.sections[1].key, "factor_performance");
    bool has_mean_ic = false;
    for (const auto& metric : report.sections[0].metrics) {
        if (metric.key == "mean_ic.1D") {
            has_mean_ic = true;
            ASSERT_TRUE(metric.number.has_value());
            EXPECT_NEAR(*metric.number, 1.0, 1e-12);
        }
    }
    EXPECT_TRUE(has_mean_ic);
    EXPECT_EQ(report.sections[1].tables.size(), 4U);
    EXPECT_EQ(report.sections[1].series.size(), 1U);
    EXPECT_EQ(report.sections[1].notes.size(), 2U);

    const std::string json = report.to_json();
    EXPECT_EQ(json, report.to_json());
    EXPECT_NE(json.find("\"domain\": \"factor\""), std::string::npos);
    EXPECT_NE(json.find("\"factor_summary\""), std::string::npos);
    EXPECT_NE(json.find("\"information_coefficient\""), std::string::npos);
    EXPECT_NE(json.find("\"quantile_returns\""), std::string::npos);
    EXPECT_NE(json.find("\"alpha_beta\""), std::string::npos);
    EXPECT_NE(json.find("\"turnover\""), std::string::npos);
    EXPECT_NE(json.find("\"mean_ic.1D\""), std::string::npos);
    EXPECT_EQ(json.back(), '\n');
}

}  // namespace
