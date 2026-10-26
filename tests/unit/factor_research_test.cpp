#include <fincore/calendar.hpp>
#include <fincore/detail/factor_compute.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/research.hpp>
#include "../../src/factor/research_export_internal.hpp"
#include "../../src/factor/research_detailed_internal.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <atomic>
#include <thread>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct research_fixture {
    fincore::factor_series factor;
    fincore::dataframe prices;
};

// Nothing here is random: the factor and price values are fixed trigonometric
// patterns, so a difference against an oracle cannot be blamed on a draw. The
// shapes are chosen to disagree on purpose — sixty factor dates over seventy price
// dates, five assets — leaving price rows past the factor window for the cases
// that check what a truncated study does.
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

research_fixture make_grid_fixture(
    const std::vector<fincore::timestamp>& dates,
    const fincore::timestamp start,
    const fincore::timestamp end) {
    const char* assets[] = {"A", "B", "C"};
    research_fixture fixture;
    fixture.prices.load_index(std::vector<fincore::timestamp>{dates});
    for (std::size_t position = 0; position < dates.size(); ++position) {
        if (dates[position] < start || dates[position] > end) {
            continue;
        }
        for (int asset = 0; asset < 3; ++asset) {
            fixture.factor.dates.push_back(dates[position]);
            fixture.factor.assets.emplace_back(assets[asset]);
            fixture.factor.values.push_back(
                static_cast<double>(asset + 1) +
                static_cast<double>(position) * 0.01);
        }
    }
    for (int asset = 0; asset < 3; ++asset) {
        std::vector<double> values;
        values.reserve(dates.size());
        for (std::size_t position = 0; position < dates.size(); ++position) {
            values.push_back(100.0 + 10.0 * asset +
                             static_cast<double>(position) * (asset + 1));
        }
        fixture.prices.load_column<double>(
            assets[asset], std::move(values),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return fixture;
}

fincore::research_spec grid_spec(const fincore::timestamp start,
                                 const fincore::timestamp end,
                                 std::vector<int> periods) {
    auto config = spec();
    config.targets = {"A", "B", "C"};
    config.start = start;
    config.end = end;
    config.periods = std::move(periods);
    config.quantiles = 3;
    config.max_loss = 1.0;
    config.preprocess = "none";
    config.stability_lags = {1};
    return config;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::stringstream contents;
    contents << stream.rdbuf();
    return contents.str();
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
    const auto repeated_directory = directory.string() + "-repeat";
    const auto sentinel_directory = directory.string() + "-sentinel";
    std::filesystem::remove_all(directory);
    std::filesystem::remove_all(repeated_directory);
    std::filesystem::remove_all(sentinel_directory);

    const auto exported = fincore::export_factor_research(
        result, directory.string());
    EXPECT_EQ(exported.directory, directory.string());
    EXPECT_EQ(exported.files.size(), 11U);
    for (const auto& name : exported.files) {
        EXPECT_TRUE(std::filesystem::exists(directory / name));
    }
    {
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
                      "date,asset,factor,direction,value,processed_value,"
                      "preprocess,group\n",
                      0),
                  0U);
        std::ifstream turnover(directory / "turnover.csv");
        std::stringstream turnover_text;
        turnover_text << turnover.rdbuf();
        EXPECT_NE(turnover_text.str().find("1,"), std::string::npos);
    }

    const auto repeated = fincore::export_factor_research(
        result, repeated_directory);
    EXPECT_EQ(repeated.files, exported.files);
    for (const auto& name : exported.files) {
        EXPECT_EQ(read_text_file(directory / name),
                  read_text_file(std::filesystem::path{repeated_directory} /
                                 name));
    }
    EXPECT_NE(read_text_file(directory / "manifest.json").find(
                  "\"status\": \"OK\""),
              std::string::npos);

    auto optional_summaries = result;
    optional_summaries.information_coefficient_summary = {};
    optional_summaries.spread.clear();
    optional_summaries.coverage.clear();
    const auto fallback_directory = directory.string() + "-fallback";
    static_cast<void>(fincore::export_factor_research(
        optional_summaries, fallback_directory));
    EXPECT_EQ(read_text_file(std::filesystem::path{fallback_directory} /
                             "ic_summary.csv"),
              "label,mean_ic\n1D,\n");
    EXPECT_EQ(read_text_file(std::filesystem::path{fallback_directory} /
                             "spreads.csv"),
              "label,spread\n1D,\n");
    EXPECT_EQ(read_text_file(std::filesystem::path{fallback_directory} /
                             "coverage.csv"),
              "label,count\n1D,\n");
    std::filesystem::remove_all(fallback_directory);

    ASSERT_TRUE(std::filesystem::create_directory(sentinel_directory));
    const auto sentinel = std::filesystem::path{sentinel_directory} /
                          "keep.txt";
    {
        std::ofstream stream(sentinel, std::ios::binary);
        stream << "preserve me\n";
    }
    EXPECT_THROW(static_cast<void>(fincore::export_factor_research(
                     result, sentinel_directory)),
                 fincore::factor_data_error);
    EXPECT_EQ(read_text_file(sentinel), "preserve me\n");
    EXPECT_FALSE(std::filesystem::exists(
        std::filesystem::path{sentinel_directory} / "ic.csv"));

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
    std::filesystem::remove_all(repeated_directory);
    std::filesystem::remove_all(sentinel_directory);
}

