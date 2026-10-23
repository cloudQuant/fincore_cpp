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
    EXPECT_EQ(result.labels.size(), 600U);
    EXPECT_EQ(result.signals.size(), 300U);
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
    EXPECT_EQ(exported.files.size(), 11U);
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
    std::ifstream labels(directory / "labels.csv");
    std::stringstream labels_text;
    labels_text << labels.rdbuf();
    EXPECT_EQ(labels_text.str().rfind("date,asset,period,value\n", 0), 0U);
    std::ifstream signals(directory / "signals.csv");
    std::stringstream signals_text;
    signals_text << signals.rdbuf();
    EXPECT_EQ(signals_text.str().rfind(
                  "date,asset,factor,direction,value,processed_value,preprocess,"
                  "group\n",
                  0),
              0U);
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

TEST(FactorResearch, AppliesWideUniverseMask) {
    auto fixture = make_fixture();
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C", "D", "E"};
    fincore::dataframe universe;
    std::vector<fincore::timestamp> index;
    for (int date = 0; date < 60; ++date) {
        index.push_back(start + date * day);
    }
    universe.load_index(std::move(index));
    for (int asset = 0; asset < 5; ++asset) {
        universe.load_column<bool>(
            assets[asset],
            std::vector<bool>(60, asset != 4),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    const auto baseline = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, spec());
    ASSERT_EQ(baseline.coverage.size(), 2U);
    EXPECT_EQ(baseline.coverage[0], 300U);

    const auto masked = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, spec(), universe);
    EXPECT_EQ(masked.status, "OK");
    EXPECT_EQ(masked.coverage[0], 240U);
    EXPECT_EQ(masked.coverage[1], 240U);
    ASSERT_EQ(masked.manifest.count("universe"), 1U);
    EXPECT_EQ(masked.manifest.at("universe"), "applied");

    fincore::dataframe four_assets;
    four_assets.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day});
    for (int asset = 0; asset < 4; ++asset) {
        four_assets.load_column<bool>(
            assets[asset], std::vector<bool>(4, true),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    auto short_spec = spec();
    short_spec.start = start;
    short_spec.end = start + 3 * day;
    const auto short_masked = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, short_spec, four_assets);
    EXPECT_EQ(short_masked.coverage[0], 16U);

    fincore::dataframe none;
    none.load_index(std::vector<fincore::timestamp>{start, start + day});
    for (int asset = 0; asset < 5; ++asset) {
        none.load_column<bool>(
            assets[asset], std::vector<bool>(2, false),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     fixture.factor, fixture.prices, spec(), none)),
                 fincore::factor_data_error);
    fincore::dataframe numeric;
    numeric.load_index(std::vector<fincore::timestamp>{start});
    numeric.load_column<double>(
        "A", {1.0}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     fixture.factor, fixture.prices, spec(), numeric)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, DerivesNextOpenToOpenLabels) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    fincore::factor_series factor;
    std::vector<fincore::timestamp> dates;
    for (int date = 0; date < 8; ++date) {
        dates.push_back(start + date * day);
    }
    for (int date = 0; date < 4; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            factor.dates.push_back(dates[static_cast<std::size_t>(date)]);
            factor.assets.push_back(assets[asset]);
            factor.values.push_back(static_cast<double>(asset + 1));
        }
    }
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{dates});
    const double steps[] = {1.0, 1.0, 1.0};
    const double bases[] = {100.0, 50.0, 10.0};
    for (int asset = 0; asset < 3; ++asset) {
        std::vector<double> values;
        for (int date = 0; date < 8; ++date) {
            values.push_back(bases[asset] + steps[asset] * date);
        }
        prices.load_column<double>(assets[asset], std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    fincore::research_spec config;
    config.targets = {"A", "B", "C"};
    config.start = start;
    config.end = start + 3 * day;
    config.periods = {1, 5};
    config.quantiles = 3;
    config.max_loss = 1.0;
    config.preprocess = "none";
    config.stability_lags = {1};
    config.label = "next_open_to_open";
    const auto result =
        fincore::evaluate_factor_research(factor, prices, config);
    EXPECT_EQ(result.status, "OK");
    ASSERT_EQ(result.information_coefficient.labels.size(), 2U);
    EXPECT_EQ(result.information_coefficient.labels[0], "1D");
    EXPECT_EQ(result.information_coefficient.labels[1], "5D");
    ASSERT_EQ(result.coverage.size(), 2U);
    EXPECT_EQ(result.coverage[0], 12U);
    EXPECT_EQ(result.coverage[1], 6U);
    std::size_t finite_ic = 0U;
    std::size_t missing_ic = 0U;
    for (const double value : result.information_coefficient.values) {
        if (std::isfinite(value)) {
            EXPECT_NEAR(value, 1.0, 1e-12);
            ++finite_ic;
        } else {
            ++missing_ic;
        }
    }
    EXPECT_EQ(finite_ic, 6U);
    EXPECT_EQ(missing_ic, 2U);
    ASSERT_EQ(result.manifest.count("label"), 1U);
    EXPECT_EQ(result.manifest.at("label"), "next_open_to_open");

    auto unsupported = config;
    unsupported.label = "unknown_basis";
    EXPECT_THROW(
        static_cast<void>(fincore::evaluate_factor_research(
            factor, prices, unsupported)),
        fincore::factor_data_error);
    auto precomputed = config;
    precomputed.label = "precomputed";
    EXPECT_THROW(
        static_cast<void>(fincore::evaluate_factor_research(
            factor, prices, precomputed)),
        fincore::factor_data_error);
    auto unsorted = prices;
    auto unsorted_index = unsorted.get_index();
    std::swap(unsorted_index[0], unsorted_index[1]);
    fincore::dataframe bad_prices;
    bad_prices.load_index(std::move(unsorted_index));
    for (int asset = 0; asset < 3; ++asset) {
        const auto& values = prices.get_column<double>(assets[asset]);
        bad_prices.load_column<double>(
            assets[asset], std::vector<double>(values.begin(), values.end()),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     factor, bad_prices, config)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, RecordsSignalsWithGroupsAndDirections) {
    auto fixture = make_fixture();
    auto config = spec();
    config.groups = {{"A", "G1"}, {"B", "G1"}, {"C", "G2"},
                     {"D", "G2"}, {"E", "G2"}};
    config.direction_overrides = {{"A", -1}};
    const auto result = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, config);
    ASSERT_EQ(result.signals.size(), 300U);
    ASSERT_EQ(result.manifest.count("groups"), 1U);
    EXPECT_EQ(result.manifest.at("groups"), "5");
    bool found_a = false;
    bool found_c = false;
    for (const auto& signal : result.signals) {
        if (signal.date == fincore::utc_timestamp(2021, 1, 4) &&
            signal.asset == "A") {
            found_a = true;
            EXPECT_EQ(signal.factor, "factor");
            EXPECT_EQ(signal.direction, -1);
            EXPECT_EQ(signal.group, "G1");
            EXPECT_EQ(signal.preprocess, "cross_section_rank");
            const double raw = 0.3 * std::sin(0.17) + 0.1 * std::cos(0.05);
            EXPECT_NEAR(signal.value, -raw, 1e-12);
            EXPECT_TRUE(std::isfinite(signal.processed_value));
        }
        if (signal.date == fincore::utc_timestamp(2021, 1, 4) &&
            signal.asset == "C") {
            found_c = true;
            EXPECT_EQ(signal.direction, 1);
            EXPECT_EQ(signal.group, "G2");
        }
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_c);

    const auto defaults = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, spec());
    ASSERT_FALSE(defaults.signals.empty());
    for (const auto& signal : defaults.signals) {
        EXPECT_EQ(signal.group, "default");
    }

    auto missing = config;
    missing.groups.erase("E");
    EXPECT_THROW(
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, missing)),
        fincore::factor_data_error);
    auto empty = config;
    empty.groups["A"] = "";
    EXPECT_THROW(
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, empty)),
        fincore::factor_data_error);
}

