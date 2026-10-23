#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/research.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct research_fixture {
    fincore::factor_series factor;
    fincore::dataframe prices;
};

research_fixture make_fixture() {
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C", "D", "E"};
    research_fixture fixture;
    for (int date = 0; date < 60; ++date) {
        for (int asset = 0; asset < 5; ++asset) {
            fixture.factor.dates.push_back(start + date * day);
            fixture.factor.assets.push_back(assets[asset]);
            fixture.factor.values.push_back(
                0.3 * std::sin((date + 1) * 0.17 + asset * 0.9) +
                0.1 * std::cos((date + 1) * 0.05 + asset) +
                0.03 * asset);
        }
    }
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 70; ++position) {
        index.push_back(start + position * day);
    }
    fixture.prices.load_index(std::move(index));
    for (int asset = 0; asset < 5; ++asset) {
        std::vector<double> values;
        double price = 100.0 + 5.0 * asset;
        for (int position = 0; position < 70; ++position) {
            price *= 1.0 + 0.002 +
                     0.01 * std::sin((position + 1) * 0.11 + asset * 0.7) +
                     0.004 *
                         std::cos((position + 1) * 0.23 + asset);
            values.push_back(price);
        }
        fixture.prices.load_column<double>(assets[asset], std::move(values),
                                           hmdf::nan_policy::dont_pad_with_nans);
    }
    return fixture;
}

fincore::research_spec spec() {
    fincore::research_spec value;
    value.targets = {"A", "B", "C", "D", "E"};
    value.start = fincore::utc_timestamp(2021, 1, 4);
    value.end = fincore::utc_timestamp(2021, 3, 4);
    value.periods = {1, 5};
    value.quantiles = 3;
    value.max_loss = 0.5;
    value.preprocess = "cross_section_rank";
    value.stability_lags = {1, 5};
    return value;
}

TEST(FactorResearch, EvaluatesPanelsAndReportsTables) {
    auto fixture = make_fixture();
    const auto result =
        fincore::evaluate_factor_research(fixture.factor, fixture.prices,
                                          spec());
    EXPECT_EQ(result.status, "OK");
    ASSERT_EQ(result.information_coefficient.labels.size(), 2U);
    EXPECT_EQ(result.information_coefficient.labels[0], "1D");
    EXPECT_EQ(result.information_coefficient.labels[1], "5D");
    EXPECT_EQ(result.information_coefficient.dates.size(), 60U);
    EXPECT_NEAR(result.information_coefficient.values[0],
                0.9999999999999999, 1e-12);
    EXPECT_NEAR(result.information_coefficient_summary.means[0],
                -0.17833333333333334, 1e-12);
    ASSERT_EQ(result.quantile_returns.quantiles.size(), 3U);
    ASSERT_EQ(result.quantile_returns.means.size(), 6U);
    EXPECT_TRUE(std::isfinite(result.quantile_returns.means[0]));
    EXPECT_GT(result.quantile_returns.means[0], 0.0);
    ASSERT_EQ(result.turnover.size(), 3U);
    ASSERT_EQ(result.stability.size(), 2U);
    ASSERT_EQ(result.spread.size(), 2U);
    EXPECT_LT(result.spread[0], 0.0);
    EXPECT_EQ(result.coverage[0], 300U);
    EXPECT_EQ(result.coverage[1], 300U);
    EXPECT_EQ(result.manifest.at("preprocess"), "cross_section_rank");
    EXPECT_EQ(result.manifest.at("periods"), "2");
    EXPECT_EQ(result.schema_version, 1);

    const auto capabilities = fincore::research_capabilities_report();
    EXPECT_EQ(capabilities.api_version, "1.0");
    EXPECT_EQ(capabilities.statuses.size(), 3U);
    EXPECT_EQ(capabilities.result_tables.size(), 12U);
}