TEST(FactorResearch,
     RejectsMalformedIndexedExportShapesBeforeFilesystemWrites) {
    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("fincore-research-invalid-export-" +
                       std::to_string(stamp));
    std::filesystem::remove_all(root);
    const auto date = fincore::utc_timestamp(2022, 1, 3);
    const auto valid = [&] {
        fincore::research_result result;
        result.information_coefficient.labels = {"1D"};
        result.information_coefficient.dates = {date};
        result.information_coefficient.values = {0.1};
        result.quantile_returns.labels = {"1D"};
        return result;
    };

    std::vector<fincore::research_result> invalid_results;
    {
        auto result = valid();
        result.information_coefficient.values.clear();
        invalid_results.push_back(std::move(result));
    }
    {
        auto result = valid();
        result.quantile_returns.quantiles = {1};
        result.quantile_returns.standard_errors = {0.001};
        invalid_results.push_back(std::move(result));
    }
    {
        auto result = valid();
        result.quantile_returns.quantiles = {1};
        result.quantile_returns.means = {0.01};
        invalid_results.push_back(std::move(result));
    }
    {
        auto result = valid();
        result.quantile_returns.labels = {"5D"};
        result.quantile_returns.quantiles = {1};
        result.quantile_returns.means = {0.01};
        result.quantile_returns.standard_errors = {0.001};
        invalid_results.push_back(std::move(result));
    }
    {
        auto result = valid();
        result.turnover.push_back({{date}, {}});
        invalid_results.push_back(std::move(result));
    }
    {
        auto result = valid();
        result.stability.push_back({{date}, {}});
        invalid_results.push_back(std::move(result));
    }

    for (std::size_t index = 0; index < invalid_results.size(); ++index) {
        const auto target = root / std::to_string(index);
        EXPECT_THROW(static_cast<void>(fincore::export_factor_research(
                         invalid_results[index], target.string())),
                     fincore::factor_data_error);
        EXPECT_FALSE(std::filesystem::exists(target));
        EXPECT_FALSE(std::filesystem::exists(root));
    }
    std::filesystem::remove_all(root);
}

TEST(FactorResearch,
     RejectsInvalidTargetAndReservedManifestKeysBeforeFilesystemWrites) {
    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("fincore-research-invalid-target-" +
                       std::to_string(stamp));
    std::filesystem::remove_all(root);

    fincore::research_result valid;
    valid.information_coefficient.labels = {"1D"};
    valid.information_coefficient.dates = {
        fincore::utc_timestamp(2022, 1, 3)};
    valid.information_coefficient.values = {0.1};
    valid.quantile_returns.labels = {"1D"};

    std::string nul_path = (root / "artifact").string();
    nul_path.push_back('\0');
    nul_path += "suffix";
    EXPECT_THROW(static_cast<void>(
                     fincore::export_factor_research(valid, nul_path)),
                 fincore::factor_data_error);
    EXPECT_FALSE(std::filesystem::exists(root));

    for (const std::string reserved_key : {"status", "exports"}) {
        auto result = valid;
        result.manifest[reserved_key] = "caller value";
        EXPECT_THROW(static_cast<void>(fincore::export_factor_research(
                         result, (root / reserved_key).string())),
                     fincore::factor_data_error);
        EXPECT_FALSE(std::filesystem::exists(root));
    }
    std::filesystem::remove_all(root);
}