TEST(FactorResearch, EvaluatesMultipleFactorPanelsWithComparisons) {
    auto fixture = make_fixture();
    auto inverse = fixture.factor;
    for (double& value : inverse.values) {
        value = -value;
    }
    std::vector<fincore::named_factor_panel> panels{
        {"F", fixture.factor}, {"G", inverse}};
    auto config = spec();
    config.comparisons = {{"F_vs_G", {"F", "G"}}};
    const auto result = fincore::evaluate_factor_research_panel(
        panels, fixture.prices, config);
    EXPECT_EQ(result.status, "OK");
    ASSERT_EQ(result.factors.size(), 2U);
    EXPECT_EQ(result.factors[0].name, "F");
    EXPECT_EQ(result.factors[1].name, "G");
    EXPECT_EQ(result.signals.size(), 600U);
    for (const auto& signal : result.signals) {
        EXPECT_TRUE(signal.factor == "F" || signal.factor == "G");
    }
    ASSERT_EQ(result.correlations.size(), 1U);
    EXPECT_EQ(result.correlations[0].left, "F");
    EXPECT_EQ(result.correlations[0].right, "G");
    EXPECT_NEAR(result.correlations[0].mean_correlation, -1.0, 1e-12);
    EXPECT_EQ(result.correlations[0].n_dates, 60U);
    ASSERT_EQ(result.comparisons.size(), 2U);
    EXPECT_EQ(result.comparisons[0].comparison, "F_vs_G");
    EXPECT_EQ(result.comparisons[0].period, "1D");
    EXPECT_NEAR(result.comparisons[0].mean_ic, -0.35666666666666664,
                1e-12);
    EXPECT_EQ(result.comparisons[0].n_dates_left, 60U);
    ASSERT_EQ(result.manifest.count("factor_count"), 1U);
    EXPECT_EQ(result.manifest.at("factor_count"), "2");

    auto duplicate = panels;
    duplicate[1].name = "F";
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     duplicate, fixture.prices, config)),
                 fincore::factor_data_error);
    auto unknown = config;
    unknown.comparisons = {{"bad", {"F", "missing"}}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     panels, fixture.prices, unknown)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, EvaluatesWeightedFactorCombinations) {
    auto fixture = make_fixture();
    auto inverse = fixture.factor;
    for (double& value : inverse.values) {
        value = -value;
    }
    std::vector<fincore::named_factor_panel> panels{
        {"F", fixture.factor}, {"G", inverse}};
    auto config = spec();
    config.combinations = {{"combo", {{"F", 0.5}, {"G", -0.5}}}};
    config.comparisons = {{"F_vs_combo", {"F", "combo"}}};
    const auto result = fincore::evaluate_factor_research_panel(
        panels, fixture.prices, config);
    EXPECT_EQ(result.status, "OK");
    ASSERT_EQ(result.factors.size(), 3U);
    EXPECT_EQ(result.factors[0].name, "F");
    EXPECT_EQ(result.factors[1].name, "G");
    EXPECT_EQ(result.factors[2].name, "combo");
    EXPECT_EQ(result.signals.size(), 900U);
    for (const auto& signal : result.signals) {
        EXPECT_TRUE(signal.factor == "F" || signal.factor == "G" ||
                    signal.factor == "combo");
    }
    ASSERT_EQ(result.correlations.size(), 3U);
    EXPECT_EQ(result.correlations[0].left, "F");
    EXPECT_EQ(result.correlations[0].right, "G");
    EXPECT_NEAR(result.correlations[0].mean_correlation, -1.0, 1e-12);
    EXPECT_EQ(result.correlations[0].n_dates, 60U);
    EXPECT_EQ(result.correlations[1].left, "F");
    EXPECT_EQ(result.correlations[1].right, "combo");
    EXPECT_GT(result.correlations[1].mean_correlation, 0.0);
    EXPECT_EQ(result.correlations[1].n_dates, 60U);
    EXPECT_EQ(result.correlations[2].left, "G");
    EXPECT_EQ(result.correlations[2].right, "combo");
    EXPECT_LT(result.correlations[2].mean_correlation, 0.0);
    EXPECT_EQ(result.correlations[2].n_dates, 60U);
    ASSERT_EQ(result.comparisons.size(), 2U);
    for (const auto& row : result.comparisons) {
        EXPECT_EQ(row.comparison, "F_vs_combo");
        EXPECT_NEAR(row.mean_ic, 0.0, 1e-12);
        EXPECT_EQ(row.n_dates_left, 60U);
        EXPECT_EQ(row.n_dates_right, 60U);
    }
    EXPECT_EQ(result.manifest.at("factor_count"), "3");
    EXPECT_EQ(result.manifest.at("combination_count"), "1");
    EXPECT_EQ(result.manifest.at("correlation_rows"), "3");

    auto unknown = config;
    unknown.combinations = {{"combo", {{"missing", 1.0}}}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     panels, fixture.prices, unknown)),
                 fincore::factor_data_error);
    auto empty_weights = config;
    empty_weights.combinations = {{"combo", {}}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     panels, fixture.prices, empty_weights)),
                 fincore::factor_data_error);
    auto shadow = config;
    shadow.combinations = {{"F", {{"G", 1.0}}}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     panels, fixture.prices, shadow)),
                 fincore::factor_data_error);
    auto non_finite = config;
    non_finite.combinations = {
        {"combo", {{"F", 1.0}, {"G", std::nan("")}}}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     panels, fixture.prices, non_finite)),
                 fincore::factor_data_error);
    auto zero = config;
    zero.combinations = {{"combo", {{"F", 0.0}, {"G", 0.0}}}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_panel(
                     panels, fixture.prices, zero)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, DerivesCalendarPeriodLabelsForWeeklyGrid) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto week = 7 * 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    std::vector<fincore::timestamp> dates;
    for (int index = 0; index < 10; ++index) {
        dates.push_back(start + index * week);
    }
    fincore::factor_series factor;
    for (int date = 0; date < 5; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            factor.dates.push_back(dates[static_cast<std::size_t>(date)]);
            factor.assets.push_back(assets[asset]);
            factor.values.push_back(static_cast<double>(asset + 1));
        }
    }
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{dates});
    const double bases[] = {100.0, 50.0, 10.0};
    for (int asset = 0; asset < 3; ++asset) {
        std::vector<double> values;
        for (int date = 0; date < 10; ++date) {
            values.push_back(bases[asset] + date);
        }
        prices.load_column<double>(assets[asset], std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    fincore::research_spec config;
    config.targets = {"A", "B", "C"};
    config.start = start;
    config.end = start + 4 * week;
    config.periods = {1};
    config.quantiles = 3;
    config.max_loss = 1.0;
    config.preprocess = "none";
    config.stability_lags = {1};
    config.label = "next_open_to_open";
    const auto next_open =
        fincore::evaluate_factor_research(factor, prices, config);
    ASSERT_EQ(next_open.information_coefficient.labels.size(), 1U);
    EXPECT_EQ(next_open.information_coefficient.labels[0], "7D");
    EXPECT_EQ(next_open.coverage[0], 15U);
    for (const double value : next_open.information_coefficient.values) {
        EXPECT_NEAR(value, 1.0, 1e-12);
    }

    config.label = "close_to_close";
    const auto close =
        fincore::evaluate_factor_research(factor, prices, config);
    ASSERT_EQ(close.information_coefficient.labels.size(), 1U);
    EXPECT_EQ(close.information_coefficient.labels[0], "7D");
    EXPECT_EQ(close.coverage[0], 15U);
}

}  // namespace
