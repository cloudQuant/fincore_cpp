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

TEST(FactorResearch, CorrelatesPanelsPerDateAcrossCommonAssets) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    const std::vector<std::vector<double>> x_rows{
        {1.0, 2.0, 3.0}, {2.0, 3.0, 4.0}, {3.0, 4.0, 5.0}, {4.0, 5.0, 6.0}};
    const std::vector<std::vector<double>> y_rows{
        {2.0, 1.0, 5.0}, {1.0, 4.0, 3.0}, {5.0, 2.0, 6.0}, {2.0, 3.0, 1.0}};
    const auto make_panel = [&](std::string name,
                                const std::vector<std::vector<double>>& rows) {
        fincore::research_factor_panel panel;
        panel.name = std::move(name);
        for (int date = 0; date < 4; ++date) {
            for (int asset = 0; asset < 3; ++asset) {
                panel.values.dates.push_back(start + date * day);
                panel.values.assets.push_back(assets[asset]);
                panel.values.values.push_back(
                    rows[static_cast<std::size_t>(date)]
                        [static_cast<std::size_t>(asset)]);
            }
        }
        return panel;
    };
    std::vector<fincore::research_factor_panel> panels;
    panels.push_back(make_panel("X", x_rows));
    panels.push_back(make_panel("Y", y_rows));
    auto negated = x_rows;
    for (auto& row : negated) {
        for (double& value : row) {
            value = -value;
        }
    }
    panels.push_back(make_panel("Z", negated));
    const auto correlations =
        fincore::research_factor_correlations(panels, 2);
    ASSERT_EQ(correlations.size(), 3U);
    EXPECT_EQ(correlations[0].left, "X");
    EXPECT_EQ(correlations[0].right, "Y");
    EXPECT_NEAR(correlations[0].mean_correlation, 0.27885564838462495,
                1e-15);
    EXPECT_NEAR(correlations[0].std_correlation, 0.561086034683472, 1e-15);
    EXPECT_EQ(correlations[0].n_dates, 4U);
    EXPECT_DOUBLE_EQ(correlations[1].mean_correlation, -1.0);
    EXPECT_DOUBLE_EQ(correlations[1].std_correlation, 0.0);
    EXPECT_NEAR(correlations[2].mean_correlation, -0.27885564838462495,
                1e-15);

    const auto too_strict =
        fincore::research_factor_correlations(panels, 4);
    ASSERT_EQ(too_strict.size(), 3U);
    EXPECT_EQ(too_strict[0].n_dates, 0U);
    EXPECT_TRUE(std::isnan(too_strict[0].mean_correlation));
    panels[1].name = "X";
    EXPECT_THROW(static_cast<void>(
                     fincore::research_factor_correlations(panels, 1)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, ComparesStoredIcSummaries) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const auto make_result = [&](std::vector<double> ic_values) {
        fincore::research_result result;
        result.information_coefficient.labels = {"1D"};
        result.information_coefficient.dates = {
            start, start + day, start + 2 * day};
        result.information_coefficient.values = std::move(ic_values);
        return result;
    };
    std::vector<fincore::named_research_result> results;
    results.push_back({"X", make_result({0.1, 0.2, 0.3})});
    results.push_back({"Y", make_result({0.05, 0.05, 0.05})});
    const std::map<std::string, std::pair<std::string, std::string>>
        comparisons{{"X_vs_Y", {"X", "Y"}}};
    const auto rows =
        fincore::research_factor_comparisons(results, comparisons);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].comparison, "X_vs_Y");
    EXPECT_EQ(rows[0].period, "1D");
    EXPECT_NEAR(rows[0].mean_ic_left, 0.2, 1e-15);
    EXPECT_NEAR(rows[0].mean_ic_right, 0.05, 1e-15);
    EXPECT_NEAR(rows[0].icir_left, 2.0, 1e-15);
    EXPECT_TRUE(std::isnan(rows[0].icir_right));
    EXPECT_NEAR(rows[0].mean_ic, 0.15, 1e-15);
    EXPECT_TRUE(std::isnan(rows[0].icir));
    EXPECT_EQ(rows[0].n_dates_left, 3U);
    EXPECT_EQ(rows[0].n_dates_right, 3U);

    const std::map<std::string, std::pair<std::string, std::string>>
        unknown{{"bad", {"X", "missing"}}};
    EXPECT_THROW(static_cast<void>(
                     fincore::research_factor_comparisons(results, unknown)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, RecordsSegmentProvenance) {
    auto fixture = make_fixture();
    auto evaluation = spec();
    evaluation.segments = {
        {"first_half", {"2021-01-04", "2021-02-01"}},
        {"second_half", {"2021-02-02", "2021-03-04"}}};
    const auto result = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, evaluation);
    ASSERT_EQ(result.manifest.count("segment.first_half"), 1U);
    EXPECT_EQ(result.manifest.at("segment.first_half"),
              "2021-01-04..2021-02-01");
    ASSERT_EQ(result.manifest.count("segment.second_half"), 1U);
    EXPECT_EQ(result.manifest.at("segment.second_half"),
              "2021-02-02..2021-03-04");

    auto empty_name = evaluation;
    empty_name.segments = {{"", {"2021-01-04", "2021-02-01"}}};
    EXPECT_THROW(
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, empty_name)),
        fincore::factor_data_error);
    auto empty_bound = evaluation;
    empty_bound.segments = {{"x", {"2021-01-04", ""}}};
    EXPECT_THROW(
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, empty_bound)),
        fincore::factor_data_error);
}