TEST(FactorResearch, RemovesOwnedDirectoryAfterInjectedMidwriteFailure) {
    fincore::research_result result;
    result.status = "OK";
    result.information_coefficient.labels = {"1D"};
    result.information_coefficient.dates = {
        fincore::utc_timestamp(2022, 1, 3)};
    result.information_coefficient.values = {0.1};
    result.quantile_returns.labels = {"1D"};

    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    const auto directory = std::filesystem::temp_directory_path() /
                          ("fincore-research-injected-failure-" +
                           std::to_string(stamp));
    std::filesystem::remove_all(directory);
    std::size_t writes = 0U;
    const fincore::research_export_detail::table_writer writer =
        [&](const std::filesystem::path& path,
            const std::vector<std::string>&,
            const fincore::research_export_detail::table_generator&) {
            if (writes == 0U) {
                std::ofstream stream(path, std::ios::binary);
                stream << "first table completed\n";
                stream.flush();
                if (!stream) {
                    throw std::runtime_error{"injected test setup write failed"};
                }
                stream.close();
                if (!stream) {
                    throw std::runtime_error{"injected test setup close failed"};
                }
                ++writes;
                return;
            }
            EXPECT_TRUE(std::filesystem::exists(directory / "ic.csv"));
            ++writes;
            throw fincore::factor_data_error{"injected table write failure"};
        };

    EXPECT_THROW(static_cast<void>(
                     fincore::research_export_detail::
                         export_factor_research_with_writer(
                             result, directory.string(), false, writer)),
                 fincore::factor_data_error);
    EXPECT_EQ(writes, 2U);
    EXPECT_FALSE(std::filesystem::exists(directory));
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

TEST(FactorResearch, DerivesIntradayLabelsFromTheStudyGrid) {
    constexpr fincore::timestamp half_hour = 1'800'000'000'000LL;
    const auto first_price = fincore::utc_timestamp(2022, 1, 3, 9, 30);
    std::vector<fincore::timestamp> dates;
    for (int row = 0; row < 5; ++row) {
        dates.push_back(first_price + row * half_hour);
    }
    const auto start = dates[1];
    const auto end = dates[3];
    const auto fixture = make_grid_fixture(dates, start, end);
    auto config = grid_spec(start, end, {1, 2});

    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        config.label = label;
        const auto result = fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config);
        ASSERT_EQ(result.information_coefficient.labels.size(), 2U);
        EXPECT_EQ(result.information_coefficient.labels[0], "30m");
        EXPECT_EQ(result.information_coefficient.labels[1], "1h");
    }
}

TEST(FactorResearch, DerivesIntradayLabelsWithUniverseForBothPriceModes) {
    constexpr fincore::timestamp half_hour = 1'800'000'000'000LL;
    const auto first_price = fincore::utc_timestamp(2022, 1, 3, 9, 30);
    std::vector<fincore::timestamp> dates;
    for (int row = 0; row < 5; ++row) {
        dates.push_back(first_price + row * half_hour);
    }
    const auto start = dates[1];
    const auto end = dates[3];
    const auto fixture = make_grid_fixture(dates, start, end);
    fincore::dataframe universe;
    universe.load_index(std::vector<fincore::timestamp>{dates});
    for (const std::string asset : {"A", "B", "C"}) {
        universe.load_column<bool>(
            asset.c_str(), std::vector<bool>(dates.size(), true),
            hmdf::nan_policy::dont_pad_with_nans);
    }

    auto config = grid_spec(start, end, {1, 2});
    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        config.label = label;
        const auto result = fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config, universe);
        ASSERT_EQ(result.information_coefficient.labels.size(), 2U);
        EXPECT_EQ(result.information_coefficient.labels[0], "30m");
        EXPECT_EQ(result.information_coefficient.labels[1], "1h");
        ASSERT_EQ(result.manifest.count("universe"), 1U);
        EXPECT_EQ(result.manifest.at("universe"), "applied");
    }

    auto unsupported = config;
    unsupported.label = "unknown_basis";
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     fixture.factor, fixture.prices, unsupported, universe)),
                 fincore::factor_data_error);
    auto precomputed = config;
    precomputed.label = "precomputed";
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research(
                     fixture.factor, fixture.prices, precomputed, universe)),
                 fincore::factor_data_error);
}