TEST(FactorResearch, PreprocessAndDirectionChangeSignals) {
    auto fixture = make_fixture();
    auto reversed = spec();
    for (const auto& asset : {"A", "B", "C", "D", "E"}) {
        reversed.direction_overrides[asset] = -1;
    }
    const auto normal =
        fincore::evaluate_factor_research(fixture.factor, fixture.prices,
                                          spec());
    const auto flipped = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, reversed);
    EXPECT_LT(flipped.information_coefficient.values[0], 0.0);
    EXPECT_NE(flipped.information_coefficient.values[0],
              normal.information_coefficient.values[0]);

    auto raw = spec();
    raw.preprocess = "none";
    const auto unprocessed = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, raw);
    EXPECT_EQ(unprocessed.status, "OK");
    EXPECT_EQ(unprocessed.quantile_returns.quantiles.size(),
              normal.quantile_returns.quantiles.size());

    auto invalid = spec();
    invalid.preprocess = "unknown";
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     fixture.factor, fixture.prices, invalid)),
                 fincore::factor_data_error);
    auto empty = spec();
    empty.start = fincore::utc_timestamp(2030, 1, 1);
    empty.end = fincore::utc_timestamp(2030, 2, 1);
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     fixture.factor, fixture.prices, empty)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, ExportsCsvTablesWithoutOverwriting) {
    fincore::research_result result;
    result.status = "OK";
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    result.information_coefficient.labels = {"1D"};
    result.information_coefficient.dates = {start, start + day};
    result.information_coefficient.values = {0.1, 0.2};
    result.information_coefficient_summary.labels = {"1D"};
    result.information_coefficient_summary.means = {0.15};
    result.quantile_returns.quantiles = {1, 3};
    result.quantile_returns.labels = {"1D"};
    result.quantile_returns.means = {-0.01, 0.02};
    result.quantile_returns.standard_errors = {0.001, 0.002};
    result.turnover.push_back(
        {{start, start + day}, {0.25, 0.5}});
    result.stability.push_back(
        {{start, start + day}, {std::nan(""), 1.0}});
    result.stability_lags = {2};
    result.spread = {0.03};
    result.coverage = {10};
    result.diagnostics = {"a warning"};
    result.manifest = {{"api_version", "1.0"}};

    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    const auto directory =
        std::filesystem::temp_directory_path() /
        ("fincore-research-export-" + std::to_string(stamp));
    std::filesystem::remove_all(directory);

    const auto exported = fincore::export_factor_research(
        result, directory.string());
    EXPECT_EQ(exported.directory, directory.string());
    EXPECT_EQ(exported.files.size(), 9U);
    for (const auto& name : exported.files) {
        EXPECT_TRUE(std::filesystem::exists(directory / name));
    }
    std::ifstream ic(directory / "ic.csv");
    std::stringstream ic_text;
    ic_text << ic.rdbuf();
    EXPECT_EQ(ic_text.str().rfind("date,1D\n", 0), 0U);
    EXPECT_NE(ic_text.str().find("0.1"), std::string::npos);
    std::ifstream manifest(directory / "manifest.json");
    std::stringstream manifest_text;
    manifest_text << manifest.rdbuf();
    EXPECT_NE(manifest_text.str().find("\"ic.csv\""), std::string::npos);
    EXPECT_NE(manifest_text.str().find("api_version"), std::string::npos);
    std::ifstream turnover(directory / "turnover.csv");
    std::stringstream turnover_text;
    turnover_text << turnover.rdbuf();
    EXPECT_NE(turnover_text.str().find("1,"), std::string::npos);

    EXPECT_THROW(
        static_cast<void>(fincore::export_factor_research(
            result, directory.string())),
        fincore::factor_data_error);
    const auto charts_directory = directory.string() + "-charts";
    EXPECT_THROW(
        static_cast<void>(fincore::export_factor_research(
            result, charts_directory, true)),
        fincore::factor_data_error);
    std::filesystem::remove_all(directory);
}

}  // namespace