TEST(FactorResearch, EvaluatesPrecomputedForwardReturns) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    fincore::factor_series factor;
    std::vector<fincore::timestamp> dates;
    for (int date = 0; date < 5; ++date) {
        dates.push_back(start + date * day);
        for (int asset = 0; asset < 3; ++asset) {
            factor.dates.push_back(start + date * day);
            factor.assets.push_back(assets[asset]);
            factor.values.push_back(static_cast<double>(asset + 1));
        }
    }
    fincore::forward_return_table table;
    table.labels = {"1D", "5D"};
    table.dates = dates;
    table.assets = {"A", "B", "C"};
    for (const auto& label : table.labels) {
        std::vector<double> values;
        const double scale = label == "1D" ? 0.01 : 0.05;
        const double shift = label == "1D" ? 0.001 : 0.002;
        for (int date = 0; date < 5; ++date) {
            for (int asset = 0; asset < 3; ++asset) {
                values.push_back(scale * (asset + 1) + shift * date);
            }
        }
        table.values.push_back(std::move(values));
    }
    fincore::research_spec config;
    config.targets = {"A", "B", "C"};
    config.start = start;
    config.end = start + 4 * day;
    config.periods = {1, 5};
    config.quantiles = 2;
    config.max_loss = 1.0;
    config.preprocess = "none";
    config.stability_lags = {1};
    const auto result =
        fincore::evaluate_factor_research(factor, table, config);
    EXPECT_EQ(result.status, "OK");
    ASSERT_EQ(result.information_coefficient.labels.size(), 2U);
    EXPECT_EQ(result.information_coefficient.labels[0], "1D");
    EXPECT_EQ(result.information_coefficient.labels[1], "5D");
    ASSERT_EQ(result.information_coefficient.dates.size(), 5U);
    for (const double value : result.information_coefficient.values) {
        EXPECT_NEAR(value, 1.0, 1e-12);
    }
    ASSERT_EQ(result.coverage.size(), 2U);
    EXPECT_EQ(result.coverage[0], 15U);
    EXPECT_EQ(result.coverage[1], 15U);
    EXPECT_EQ(result.quantile_returns.quantiles.size(), 2U);
    ASSERT_EQ(result.manifest.count("label"), 1U);
    EXPECT_EQ(result.manifest.at("label"), "precomputed");

    auto missing = table;
    missing.labels = {"1D"};
    missing.values.pop_back();
    EXPECT_THROW(
        static_cast<void>(
            fincore::evaluate_factor_research(factor, missing, config)),
        fincore::factor_data_error);
    auto malformed = table;
    malformed.values[0].pop_back();
    EXPECT_THROW(
        static_cast<void>(
            fincore::evaluate_factor_research(factor, malformed, config)),
        fincore::factor_data_error);
    auto duplicate = table;
    duplicate.dates[1] = duplicate.dates[0];
    EXPECT_THROW(
        static_cast<void>(
            fincore::evaluate_factor_research(factor, duplicate, config)),
        fincore::factor_data_error);
}

}  // namespace