TEST(FactorResearch, UsesTheFirstStudyDateAfterPriceBufferRows) {
    const std::vector<fincore::timestamp> dates{
        fincore::utc_timestamp(2022, 1, 3, 8, 30),
        fincore::utc_timestamp(2022, 1, 3, 9, 0),
        fincore::utc_timestamp(2022, 1, 3, 9, 30),
        fincore::utc_timestamp(2022, 1, 3, 10, 30),
        fincore::utc_timestamp(2022, 1, 3, 11, 30)};
    const auto start = dates[2];
    const auto end = dates[3];
    const auto fixture = make_grid_fixture(dates, start, end);
    auto config = grid_spec(start, end, {1});

    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        config.label = label;
        const auto result = fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config);
        ASSERT_EQ(result.information_coefficient.labels.size(), 1U);
        EXPECT_EQ(result.information_coefficient.labels[0], "1h");
    }
}

TEST(FactorResearch, NormalizesWeekendAndHolidayGapsOnTheInferredCalendar) {
    std::vector<fincore::timestamp> weekend_dates;
    for (unsigned day = 3; day <= 7; ++day) {
        weekend_dates.push_back(fincore::utc_timestamp(2022, 1, day, 9, 30));
    }
    weekend_dates.push_back(fincore::utc_timestamp(2022, 1, 10, 9, 30));
    weekend_dates.push_back(fincore::utc_timestamp(2022, 1, 11, 9, 30));
    const auto friday = weekend_dates[4];
    const auto monday = weekend_dates[5];
    const auto weekend_fixture =
        make_grid_fixture(weekend_dates, friday, monday);

    std::vector<fincore::timestamp> holiday_dates;
    for (unsigned day = 3; day <= 7; ++day) {
        holiday_dates.push_back(
            day == 7 ? fincore::utc_timestamp(2022, 1, day, 15, 30)
                     : fincore::utc_timestamp(2022, 1, day, 9, 30));
    }
    holiday_dates.push_back(fincore::utc_timestamp(2022, 1, 11, 9, 30));
    holiday_dates.push_back(fincore::utc_timestamp(2022, 1, 12, 9, 30));
    const auto holiday_start = holiday_dates[4];
    const auto holiday_end = holiday_dates[5];
    const auto holiday_fixture =
        make_grid_fixture(holiday_dates, holiday_start, holiday_end);

    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        auto weekend_config = grid_spec(friday, monday, {1});
        weekend_config.label = label;
        const auto weekend_result = fincore::evaluate_factor_research(
            weekend_fixture.factor, weekend_fixture.prices, weekend_config);
        ASSERT_EQ(weekend_result.information_coefficient.labels.size(), 1U);
        EXPECT_EQ(weekend_result.information_coefficient.labels[0], "1D");

        auto holiday_config = grid_spec(holiday_start, holiday_end, {1});
        holiday_config.label = label;
        const auto holiday_result = fincore::evaluate_factor_research(
            holiday_fixture.factor, holiday_fixture.prices, holiday_config);
        ASSERT_EQ(holiday_result.information_coefficient.labels.size(), 1U);
        EXPECT_EQ(holiday_result.information_coefficient.labels[0], "1D18h");
    }
}

TEST(FactorResearch, FormatsSubsecondAndCalendarDurationComponents) {
    constexpr fincore::timestamp second = 1'000'000'000LL;
    const auto start = fincore::utc_timestamp(2022, 1, 3, 9, 30);
    const std::vector<fincore::timestamp> offsets{
        0LL, 1LL, 1'000LL, 1'000'000LL, second, 60LL * second,
        3'600LL * second, 86'400LL * second, 86'400LL * second + second};
    std::vector<fincore::timestamp> dates;
    for (const auto offset : offsets) {
        dates.push_back(start + offset);
    }
    const auto fixture = make_grid_fixture(dates, start, start);
    auto config = grid_spec(start, start, {1, 2, 3, 4, 5, 6, 7});
    const std::vector<std::string> expected{
        "1ns", "1us", "1ms", "1s", "1m", "1h", "1D"};

    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        config.label = label;
        const auto result = fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config);
        EXPECT_EQ(result.information_coefficient.labels, expected);
    }
}

TEST(FactorResearch, RejectsDuplicateCanonicalLabelsForBothPriceLabelModes) {
    const auto start = fincore::utc_timestamp(2022, 1, 3, 9, 30);
    const std::vector<fincore::timestamp> dates{
        start, start + 86'400'000'000'000LL,
        start + 2 * 86'400'000'000'000LL};
    const auto fixture = make_grid_fixture(dates, dates.front(), dates.front());
    auto config = grid_spec(dates.front(), dates.front(), {1, 1});

    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        config.label = label;
        try {
            static_cast<void>(fincore::evaluate_factor_research(
                fixture.factor, fixture.prices, config));
            FAIL() << "expected duplicate canonical-label rejection";
        } catch (const fincore::factor_data_error& error) {
            EXPECT_STREQ(
                error.what(),
                "requested periods map to duplicate forward-return labels");
        }
    }
}

TEST(FactorResearch, FormatsIntervalsAcrossTheFullTimestampRangeWithoutOverflow) {
    constexpr auto minimum = std::numeric_limits<fincore::timestamp>::min();
    constexpr auto maximum = std::numeric_limits<fincore::timestamp>::max();
    const std::vector<fincore::timestamp> dates{
        minimum, minimum + 1, maximum - 1, maximum};
    const auto fixture = make_grid_fixture(dates, minimum, minimum);
    auto config = grid_spec(minimum, minimum, {2});

    for (const std::string label : {"close_to_close", "next_open_to_open"}) {
        config.label = label;
        const auto result = fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config);
        ASSERT_EQ(result.information_coefficient.labels.size(), 1U);
        EXPECT_EQ(result.information_coefficient.labels[0],
                  "1D23h34m33s709ms551us614ns");
    }
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
    EXPECT_EQ(next_open.information_coefficient.labels[0], "1D");
    EXPECT_EQ(next_open.coverage[0], 15U);
    for (const double value : next_open.information_coefficient.values) {
        EXPECT_NEAR(value, 1.0, 1e-12);
    }

    config.label = "close_to_close";
    const auto close =
        fincore::evaluate_factor_research(factor, prices, config);
    ASSERT_EQ(close.information_coefficient.labels.size(), 1U);
    EXPECT_EQ(close.information_coefficient.labels[0], "1D");
    EXPECT_EQ(close.coverage[0], 15U);
}

std::string capture_factor_error(const std::function<void()>& action) {
    try {
        action();
    } catch (const fincore::factor_data_error& error) {
        return error.what();
    }
    return {};
}

TEST(FactorResearch, RejectsDuplicateCellsBeforeTargetMembership) {
    auto fixture = make_fixture();
    auto duplicated = fixture.factor;
    duplicated.dates.insert(duplicated.dates.begin() + 1,
                            duplicated.dates.front());
    duplicated.assets.insert(duplicated.assets.begin() + 1,
                             duplicated.assets.front());
    duplicated.values.insert(duplicated.values.begin() + 1, 1.0);
    auto config = spec();
    config.targets = {"MISSING"};
    const auto message = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research(
            duplicated, fixture.prices, config));
    });
    EXPECT_NE(message.find("duplicate (date, asset) keys"),
              std::string::npos);
    EXPECT_EQ(message.find("missing requested target"), std::string::npos);
}

TEST(FactorResearch, UnsortedUniquePanelPreservesSourceOrderSubsequence) {
    const auto start = fincore::utc_timestamp(2024, 6, 3);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    fincore::factor_series factors;
    factors.dates = {start + 2 * day, start + day, start,
                     start + 2 * day, start + day, start};
    factors.assets = {"B", "A", "A", "A", "B", "B"};
    factors.values = {20.0, 10.0, 0.0, 30.0, 40.0, 1.0};

    fincore::research_spec config;
    config.targets = {"A", "B"};
    config.start = start + day;
    config.end = start + 2 * day;
    config.preprocess = "none";
    const auto panel =
        fincore::detail::research_panel_for_benchmark(factors, config);

    ASSERT_EQ(panel.dates.size(), 4U);
    EXPECT_EQ(panel.dates[0], start + 2 * day);
    EXPECT_EQ(panel.assets[0], "B");
    EXPECT_EQ(panel.values[0], 20.0);
    EXPECT_EQ(panel.dates[1], start + day);
    EXPECT_EQ(panel.assets[1], "A");
    EXPECT_EQ(panel.values[1], 10.0);
    EXPECT_EQ(panel.dates[2], start + 2 * day);
    EXPECT_EQ(panel.assets[2], "A");
    EXPECT_EQ(panel.values[2], 30.0);
    EXPECT_EQ(panel.dates[3], start + day);
    EXPECT_EQ(panel.assets[3], "B");
    EXPECT_EQ(panel.values[3], 40.0);

    fincore::detail::factor_compute_diagnostics_scope diagnostics_scope;
    const auto source_rows =
        fincore::research_detailed_detail::select_panel_source_rows(
            factors, config);
    EXPECT_EQ(diagnostics_scope.snapshot().panel_preprocessings, 1U);
    ASSERT_EQ(source_rows.size(), 4U);
    EXPECT_EQ(source_rows[0], 0U);
    EXPECT_EQ(source_rows[1], 1U);
    EXPECT_EQ(source_rows[2], 3U);
    EXPECT_EQ(source_rows[3], 4U);
}

TEST(FactorResearch, RejectsNonAdjacentDuplicateOutsideWindowFirst) {
    const auto start = fincore::utc_timestamp(2024, 6, 10);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    fincore::factor_series factors;
    factors.dates = {start - 2 * day, start - day, start - 2 * day};
    factors.assets = {"A", "B", "A"};
    factors.values = {1.0, 2.0, 3.0};

    fincore::research_spec config;
    config.targets = {"MISSING"};
    config.start = start;
    config.end = start + day;
    const auto message = capture_factor_error([&] {
        static_cast<void>(fincore::detail::research_panel_for_benchmark(
            factors, config));
    });
    EXPECT_EQ(message,
              "factor panel contains duplicate (date, asset) keys");
}

TEST(FactorResearch, ReportsFullPanelMissingTargetsInRequestOrder) {
    auto fixture = make_fixture();
    auto config = spec();
    config.targets = {"MISSING_Z", "A", "MISSING_A"};
    const auto message = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config));
    });
    EXPECT_EQ(message,
              "factor panel is missing requested target assets: "
              "MISSING_Z, MISSING_A");
}

TEST(FactorResearch, RejectsTargetsAbsentFromWindowButPresentOutside) {
    auto fixture = make_fixture();
    auto factor = fixture.factor;
    const auto day = 86'400'000'000'000LL;
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto end = fincore::utc_timestamp(2021, 3, 4);
    factor.dates.push_back(start - day);
    factor.assets.push_back("Z");
    factor.values.push_back(1.0);
    factor.dates.push_back(end + day);
    factor.assets.push_back("Z");
    factor.values.push_back(2.0);
    auto config = spec();
    config.targets = {"A", "B", "C", "D", "E", "Z"};
    const auto message = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research(
            factor, fixture.prices, config));
    });
    EXPECT_EQ(message,
              "research window is missing requested target assets: Z");
}

TEST(FactorResearch, EmptyWindowPrecedesInWindowTargetChecks) {
    auto fixture = make_fixture();
    auto config = spec();
    const auto day = 86'400'000'000'000LL;
    config.start = fincore::utc_timestamp(2021, 3, 4) + 10 * day;
    config.end = config.start + 10 * day;
    config.targets = {"A"};
    const auto message = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config));
    });
    EXPECT_EQ(message,
              "factors contain no cells inside the requested window");

    config.targets = {"MISSING"};
    const auto missing = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config));
    });
    EXPECT_EQ(missing,
              "factor panel is missing requested target assets: MISSING");
}

TEST(FactorResearch, EmptyTargetsSelectAllObservedAssets) {
    auto fixture = make_fixture();
    auto config = spec();
    config.targets.clear();
    const auto result = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, config);
    EXPECT_EQ(result.status, "OK");
    EXPECT_EQ(result.signals.size(), 300U);
    ASSERT_FALSE(result.coverage.empty());
    EXPECT_EQ(result.coverage.front(), 300U);
}

TEST(FactorResearch, CorrelationsConsumeProcessedOriginalPanels) {
    auto fixture = make_fixture();
    auto transformed = fixture.factor;
    for (double& value : transformed.values) {
        value = value * value * value;
    }
    std::vector<fincore::named_factor_panel> panels{
        {"F", fixture.factor}, {"H", transformed}};
    auto raw = spec();
    raw.preprocess = "none";
    raw.periods = {1};
    raw.stability_lags = {1};
    const auto raw_result =
        fincore::evaluate_factor_research_panel(panels, fixture.prices, raw);
    ASSERT_EQ(raw_result.correlations.size(), 1U);
    EXPECT_NEAR(raw_result.correlations[0].mean_correlation,
                0.9253973936793224, 1e-12);
    EXPECT_EQ(raw_result.correlations[0].n_dates, 60U);

    auto ranked = raw;
    ranked.preprocess = "cross_section_rank";
    const auto ranked_result =
        fincore::evaluate_factor_research_panel(panels, fixture.prices, ranked);
    ASSERT_EQ(ranked_result.correlations.size(), 1U);
    EXPECT_NEAR(ranked_result.correlations[0].mean_correlation, 1.0, 1e-12);
}

TEST(FactorResearch, PrecomputedAndMultiFactorEntriesUseTargetGuards) {
    auto fixture = make_fixture();
    fincore::forward_return_table table;
    table.assets = {"A", "B", "C", "D", "E"};
    table.labels = {"1D"};
    for (const auto date : fixture.factor.dates) {
        if (table.dates.empty() || table.dates.back() != date) {
            table.dates.push_back(date);
        }
    }
    table.values = {std::vector<double>(
        table.dates.size() * table.assets.size(), 0.01)};
    auto config = spec();
    config.targets = {"MISSING"};
    const auto precomputed_message = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, table, config));
    });
    EXPECT_EQ(precomputed_message,
              "factor panel is missing requested target assets: MISSING");

    std::vector<fincore::named_factor_panel> panels{
        {"F", fixture.factor}, {"G", fixture.factor}};
    const auto panel_message = capture_factor_error([&] {
        static_cast<void>(fincore::evaluate_factor_research_panel(
            panels, fixture.prices, config));
    });
    EXPECT_EQ(panel_message,
              "factor panel is missing requested target assets: MISSING");
}

TEST(FactorResearch, DirectionOverridesReachCorrelationPanels) {
    auto fixture = make_fixture();
    auto inverse = fixture.factor;
    for (double& value : inverse.values) {
        value = -value;
    }
    std::vector<fincore::named_factor_panel> panels{
        {"F", fixture.factor}, {"G", inverse}};
    auto config = spec();
    config.preprocess = "cross_section_rank";
    config.periods = {1};
    config.stability_lags = {1};
    const auto result =
        fincore::evaluate_factor_research_panel(panels, fixture.prices, config);
    ASSERT_EQ(result.correlations.size(), 1U);
    EXPECT_NEAR(result.correlations[0].mean_correlation, -1.0, 1e-12);
}

TEST(FactorResearch, ReusesPanelsAndInformationCoefficients) {
    auto fixture = make_fixture();
    auto config = spec();
    config.periods = {1, 5};
    fincore::detail::reset_factor_compute_diagnostics();
    const auto single = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, config);
    const auto single_counts =
        fincore::detail::get_factor_compute_diagnostics();
    EXPECT_EQ(single.status, "OK");
    EXPECT_EQ(single_counts.panel_preprocessings, 1U);
    EXPECT_EQ(single_counts.information_coefficient_kernel_calls, 2U);

    std::vector<fincore::named_factor_panel> panels{
        {"F", fixture.factor}, {"G", fixture.factor}};
    fincore::detail::reset_factor_compute_diagnostics();
    const auto multi =
        fincore::evaluate_factor_research_panel(panels, fixture.prices, config);
    const auto multi_counts =
        fincore::detail::get_factor_compute_diagnostics();
    EXPECT_EQ(multi.status, "OK");
    EXPECT_EQ(multi_counts.panel_preprocessings, 2U);
    EXPECT_EQ(multi_counts.information_coefficient_kernel_calls, 4U);

    const auto diagnostics_before =
        fincore::detail::get_factor_compute_diagnostics();
    {
        fincore::detail::factor_compute_diagnostics_scope scope;
        static_cast<void>(fincore::evaluate_factor_research(
            fixture.factor, fixture.prices, config));
        EXPECT_EQ(scope.snapshot().panel_preprocessings, 1U);
        EXPECT_EQ(scope.snapshot().information_coefficient_kernel_calls, 2U);
    }
    const auto diagnostics_after =
        fincore::detail::get_factor_compute_diagnostics();
    EXPECT_EQ(diagnostics_after.panel_preprocessings,
              diagnostics_before.panel_preprocessings);
    EXPECT_EQ(diagnostics_after.information_coefficient_kernel_calls,
              diagnostics_before.information_coefficient_kernel_calls);
}


TEST(FactorResearch, DiagnosticsAreIsolatedAcrossConcurrentCalls) {
    auto fixture = make_fixture();
    auto config = spec();
    config.periods = {1};
    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for (int worker = 0; worker < 4; ++worker) {
        threads.emplace_back([&] {
            fincore::detail::factor_compute_diagnostics_scope scope;
            static_cast<void>(fincore::evaluate_factor_research(
                fixture.factor, fixture.prices, config));
            const auto counts = scope.snapshot();
            if (counts.panel_preprocessings != 1U ||
                counts.information_coefficient_kernel_calls != 1U) {
                ++mismatches;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(mismatches.load(), 0);
}

}  // namespace

TEST(FactorResearch, StreamingExportMatchesCollectingWriterBytes) {
    auto fixture = make_fixture();
    const auto result = fincore::evaluate_factor_research(
        fixture.factor, fixture.prices, spec());
    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch()
                           .count();
    const auto root = std::filesystem::temp_directory_path() /
                      ("fincore-research-export-equality-" +
                       std::to_string(stamp));
    const auto streaming = root / "streaming";
    const auto collecting = root / "collecting";
    std::filesystem::remove_all(root);

    const auto streamed =
        fincore::export_factor_research(result, streaming.string());
    const fincore::research_export_detail::table_writer collector =
        [](const std::filesystem::path& path,
           const std::vector<std::string>& header,
           const fincore::research_export_detail::table_generator& generate) {
            std::vector<std::vector<std::string>> rows;
            generate([&rows](const std::vector<std::string>& row) {
                rows.push_back(row);
            });
            fincore::research_export_detail::write_table_rows(path, header,
                                                              rows);
        };
    const auto collected =
        fincore::research_export_detail::export_factor_research_with_writer(
            result, collecting.string(), false, collector);
    EXPECT_EQ(streamed.files, collected.files);

    const auto read_bytes = [](const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>());
    };
    std::size_t compared = 0U;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(streaming)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto relative = std::filesystem::relative(entry.path(), streaming);
        EXPECT_TRUE(std::filesystem::exists(collecting / relative));
        EXPECT_EQ(read_bytes(entry.path()),
                  read_bytes(collecting / relative))
            << relative.string();
        ++compared;
    }
    EXPECT_EQ(compared, streamed.files.size());
    std::filesystem::remove_all(root);
}
