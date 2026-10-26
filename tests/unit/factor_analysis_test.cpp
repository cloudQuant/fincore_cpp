#include <fincore/calendar.hpp>
#include <fincore/detail/digest.hpp>
#include <fincore/detail/factor_compute.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/report.hpp>
#include <fincore/factor/tears.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

fincore::prepared_factor_data prepared_fixture() {
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C", "D", "E"};
    fincore::factor_series factor;
    for (int date = 0; date < 60; ++date) {
        for (int asset = 0; asset < 5; ++asset) {
            factor.dates.push_back(start + date * day);
            factor.assets.push_back(assets[asset]);
            factor.values.push_back(
                0.3 * std::sin((date + 1) * 0.17 + asset * 0.9) +
                0.1 * std::cos((date + 1) * 0.05 + asset) +
                0.03 * asset);
        }
    }
    // The price index runs ten stamps past the factor panel, so no factor date
    // loses its five-day label at the end of the series.
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 70; ++position) {
        index.push_back(start + position * day);
    }
    fincore::dataframe prices;
    prices.load_index(std::move(index));
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
        prices.load_column<double>(assets[asset], std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    fincore::prepare_factor_config config;
    config.periods = {1, 5};
    config.quantiles = 3;
    config.max_loss = 0.5;
    return fincore::prepare_factor_data(factor, prices, config);
}

TEST(FactorAnalysis, AssemblesComputeOnceModel) {
    const auto prepared = prepared_fixture();
    const auto model = fincore::analyze_factor(prepared);
    ASSERT_EQ(model.forward_periods.size(), 2U);
    EXPECT_EQ(model.forward_periods[0], "1D");
    EXPECT_EQ(model.forward_periods[1], "5D");
    EXPECT_EQ(model.factor_data.rows.size(), prepared.rows.size());
    ASSERT_EQ(model.information_coefficient.dates.size(), 60U);
    EXPECT_NEAR(model.information_coefficient.values[0],
                0.9999999999999999, 1e-12);
    EXPECT_NEAR(model.mean_information_coefficient.means[0],
                -0.17833333333333332, 1e-12);
    ASSERT_EQ(model.factor_returns.dates.size(), 60U);
    EXPECT_NEAR(model.factor_returns.values[0],
                0.003392085591589239, 1e-12);
    ASSERT_EQ(model.alpha_beta.size(), 2U);
    EXPECT_NEAR(model.alpha_beta[0].beta, 0.16261645466483737, 1e-12);
    ASSERT_EQ(model.mean_returns_by_quantile.quantiles.size(), 3U);
    EXPECT_NEAR(model.mean_returns_by_quantile.means[0],
                0.00201525411346395, 1e-12);
    ASSERT_EQ(model.quantile_turnover.size(), 3U);
    EXPECT_TRUE(std::isnan(model.quantile_turnover[0].values.values[0]));
    ASSERT_EQ(model.rank_autocorrelation[0].values.dates.size(), 60U);
    EXPECT_NEAR(model.rank_autocorrelation[0].values.values[1],
                0.9999999999999998, 1e-12);
    EXPECT_EQ(model.factor_weights.size(), prepared.rows.size());
    EXPECT_EQ(model.result_fingerprint.size(), 64U);

    const auto repeated = fincore::analyze_factor(prepared);
    EXPECT_EQ(model.result_fingerprint, repeated.result_fingerprint);

    fincore::factor_analysis_config config;
    config.long_short = false;
    const auto long_only = fincore::analyze_factor(prepared, config);
    EXPECT_NE(long_only.result_fingerprint, model.result_fingerprint);
    const auto direct = fincore::factor_returns(prepared, false);
    EXPECT_NEAR(long_only.factor_returns.values[0], direct.values[0],
                1e-15);
    EXPECT_NEAR(model.factor_returns.values[0],
                fincore::factor_returns(prepared).values[0], 1e-15);
}

TEST(FactorAnalysis, RejectsEmptyPreparedData) {
    fincore::prepared_factor_data empty;
    EXPECT_THROW(static_cast<void>(fincore::analyze_factor(empty)),
                 fincore::factor_data_error);
}

}  // namespace
namespace {

fincore::prepared_factor_data oracle_prepared() {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D", "5D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C", "D"};
    for (int date = 0; date < 3; ++date) {
        for (int asset = 0; asset < 4; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset / 2 + 1;
            row.forward_returns = {0.01 * (asset + 1) + 0.001 * date,
                                   0.05 * (asset + 1) + 0.002 * date};
            data.rows.push_back(row);
        }
    }
    return data;
}

std::filesystem::path group_snapshot_fixture_path(const std::string& name) {
    const std::filesystem::path source_file{__FILE__};
    auto path = source_file.parent_path().parent_path() / "fixtures" /
                "group_snapshots" / name;
    if (!std::filesystem::exists(path)) {
        path = std::filesystem::current_path() / "tests" / "fixtures" /
               "group_snapshots" / name;
    }
    return path;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::stringstream stream{line};
    std::vector<std::string> fields;
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

fincore::timestamp parse_fixture_date(const std::string& value) {
    return fincore::utc_timestamp(std::stoi(value.substr(0, 4)),
                                  static_cast<unsigned>(
                                      std::stoi(value.substr(5, 2))),
                                  static_cast<unsigned>(
                                      std::stoi(value.substr(8, 2))));
}

fincore::prepared_factor_data group_snapshot_fixture() {
    std::ifstream input{group_snapshot_fixture_path("unequal_groups.csv")};
    if (!input) {
        throw std::runtime_error{"unable to open unequal group fixture"};
    }
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D", "5D"};
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        const auto fields = split_csv_line(line);
        if (fields.size() != 7U) {
            throw std::runtime_error{"invalid unequal group fixture row"};
        }
        fincore::factor_row row;
        row.date = parse_fixture_date(fields[0]);
        row.asset = fields[1];
        row.group = fields[2];
        row.has_group = true;
        row.quantile = std::stoi(fields[3]);
        row.factor = std::stod(fields[4]);
        row.forward_returns = {std::stod(fields[5]), std::stod(fields[6])};
        data.rows.push_back(std::move(row));
    }
    return data;
}

struct group_oracle_row {
    std::string view;
    std::string mode;
    std::string date;
    std::string group;
    int quantile = -1;
    std::string label;
    double value = 0.0;
    double standard_error = 0.0;
};

std::vector<group_oracle_row> group_snapshot_oracle() {
    std::ifstream input{group_snapshot_fixture_path("python_oracle.csv")};
    if (!input) {
        throw std::runtime_error{"unable to open Python group oracle fixture"};
    }
    std::vector<group_oracle_row> rows;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        const auto fields = split_csv_line(line);
        if (fields.size() != 8U) {
            throw std::runtime_error{"invalid Python group oracle row"};
        }
        group_oracle_row row;
        row.view = fields[0];
        row.mode = fields[1];
        row.date = fields[2];
        row.group = fields[3];
        if (!fields[4].empty()) {
            row.quantile = std::stoi(fields[4]);
        }
        row.label = fields[5];
        row.value = std::stod(fields[6]);
        row.standard_error = std::stod(fields[7]);
        rows.push_back(std::move(row));
    }
    return rows;
}

void expect_oracle_value(const double actual, const double expected) {
    if (std::isnan(expected)) {
        EXPECT_TRUE(std::isnan(actual));
        return;
    }
    ASSERT_TRUE(std::isfinite(actual));
    EXPECT_LE(std::abs(actual - expected),
              1e-12 + 1e-12 * std::abs(expected));
}

void expect_same_values(const std::vector<double>& actual,
                        const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        if (std::isnan(expected[index])) {
            EXPECT_TRUE(std::isnan(actual[index]));
        } else {
            EXPECT_DOUBLE_EQ(actual[index], expected[index]);
        }
    }
}

void expect_same_weights(
    const std::vector<fincore::factor_weight_row>& actual,
    const std::vector<fincore::factor_weight_row>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index].date, expected[index].date);
        EXPECT_EQ(actual[index].asset, expected[index].asset);
        if (std::isnan(expected[index].weight)) {
            EXPECT_TRUE(std::isnan(actual[index].weight));
        } else {
            EXPECT_DOUBLE_EQ(actual[index].weight, expected[index].weight);
        }
    }
}

void expect_same_returns(const fincore::factor_return_table& actual,
                         const fincore::factor_return_table& expected) {
    EXPECT_EQ(actual.dates, expected.dates);
    EXPECT_EQ(actual.labels, expected.labels);
    expect_same_values(actual.values, expected.values);
}

void expect_same_ic(const fincore::factor_ic_table& actual,
                    const fincore::factor_ic_table& expected) {
    EXPECT_EQ(actual.dates, expected.dates);
    EXPECT_EQ(actual.labels, expected.labels);
    EXPECT_EQ(actual.groups, expected.groups);
    expect_same_values(actual.values, expected.values);
}

void expect_same_series(const fincore::factor_series_result& actual,
                        const fincore::factor_series_result& expected) {
    EXPECT_EQ(actual.dates, expected.dates);
    expect_same_values(actual.values, expected.values);
}

using group_oracle_key =
    std::tuple<std::string, fincore::timestamp, std::string, int,
               std::string>;
using group_oracle_values = std::pair<double, double>;

std::map<group_oracle_key, group_oracle_values> snapshot_values(
    const fincore::factor_group_snapshot& snapshot) {
    std::map<group_oracle_key, group_oracle_values> values;
    const auto& returns = snapshot.mean_returns_by_quantile;
    for (std::size_t row = 0; row < returns.quantiles.size(); ++row) {
        for (std::size_t label = 0; label < returns.labels.size(); ++label) {
            const std::size_t position = row * returns.labels.size() + label;
            values.emplace(
                group_oracle_key{"quantile_mean", 0, snapshot.group,
                                 returns.quantiles[row], returns.labels[label]},
                group_oracle_values{returns.means[position],
                                    returns.standard_errors[position]});
        }
    }
    const auto& date_returns = snapshot.mean_returns_by_date;
    for (std::size_t row = 0; row < date_returns.dates.size(); ++row) {
        const std::size_t date_position =
            row * date_returns.quantiles.size() * date_returns.labels.size();
        for (std::size_t quantile = 0;
             quantile < date_returns.quantiles.size(); ++quantile) {
            for (std::size_t label = 0; label < date_returns.labels.size();
                 ++label) {
                const std::size_t position =
                    date_position +
                    quantile * date_returns.labels.size() + label;
                values.emplace(
                    group_oracle_key{"quantile_by_date", date_returns.dates[row], snapshot.group,
                                     date_returns.quantiles[quantile],
                                     date_returns.labels[label]},
                    group_oracle_values{date_returns.means[position],
                                        date_returns.standard_errors[position]});
            }
        }
    }
    for (const auto& row : snapshot.mean_return_spread) {
        values.emplace(group_oracle_key{"spread", 0, snapshot.group, -1,
                                        row.period},
                       group_oracle_values{row.spread, row.standard_error});
    }
    const auto& information = snapshot.information_coefficient;
    for (std::size_t row = 0; row < information.dates.size(); ++row) {
        for (std::size_t label = 0; label < information.labels.size(); ++label) {
            values.emplace(
                group_oracle_key{"ic", information.dates[row], snapshot.group, -1,
                                 information.labels[label]},
                group_oracle_values{information.values[
                                        row * information.labels.size() + label],
                                    std::nan("")});
        }
    }
    const auto& mean_information = snapshot.mean_information_coefficient;
    for (std::size_t label = 0; label < mean_information.labels.size();
         ++label) {
        values.emplace(
            group_oracle_key{"mean_ic", 0, snapshot.group, -1,
                             mean_information.labels[label]},
            group_oracle_values{mean_information.means[label], std::nan("")});
    }
    for (const auto& monthly : snapshot.time_aggregated_information) {
        for (std::size_t row = 0; row < monthly.dates.size(); ++row) {
            for (std::size_t label = 0; label < monthly.labels.size(); ++label) {
                values.emplace(
                    group_oracle_key{"monthly_ic", monthly.dates[row], snapshot.group, -1,
                                     monthly.labels[label]},
                    group_oracle_values{monthly.means[
                                            row * monthly.labels.size() + label],
                                        std::nan("")});
            }
        }
    }
    return values;
}

TEST(FactorAnalysis, BuildsOwnedByGroupSnapshotsFromFullUniverseOracle) {
    auto prepared = group_snapshot_fixture();
    const auto oracle = group_snapshot_oracle();
    for (const bool group_neutral : {false, true}) {
        fincore::factor_analysis_config config;
        config.periods = {"1D", "5D"};
        config.turnover_periods = {1};
        config.time_aggregation = {"M"};
        config.include_portfolio_inputs = false;
        config.by_group = true;
        config.group_neutral = group_neutral;
        const auto model = fincore::analyze_factor(prepared, config);
        const std::string mode = group_neutral ? "group_neutral" : "ordinary";

        ASSERT_EQ(model.group_snapshots.size(), 2U);
        EXPECT_EQ(model.group_snapshots[0].group, "G1");
        EXPECT_EQ(model.group_snapshots[1].group, "G2");
        EXPECT_TRUE(model.information_coefficient.groups.empty());
        EXPECT_TRUE(model.mean_returns_by_quantile.groups.empty());
        EXPECT_TRUE(model.mean_returns_by_date.groups.empty());
        ASSERT_EQ(model.grouped_results.size(), 2U);
        EXPECT_EQ(model.grouped_results[0].group, "G1");

        for (const auto& snapshot : model.group_snapshots) {
            const auto actual = snapshot_values(snapshot);
            std::map<group_oracle_key, group_oracle_values> expected;
            for (const auto& row : oracle) {
                if (row.mode != mode || row.group != snapshot.group ||
                    row.view == "summary_ic") {
                    continue;
                }
                const fincore::timestamp date =
                    row.date.empty() ? 0 : parse_fixture_date(row.date);
                expected.emplace(
                    group_oracle_key{row.view, date, row.group, row.quantile,
                                     row.label},
                    group_oracle_values{row.value, row.standard_error});
            }
            for (const auto& [key, oracle_values] : expected) {
                const auto found = actual.find(key);
                ASSERT_NE(found, actual.end()) << snapshot.group << ": "
                                               << std::get<0>(key);
                expect_oracle_value(found->second.first, oracle_values.first);
                expect_oracle_value(found->second.second,
                                    oracle_values.second);
            }
            for (const auto& [key, values] : actual) {
                if (expected.contains(key)) {
                    continue;
                }
                // Python's by-date table omits absent (quantile, date, group)
                // tuples. The native rectangular cube keeps those cells as NaN.
                ASSERT_EQ(std::get<0>(key), "quantile_by_date")
                    << snapshot.group;
                EXPECT_TRUE(std::isnan(values.first));
                EXPECT_TRUE(std::isnan(values.second));
            }
        }

        auto aggregate_config = config;
        aggregate_config.by_group = false;
        const auto aggregate =
            fincore::analyze_factor(prepared, aggregate_config);
        expect_same_values(model.information_coefficient.values,
                           aggregate.information_coefficient.values);
        expect_same_values(model.mean_returns_by_quantile.means,
                           aggregate.mean_returns_by_quantile.means);
        expect_same_values(model.mean_returns_by_quantile.standard_errors,
                           aggregate.mean_returns_by_quantile.standard_errors);
        expect_same_values(model.mean_returns_by_date.means,
                           aggregate.mean_returns_by_date.means);
        expect_same_values(model.mean_returns_by_date.standard_errors,
                           aggregate.mean_returns_by_date.standard_errors);
        ASSERT_EQ(model.time_aggregated_information.size(), 1U);
        ASSERT_EQ(aggregate.time_aggregated_information.size(), 1U);
        expect_same_values(model.time_aggregated_information[0].means,
                           aggregate.time_aggregated_information[0].means);

        if (group_neutral) {
            ASSERT_TRUE(model.summary_information_coefficient.has_value());
            const auto& summary = *model.summary_information_coefficient;
            EXPECT_TRUE(summary.groups.empty());
            std::map<group_oracle_key, double> expected_summary;
            for (const auto& row : oracle) {
                if (row.mode == mode && row.view == "summary_ic") {
                    expected_summary.emplace(
                        group_oracle_key{"summary_ic",
                                         parse_fixture_date(row.date), "", -1,
                                         row.label},
                        row.value);
                }
            }
            ASSERT_EQ(summary.values.size(), expected_summary.size());
            for (std::size_t date = 0; date < summary.dates.size(); ++date) {
                for (std::size_t label = 0; label < summary.labels.size();
                     ++label) {
                    const auto key = group_oracle_key{
                        "summary_ic", summary.dates[date], "", -1,
                        summary.labels[label]};
                    const auto found = expected_summary.find(key);
                    ASSERT_NE(found, expected_summary.end());
                    expect_oracle_value(
                        summary.values[date * summary.labels.size() + label],
                        found->second);
                }
            }
        } else {
            EXPECT_FALSE(model.summary_information_coefficient.has_value());
            const auto& group_snapshot = model.group_snapshots[0];
            const auto grouped_result = std::find_if(
                model.grouped_results.begin(), model.grouped_results.end(),
                [](const auto& result) { return result.group == "G1"; });
            ASSERT_NE(grouped_result, model.grouped_results.end());
            ASSERT_FALSE(group_snapshot.mean_returns_by_quantile.means.empty());
            ASSERT_FALSE(grouped_result->mean_returns_by_quantile.means.empty());
            EXPECT_GT(std::abs(
                          group_snapshot.mean_returns_by_quantile.means[0] -
                          grouped_result->mean_returns_by_quantile.means[0]),
                      1e-2);

            const double snapshot_value =
                group_snapshot.mean_returns_by_quantile.means[0];
            auto copied_model = model;
            copied_model.mean_returns_by_quantile.means[0] = 123.0;
            copied_model.group_snapshots[0]
                .mean_returns_by_quantile.means[0] = 456.0;
            EXPECT_DOUBLE_EQ(
                model.group_snapshots[0].mean_returns_by_quantile.means[0],
                snapshot_value);
            auto changed_input = prepared;
            changed_input.rows[0].group = "MUTATED";
            changed_input.rows[0].forward_returns[0] = 999.0;
            EXPECT_DOUBLE_EQ(
                model.group_snapshots[0].mean_returns_by_quantile.means[0],
                snapshot_value);
        }
    }
}

TEST(FactorAnalysis, ReusesWeightAndInformationKernelsWithStandaloneParity) {
    auto prepared = group_snapshot_fixture();
    fincore::factor_analysis_config config;
    config.periods = {"1D", "5D"};
    config.by_group = true;
    config.group_neutral = true;

    fincore::detail::reset_factor_compute_diagnostics();
    const auto model = fincore::analyze_factor(prepared, config);
    const auto diagnostics =
        fincore::detail::get_factor_compute_diagnostics();
    // One primary weight pass, one each for the two independent subgroup
    // reruns, and one separate equal-weight benchmark pass.
    EXPECT_EQ(diagnostics.weight_kernel_calls, 4U);
    // Aggregate, distinct unadjusted summary, by-group snapshot, and one IC
    // table for each independently analyzed subgroup.
    EXPECT_EQ(diagnostics.information_coefficient_kernel_calls, 5U);
    // One aggregate+by-date view for the model and one for the grouped
    // full-universe tables; independent subgroup reruns use the standalone
    // kernels. Positions are built once per period and portfolio inputs reuse
    // the first period's positions.
    EXPECT_EQ(diagnostics.quantile_view_builds, 2U);
    EXPECT_EQ(diagnostics.position_builds, 2U);
    EXPECT_EQ(diagnostics.portfolio_input_builds, 1U);
    EXPECT_EQ(diagnostics.panel_preprocessings, 0U);

    expect_same_ic(model.information_coefficient,
                   fincore::factor_information_coefficient(prepared, true));
    const auto expected_mean_ic =
        fincore::mean_information_coefficient(prepared, true);
    EXPECT_EQ(model.mean_information_coefficient.labels,
              expected_mean_ic.labels);
    EXPECT_EQ(model.mean_information_coefficient.groups,
              expected_mean_ic.groups);
    expect_same_values(model.mean_information_coefficient.means,
                       expected_mean_ic.means);
    ASSERT_TRUE(model.summary_information_coefficient.has_value());
    expect_same_ic(*model.summary_information_coefficient,
                   fincore::factor_information_coefficient(prepared, false));

    const auto expected_weights =
        fincore::factor_weights(prepared, true, true, false);
    expect_same_weights(model.factor_weights, expected_weights);
    expect_same_returns(model.factor_returns,
                        fincore::factor_returns(prepared, true, true, false));
    const auto expected_alpha_beta =
        fincore::factor_alpha_beta(prepared, true, true, false);
    ASSERT_EQ(model.alpha_beta.size(), expected_alpha_beta.size());
    for (std::size_t index = 0; index < model.alpha_beta.size(); ++index) {
        EXPECT_EQ(model.alpha_beta[index].label,
                  expected_alpha_beta[index].label);
        EXPECT_DOUBLE_EQ(model.alpha_beta[index].annual_alpha,
                         expected_alpha_beta[index].annual_alpha);
        EXPECT_DOUBLE_EQ(model.alpha_beta[index].beta,
                         expected_alpha_beta[index].beta);
    }

    ASSERT_EQ(model.factor_cumulative_returns.size(), config.periods.size());
    ASSERT_EQ(model.factor_positions.size(), config.periods.size());
    for (std::size_t index = 0; index < config.periods.size(); ++index) {
        expect_same_series(
            model.factor_cumulative_returns[index].values,
            fincore::factor_cumulative_returns(prepared, config.periods[index],
                                               true, true, false));
        expect_same_weights(
            model.factor_positions[index].positions,
            fincore::factor_positions(prepared, config.periods[index], true,
                                      true, false));
    }

    ASSERT_TRUE(model.portfolio_inputs.has_value());
    const auto expected_portfolio =
        fincore::build_factor_portfolio_inputs(prepared, config.periods.front(),
                                              true, true, false);
    expect_same_series(model.portfolio_inputs->returns,
                       expected_portfolio.returns);
    expect_same_weights(model.portfolio_inputs->positions,
                        expected_portfolio.positions);
    ASSERT_TRUE(model.portfolio_inputs->benchmark_returns.has_value());
    ASSERT_TRUE(expected_portfolio.benchmark_returns.has_value());
    expect_same_series(*model.portfolio_inputs->benchmark_returns,
                       *expected_portfolio.benchmark_returns);

    ASSERT_EQ(model.grouped_results.size(), 2U);
    for (std::size_t index = 0; index < model.grouped_results.size(); ++index) {
        fincore::prepared_factor_data group_data;
        group_data.forward_labels = prepared.forward_labels;
        for (const auto& row : prepared.rows) {
            if (row.group == model.grouped_results[index].group) {
                group_data.rows.push_back(row);
            }
        }
        const auto& grouped = model.grouped_results[index];
        expect_same_weights(
            grouped.factor_weights,
            fincore::factor_weights(group_data, true, false, false));
        expect_same_returns(
            grouped.factor_returns,
            fincore::factor_returns(group_data, true, false, false));
        expect_same_ic(grouped.information_coefficient,
                       fincore::factor_information_coefficient(group_data));
        const auto expected_group_ic =
            fincore::mean_information_coefficient(group_data);
        EXPECT_EQ(grouped.mean_information_coefficient.labels,
                  expected_group_ic.labels);
        expect_same_values(grouped.mean_information_coefficient.means,
                           expected_group_ic.means);
    }

    const auto after_model_projection =
        fincore::detail::get_factor_compute_diagnostics();
    EXPECT_FALSE(fincore::build_factor_tear_charts(model)
                     .information_coefficient.series.empty());
    EXPECT_FALSE(fincore::build_factor_report(model).sections.empty());
    EXPECT_FALSE(fincore::build_factor_tear_charts(model)
                     .information_coefficient.series.empty());
    EXPECT_FALSE(fincore::build_factor_report(model).sections.empty());
    const auto after_projections =
        fincore::detail::get_factor_compute_diagnostics();
    EXPECT_EQ(after_projections.weight_kernel_calls,
              after_model_projection.weight_kernel_calls);
    EXPECT_EQ(after_projections.information_coefficient_kernel_calls,
              after_model_projection.information_coefficient_kernel_calls);

    const double original_factor = prepared.rows[0].factor;
    const double original_return = prepared.rows[0].forward_returns[0];
    const double original_model_return = model.factor_returns.values[0];
    const double original_group_mean =
        model.group_snapshots[0].mean_returns_by_quantile.means[0];
    const std::string original_fingerprint = model.result_fingerprint;
    prepared.rows[0].factor += 7.0;
    prepared.rows[0].forward_returns[0] += 0.5;
    EXPECT_DOUBLE_EQ(model.factor_data.rows[0].factor, original_factor);
    EXPECT_DOUBLE_EQ(model.factor_data.rows[0].forward_returns[0],
                     original_return);
    EXPECT_DOUBLE_EQ(model.factor_returns.values[0], original_model_return);
    EXPECT_DOUBLE_EQ(
        model.group_snapshots[0].mean_returns_by_quantile.means[0],
        original_group_mean);

    const auto recomputed = fincore::analyze_factor(prepared, config);
    EXPECT_NE(recomputed.result_fingerprint, original_fingerprint);
    EXPECT_NE(recomputed.factor_returns.values[0], original_model_return);
}

TEST(FactorAnalysis, InfersFromStoredAggregateIcSnapshot) {
    fincore::factor_analysis_config config;
    config.periods = {"1D", "5D"};
    const auto model = fincore::analyze_factor(oracle_prepared(), config);
    const auto inference = fincore::factor_model_inference(model);
    ASSERT_EQ(inference.hypotheses.size(), 2U);
    EXPECT_EQ(inference.hypotheses[0].label, "1D");
    EXPECT_EQ(inference.hypotheses[0].n_observations, 3U);
    EXPECT_DOUBLE_EQ(inference.alpha, 0.05);
    EXPECT_TRUE(inference.hypotheses[0].testable);
    for (const auto& hypothesis : inference.hypotheses) {
        EXPECT_TRUE(std::isfinite(hypothesis.p_value));
        EXPECT_TRUE(std::isfinite(hypothesis.adjusted_p_value));
    }
}

TEST(FactorAnalysis, BuildsOptionalEventModelFromEventReturns) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    fincore::prepared_factor_data prepared;
    prepared.forward_labels = {"1D"};
    std::vector<fincore::timestamp> dates;
    for (int date = 0; date < 5; ++date) {
        dates.push_back(start + date * day);
    }
    const char* assets[] = {"A", "B", "C"};
    for (int date = 0; date < 5; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            fincore::factor_row row;
            row.date = dates[static_cast<std::size_t>(date)];
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset + 1;
            row.forward_returns = {
                0.01 * (asset + 1) + 0.001 * date};
            prepared.rows.push_back(row);
        }
    }
    fincore::dataframe event_returns;
    event_returns.load_index(std::vector<fincore::timestamp>{dates});
    event_returns.load_column<double>(
        "A", {0.010, 0.020, 0.030, 0.010, 0.040},
        hmdf::nan_policy::dont_pad_with_nans);
    event_returns.load_column<double>(
        "B", {0.020, 0.010, 0.020, 0.030, 0.010},
        hmdf::nan_policy::dont_pad_with_nans);
    event_returns.load_column<double>(
        "C", {0.030, 0.040, 0.010, 0.020, 0.030},
        hmdf::nan_policy::dont_pad_with_nans);
    fincore::factor_analysis_config config;
    config.periods = {"1D"};
    config.turnover_periods = {1};
    config.include_portfolio_inputs = false;
    const auto model = fincore::analyze_factor(prepared, config,
                                               event_returns, 1, 2);
    ASSERT_TRUE(model.event_analysis.has_value());
    const auto& event = *model.event_analysis;
    EXPECT_EQ(event.offsets, (std::vector<int>{-1, 0, 1, 2}));
    EXPECT_EQ(event.event_dates.size(), 5U);
    ASSERT_EQ(event.mean_returns.size(), 4U);
    for (const double value : event.mean_returns) {
        EXPECT_NEAR(value, 0.0, 1e-15);
    }
    EXPECT_EQ(event.return_distribution.size(), 16U);
    ASSERT_EQ(event.quantile_average_returns.size(), 3U);
    const auto& q1 = event.quantile_average_returns[0];
    EXPECT_EQ(q1.quantile, 1);
    EXPECT_NEAR(q1.means[0], -0.003333333333333334, 1e-15);
    EXPECT_NEAR(q1.means[2], 0.002499999999999999, 1e-15);
    EXPECT_NEAR(q1.means[3], 0.004444444444444444, 1e-15);
    EXPECT_NEAR(q1.standard_deviations[0], 0.009428090415820633, 1e-15);
    const auto& q2 = event.quantile_average_returns[1];
    EXPECT_NEAR(q2.means[0], -0.0008333333333333339, 1e-15);
    EXPECT_NEAR(q2.means[1], -0.004000000000000001, 1e-15);
    const auto& q3 = event.quantile_average_returns[2];
    EXPECT_NEAR(q3.means[0], 0.004166666666666666, 1e-15);
    EXPECT_NEAR(q3.means[3], -0.002222222222222223, 1e-15);
    ASSERT_TRUE(model.event_input_snapshot.has_value());
    EXPECT_EQ(model.event_input_snapshot->get_index().size(), 5U);

    const auto without_event = fincore::analyze_factor(prepared, config);
    EXPECT_NE(model.result_fingerprint, without_event.result_fingerprint);
    auto changed = event_returns;
    changed.load_column<double>(
        "A", {0.011, 0.020, 0.030, 0.010, 0.040},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto changed_model = fincore::analyze_factor(prepared, config,
                                                       changed, 1, 2);
    EXPECT_NE(changed_model.result_fingerprint, model.result_fingerprint);
    auto wider = event_returns;
    wider.load_column<double>(
        "D", {0.01, 0.02, 0.03, 0.04, 0.05},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto wider_model =
        fincore::analyze_factor(prepared, config, wider, 1, 2);
    EXPECT_NE(wider_model.result_fingerprint, model.result_fingerprint);

    auto neutral = config;
    neutral.group_neutral = true;
    EXPECT_THROW(static_cast<void>(fincore::analyze_factor(
                     prepared, neutral, event_returns, 1, 2)),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::analyze_factor(
                     prepared, config, event_returns, -1, 2)),
                 fincore::factor_data_error);
}

TEST(FactorAnalysis, BuildsGroupedResultsWhenRequested) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    fincore::prepared_factor_data prepared;
    prepared.forward_labels = {"1D"};
    const char* assets[] = {"A", "B", "C", "D"};
    const char* groups[] = {"G1", "G1", "G2", "G2"};
    const int quantiles[] = {1, 2, 1, 2};
    for (int date = 0; date < 4; ++date) {
        for (int asset = 0; asset < 4; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = quantiles[asset];
            row.has_group = true;
            row.group = groups[asset];
            row.forward_returns = {
                0.01 * (asset + 1) + 0.001 * date};
            prepared.rows.push_back(row);
        }
    }
    fincore::factor_analysis_config config;
    config.periods = {"1D"};
    config.turnover_periods = {1, 2};
    config.include_portfolio_inputs = false;
    config.by_group = true;
    const auto model = fincore::analyze_factor(prepared, config);
    ASSERT_EQ(model.grouped_results.size(), 2U);
    const auto& g1 = model.grouped_results[0];
    EXPECT_EQ(g1.group, "G1");
    ASSERT_EQ(g1.quantile_statistics.size(), 2U);
    EXPECT_DOUBLE_EQ(g1.quantile_statistics[0].mean, 1.0);
    EXPECT_DOUBLE_EQ(g1.quantile_statistics[1].mean, 2.0);
    EXPECT_EQ(g1.quantile_statistics[0].count, 4U);
    EXPECT_DOUBLE_EQ(g1.quantile_statistics[0].count_percent, 50.0);
    ASSERT_EQ(g1.mean_returns_by_quantile.means.size(), 2U);
    EXPECT_NEAR(g1.mean_returns_by_quantile.means[0], -0.005000000000000001,
                1e-15);
    EXPECT_NEAR(g1.mean_returns_by_quantile.means[1], 0.004999999999999999,
                1e-15);
    ASSERT_EQ(g1.factor_weights.size(), 8U);
    EXPECT_EQ(g1.factor_weights[0].asset, "A");
    EXPECT_DOUBLE_EQ(g1.factor_weights[0].weight, -0.5);
    EXPECT_EQ(g1.factor_weights[1].asset, "B");
    EXPECT_DOUBLE_EQ(g1.factor_weights[1].weight, 0.5);
    ASSERT_EQ(g1.factor_returns.dates.size(), 4U);
    EXPECT_NEAR(g1.factor_returns.values[0], 0.005, 1e-15);
    EXPECT_NEAR(g1.factor_returns.values[1], 0.005000000000000001, 1e-15);
    ASSERT_EQ(g1.information_coefficient.values.size(), 4U);
    EXPECT_NEAR(g1.information_coefficient.values[0],
                0.9999999999999999, 1e-12);
    EXPECT_NEAR(g1.mean_information_coefficient.means[0],
                0.9999999999999999, 1e-12);
    ASSERT_EQ(g1.quantile_turnover.size(), 4U);
    for (const auto& turnover : g1.quantile_turnover) {
        EXPECT_DOUBLE_EQ(turnover.values.values.back(), 0.0);
    }
    ASSERT_EQ(g1.rank_autocorrelation.size(), 2U);
    EXPECT_NEAR(g1.rank_autocorrelation[0].values.values.back(),
                0.9999999999999998, 1e-12);

    const auto& g2 = model.grouped_results[1];
    EXPECT_EQ(g2.group, "G2");
    EXPECT_DOUBLE_EQ(g2.quantile_statistics[0].mean, 3.0);
    EXPECT_DOUBLE_EQ(g2.quantile_statistics[1].mean, 4.0);
    EXPECT_NEAR(g2.mean_returns_by_quantile.means[0],
                -0.0050000000000000044, 1e-15);
    EXPECT_NEAR(g2.mean_returns_by_quantile.means[1],
                0.0049999999999999975, 1e-15);

    auto aggregate = config;
    aggregate.by_group = false;
    EXPECT_NE(fincore::analyze_factor(prepared, aggregate).result_fingerprint,
              model.result_fingerprint);
    auto missing = prepared;
    missing.rows[0].has_group = false;
    EXPECT_THROW(
        static_cast<void>(fincore::analyze_factor(missing, config)),
        fincore::factor_data_error);
}

}  // namespace

TEST(FactorAnalysis, MatchesCleanPythonModelSnapshot) {
    fincore::factor_analysis_config config;
    config.periods = {"1D", "5D"};
    config.turnover_periods = {1, 2};
    config.time_aggregation = {"M"};
    const auto model =
        fincore::analyze_factor(oracle_prepared(), config);

    ASSERT_EQ(model.forward_periods.size(), 2U);
    ASSERT_EQ(model.quantile_statistics.size(), 2U);
    const auto& q1 = model.quantile_statistics[0];
    EXPECT_EQ(q1.quantile, 1);
    EXPECT_DOUBLE_EQ(q1.minimum, 1.0);
    EXPECT_DOUBLE_EQ(q1.maximum, 2.0);
    EXPECT_DOUBLE_EQ(q1.mean, 1.5);
    EXPECT_NEAR(q1.stddev, 0.5477225575051661, 1e-15);
    EXPECT_EQ(q1.count, 6U);
    EXPECT_DOUBLE_EQ(q1.count_percent, 50.0);
    EXPECT_DOUBLE_EQ(model.quantile_statistics[1].mean, 3.5);

    ASSERT_EQ(model.information_coefficient.dates.size(), 3U);
    for (const double value : model.information_coefficient.values) {
        EXPECT_DOUBLE_EQ(value, 1.0);
    }
    ASSERT_EQ(model.mean_information_coefficient.means.size(), 2U);
    EXPECT_DOUBLE_EQ(model.mean_information_coefficient.means[0], 1.0);

    ASSERT_EQ(model.mean_returns_by_quantile.means.size(), 4U);
    EXPECT_NEAR(model.mean_returns_by_quantile.means[0],
                -0.010000000000000002, 1e-15);
    EXPECT_NEAR(model.mean_returns_by_quantile.means[1],
                -0.049999999999999996, 1e-15);
    EXPECT_NEAR(model.mean_returns_by_quantile.means[2],
                0.009999999999999998, 1e-15);
    EXPECT_NEAR(model.mean_returns_by_quantile.means[3],
                0.05000000000000002, 1e-15);

    ASSERT_EQ(model.mean_returns_by_date.dates.size(), 3U);
    ASSERT_EQ(model.mean_returns_by_date.quantiles.size(), 2U);
    ASSERT_EQ(model.mean_returns_by_date.labels.size(), 2U);
    EXPECT_NEAR(model.mean_returns_by_date.means[0],
                -0.010000000000000002, 1e-15);
    EXPECT_NEAR(model.mean_returns_by_date.means[1],
                -0.049999999999999996, 1e-15);
    EXPECT_NEAR(model.mean_returns_by_date.means[2],
                0.009999999999999998, 1e-15);
    EXPECT_NEAR(model.mean_returns_by_date.standard_errors[0], 0.005,
                1e-15);

    ASSERT_EQ(model.mean_return_spread.size(), 2U);
    EXPECT_EQ(model.mean_return_spread[0].period, "1D");
    EXPECT_NEAR(model.mean_return_spread[0].spread, 0.02, 1e-15);
    EXPECT_LT(std::abs(model.mean_return_spread[0].standard_error), 1e-15);
    EXPECT_NEAR(model.mean_return_spread[1].spread, 0.1, 1e-15);

    ASSERT_EQ(model.quantile_turnover.size(), 4U);
    for (const auto& turnover : model.quantile_turnover) {
        EXPECT_TRUE(std::isnan(turnover.values.values[0]));
        EXPECT_DOUBLE_EQ(turnover.values.values.back(), 0.0);
    }
    ASSERT_EQ(model.rank_autocorrelation.size(), 2U);
    EXPECT_TRUE(std::isnan(
        model.rank_autocorrelation[0].values.values[0]));
    EXPECT_DOUBLE_EQ(model.rank_autocorrelation[0].values.values[1], 1.0);
    EXPECT_DOUBLE_EQ(model.rank_autocorrelation[1].values.values[2], 1.0);

    ASSERT_EQ(model.factor_cumulative_returns.size(), 2U);
    EXPECT_NEAR(model.factor_cumulative_returns[0].values.values.back(),
                1.0379707031249998, 1e-15);
    EXPECT_NEAR(model.factor_cumulative_returns[1].values.values.back(),
                1.199462890625, 1e-15);

    ASSERT_EQ(model.quantile_cumulative_returns.size(), 2U);
    const auto& curves = model.quantile_cumulative_returns[0];
    ASSERT_EQ(curves.quantiles.size(), 2U);
    EXPECT_EQ(curves.quantiles[0], 2);
    EXPECT_EQ(curves.quantiles[1], 1);
    EXPECT_NEAR(curves.values[0], 1.01, 1e-15);
    EXPECT_NEAR(curves.values[1], 0.99, 1e-15);
    EXPECT_NEAR(curves.values[4], 1.030301, 1e-15);
    EXPECT_NEAR(curves.values[5], 0.9702989999999999, 1e-15);
    EXPECT_NEAR(model.quantile_cumulative_returns[1].values[0],
                1.0098057976734853, 1e-15);
    EXPECT_NEAR(model.quantile_cumulative_returns[1].values[1],
                0.9897937816869885, 1e-15);

    ASSERT_EQ(model.factor_positions.size(), 2U);
    EXPECT_EQ(model.factor_positions[0].positions.size(), 16U);
    EXPECT_EQ(model.factor_positions[1].positions.size(), 24U);

    ASSERT_EQ(model.time_aggregated_information.size(), 1U);
    const auto& monthly = model.time_aggregated_information[0];
    EXPECT_EQ(monthly.frequency, "M");
    ASSERT_EQ(monthly.dates.size(), 1U);
    EXPECT_EQ(monthly.dates[0], fincore::utc_timestamp(2022, 1, 31));
    ASSERT_EQ(monthly.means.size(), 2U);
    EXPECT_DOUBLE_EQ(monthly.means[0], 1.0);

    ASSERT_TRUE(model.portfolio_inputs.has_value());
    EXPECT_NEAR(model.portfolio_inputs->returns.values[1],
                0.012499999999999956, 1e-15);
    EXPECT_EQ(model.portfolio_inputs->positions.size(), 20U);
    ASSERT_TRUE(model.portfolio_inputs->benchmark_returns.has_value());
    EXPECT_NEAR(model.portfolio_inputs->benchmark_returns->values[1], 0.026,
                1e-15);
    EXPECT_NEAR(model.portfolio_inputs->benchmark_returns->values[2], 0.027,
                1e-15);
    EXPECT_EQ(model.result_fingerprint.size(), 64U);

    auto lean = config;
    lean.include_portfolio_inputs = false;
    EXPECT_FALSE(
        fincore::analyze_factor(oracle_prepared(), lean).portfolio_inputs);
    auto unknown = config;
    unknown.periods = {"9D"};
    EXPECT_THROW(
        static_cast<void>(
            fincore::analyze_factor(oracle_prepared(), unknown)),
        fincore::factor_data_error);
    auto zero_lag = config;
    zero_lag.turnover_periods = {0};
    EXPECT_THROW(
        static_cast<void>(
            fincore::analyze_factor(oracle_prepared(), zero_lag)),
        fincore::factor_data_error);
    auto weekly = config;
    weekly.time_aggregation = {"W"};
    EXPECT_THROW(
        static_cast<void>(
            fincore::analyze_factor(oracle_prepared(), weekly)),
        fincore::factor_data_error);
}

namespace {

fincore::prepared_factor_data fingerprint_fixture() {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D", "5D"};
    fincore::factor_row row{};
    row.date = fincore::utc_timestamp(2024, 1, 2);
    row.asset = "A";
    row.factor = 0.0;
    row.quantile = 0;
    row.has_group = true;
    row.group = "G";
    row.forward_returns = {0.0, 0.0};
    data.rows.push_back(std::move(row));
    return data;
}

}  // namespace

TEST(FactorAnalysis, FingerprintFramesEmbeddedTextAndGroupPresence) {
    auto first = fingerprint_fixture();
    first.rows[0].asset = "A";
    first.rows[0].group = std::string("\0", 1U);
    auto second = first;
    second.rows[0].asset = std::string("A\0", 2U);
    second.rows[0].group.clear();

    fincore::factor_analysis_config config;
    config.periods = {"1D"};
    config.include_portfolio_inputs = false;
    // The old NUL-terminated framing collided for these equal-width numeric
    // fields: one embedded NUL moved from group text to asset text.
    EXPECT_NE(fincore::analyze_factor(first, config).result_fingerprint,
              fincore::analyze_factor(second, config).result_fingerprint);

    auto absent_group = fingerprint_fixture();
    absent_group.rows[0].has_group = false;
    absent_group.rows[0].group.clear();
    auto empty_group = absent_group;
    empty_group.rows[0].has_group = true;
    EXPECT_NE(
        fincore::analyze_factor(absent_group, config).result_fingerprint,
        fincore::analyze_factor(empty_group, config).result_fingerprint);
}

TEST(FactorAnalysis, FingerprintIncludesUnselectedInputsAndOwnsConfig) {
    const auto prepared = prepared_fixture();
    fincore::factor_analysis_config config;
    config.long_short = false;
    config.equal_weight = true;
    config.periods = {"1D"};
    config.turnover_periods = {2};
    config.time_aggregation = {"ME"};
    config.include_portfolio_inputs = false;
    config.portfolio_benchmark_period = "5D";

    const auto model = fincore::analyze_factor(prepared, config);
    ASSERT_EQ(model.config.periods, (std::vector<std::string>{"1D"}));
    EXPECT_EQ(model.config.turnover_periods, (std::vector<int>{2}));
    EXPECT_EQ(model.config.time_aggregation,
              (std::vector<std::string>{"ME"}));
    EXPECT_EQ(model.config.portfolio_benchmark_period, "5D");
    EXPECT_EQ(model.result_fingerprint_version,
              fincore::factor_analysis_fingerprint_version);

    auto changed_label = prepared;
    ASSERT_GE(changed_label.forward_labels.size(), 2U);
    changed_label.forward_labels[1] = "10D";
    const auto label_model = fincore::analyze_factor(changed_label, config);
    EXPECT_NE(label_model.result_fingerprint, model.result_fingerprint);
    EXPECT_EQ(label_model.factor_returns.values, model.factor_returns.values);

    auto changed_unselected_input = prepared;
    changed_unselected_input.rows[0].forward_returns[1] += 0.25;
    const auto input_model =
        fincore::analyze_factor(changed_unselected_input, config);
    EXPECT_NE(input_model.result_fingerprint, model.result_fingerprint);
    EXPECT_EQ(input_model.factor_returns.values, model.factor_returns.values);

    auto changed_loss_metadata = prepared;
    changed_loss_metadata.loss_report.forward_returns_loss += 0.01;
    const auto loss_model =
        fincore::analyze_factor(changed_loss_metadata, config);
    EXPECT_NE(loss_model.result_fingerprint, model.result_fingerprint);
    EXPECT_EQ(loss_model.factor_returns.values, model.factor_returns.values);

    // Omitting periods resolves to the same selected periods as naming every
    // label, so only the fingerprint records which spelling the caller used.
    auto implicit_all_periods = config;
    implicit_all_periods.periods.clear();
    auto explicit_all_periods = config;
    explicit_all_periods.periods = prepared.forward_labels;
    const auto implicit_model =
        fincore::analyze_factor(prepared, implicit_all_periods);
    const auto explicit_model =
        fincore::analyze_factor(prepared, explicit_all_periods);
    EXPECT_EQ(implicit_model.forward_periods, explicit_model.forward_periods);
    EXPECT_NE(implicit_model.result_fingerprint,
              explicit_model.result_fingerprint);

    const auto original_fingerprint = model.result_fingerprint;
    config.periods[0] = "5D";
    config.turnover_periods[0] = 3;
    config.time_aggregation[0] = "M";
    config.portfolio_benchmark_period = "1D";
    EXPECT_EQ(model.config.periods, (std::vector<std::string>{"1D"}));
    EXPECT_EQ(model.config.turnover_periods, (std::vector<int>{2}));
    EXPECT_EQ(model.config.time_aggregation,
              (std::vector<std::string>{"ME"}));
    EXPECT_EQ(model.config.portfolio_benchmark_period, "5D");
    EXPECT_EQ(model.result_fingerprint, original_fingerprint);
    const auto changed_config_model = fincore::analyze_factor(prepared, config);
    EXPECT_NE(changed_config_model.result_fingerprint, original_fingerprint);
}

TEST(FactorAnalysis, EventFingerprintRejectsUnsupportedOrRaggedColumns) {
    const auto prepared = prepared_fixture();
    fincore::factor_analysis_config config;
    config.periods = {"1D"};
    config.include_portfolio_inputs = false;
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;

    fincore::dataframe mixed;
    mixed.load_index(std::vector<fincore::timestamp>{start, start + day});
    mixed.load_column<double>("returns", {0.01, 0.02},
                              hmdf::nan_policy::dont_pad_with_nans);
    mixed.load_column<int>("metadata", {1, 2},
                           hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::analyze_factor(
                     prepared, config, mixed, 0, 0)),
                 fincore::factor_data_error);

    fincore::dataframe ragged;
    ragged.load_index(std::vector<fincore::timestamp>{start, start + day});
    ragged.load_column<double>("returns", {0.01},
                               hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::analyze_factor(
                     prepared, config, ragged, 0, 0)),
                 fincore::factor_data_error);
}

TEST(FactorAnalysis, RejectsPeriodDayCountOverflow) {
    const std::string maximum =
        std::to_string(std::numeric_limits<int>::max());
    for (const auto& label : {maximum + "0D", maximum + "D1D"}) {
        auto prepared = fingerprint_fixture();
        prepared.forward_labels = {label};
        prepared.rows[0].forward_returns = {0.0};
        fincore::factor_analysis_config config;
        config.periods = {label};
        config.include_portfolio_inputs = false;
        EXPECT_THROW(static_cast<void>(fincore::analyze_factor(prepared, config)),
                     fincore::factor_data_error);
        EXPECT_THROW(static_cast<void>(fincore::factor_positions(
                         prepared, label, true, false, false)),
                     fincore::factor_data_error);
    }
}

TEST(FactorAnalysis, DefaultRecordsHaveSafeFieldsAndStableFingerprints) {
    fincore::factor_row row;
    EXPECT_EQ(row.date, 0);
    EXPECT_TRUE(row.asset.empty());
    EXPECT_DOUBLE_EQ(row.factor, 0.0);
    EXPECT_EQ(row.quantile, 0);
    EXPECT_FALSE(row.has_group);
    EXPECT_TRUE(row.group.empty());
    EXPECT_TRUE(row.forward_returns.empty());

    fincore::factor_loss_report loss;
    EXPECT_EQ(loss.input_count, 0U);
    EXPECT_EQ(loss.finite_factor_count, 0U);
    EXPECT_EQ(loss.forward_returns_count, 0U);
    EXPECT_EQ(loss.binning_count, 0U);
    EXPECT_DOUBLE_EQ(loss.factor_input_loss, 0.0);
    EXPECT_DOUBLE_EQ(loss.forward_returns_loss, 0.0);
    EXPECT_DOUBLE_EQ(loss.binning_loss, 0.0);
    EXPECT_DOUBLE_EQ(loss.total_loss, 0.0);

    fincore::prepared_factor_data data;
    data.forward_labels = {"1D"};
    row.date = fincore::utc_timestamp(2024, 1, 2);
    row.asset = "A";
    row.factor = 0.5;
    row.quantile = 1;
    row.forward_returns = {0.01};
    data.rows.push_back(row);  // has_group remains the safe default: false

    fincore::factor_analysis_config config;
    config.periods = {"1D"};
    config.include_portfolio_inputs = false;
    const auto first = fincore::analyze_factor(data, config);
    const auto second = fincore::analyze_factor(data, config);
    EXPECT_FALSE(first.factor_data.rows[0].has_group);
    EXPECT_EQ(first.factor_data.rows[0].group, "");
    EXPECT_EQ(first.result_fingerprint, second.result_fingerprint);
}

TEST(FactorAnalysis, QuantileViewsMatchStandaloneKernels) {
    const auto prepared = prepared_fixture();
    const auto views = fincore::detail::build_quantile_return_views(
        prepared, false, true, false, true, true);
    const auto aggregate = fincore::mean_return_by_quantile(
        prepared, false, false, true, false);
    const auto by_date = fincore::mean_return_by_quantile_by_date(
        prepared, true, false, false);
    EXPECT_EQ(views.aggregate.quantiles, aggregate.quantiles);
    EXPECT_EQ(views.aggregate.labels, aggregate.labels);
    expect_same_values(views.aggregate.means, aggregate.means);
    expect_same_values(views.aggregate.standard_errors,
                       aggregate.standard_errors);
    EXPECT_EQ(views.by_date.quantiles, by_date.quantiles);
    EXPECT_EQ(views.by_date.labels, by_date.labels);
    EXPECT_EQ(views.by_date.dates, by_date.dates);
    expect_same_values(views.by_date.means, by_date.means);
    expect_same_values(views.by_date.standard_errors,
                       by_date.standard_errors);

    const auto grouped = group_snapshot_fixture();
    const auto grouped_views = fincore::detail::build_quantile_return_views(
        grouped, true, true, false, true, true);
    const auto grouped_aggregate = fincore::mean_return_by_quantile(
        grouped, false, true, true, false);
    const auto grouped_by_date = fincore::mean_return_by_quantile_by_date(
        grouped, true, false, true);
    EXPECT_EQ(grouped_views.aggregate.quantiles,
              grouped_aggregate.quantiles);
    EXPECT_EQ(grouped_views.aggregate.groups, grouped_aggregate.groups);
    expect_same_values(grouped_views.aggregate.means,
                       grouped_aggregate.means);
    expect_same_values(grouped_views.aggregate.standard_errors,
                       grouped_aggregate.standard_errors);
    EXPECT_EQ(grouped_views.by_date.quantiles, grouped_by_date.quantiles);
    EXPECT_EQ(grouped_views.by_date.groups, grouped_by_date.groups);
    EXPECT_EQ(grouped_views.by_date.dates, grouped_by_date.dates);
    expect_same_values(grouped_views.by_date.means, grouped_by_date.means);
    expect_same_values(grouped_views.by_date.standard_errors,
                       grouped_by_date.standard_errors);
}

TEST(FactorAnalysis, FingerprintMatchesThePreRefactorGolden) {
    auto prepared = prepared_fixture();
    fincore::factor_analysis_config config;
    config.periods = {"1D", "5D"};
    config.include_portfolio_inputs = false;
    const auto model = fincore::analyze_factor(prepared, config);
    // The v2 payload contains libm-derived factor values. Keep one golden per
    // audited platform; S->C digest equality is separately checked by the
    // alignment benchmark on each platform.
#if defined(__APPLE__)
    EXPECT_EQ(model.result_fingerprint,
              "9245d5acea4a79cb6442940614e39ef5ffb0d2346d0d5c833c2812afd8952c68");
#elif defined(__linux__)
#if defined(__x86_64__) || defined(_M_X64)
    EXPECT_EQ(model.result_fingerprint,
              "3ded2b858e92ca624f1aeaed14353b61aceac16ba51728cbf5085602e4f15f76");
#else
    EXPECT_EQ(model.result_fingerprint,
              "1cbdec5c2a11faa9a20320ac8ecb479cf1d644d3aec2b904f2caaac1ddaac869");
#endif
#else
    EXPECT_EQ(model.result_fingerprint.size(), 64U);
#endif
}


TEST(Detail, Sha256KnownAnswersAndIncrementalLifecycle) {
    struct known_answer {
        std::size_t size;
        const char* hex;
    };
    const std::vector<known_answer> answers{
        {0U, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {3U, "b39fad1a1075f64570b3226d339ea818f9c66ecd2f1c59fd8b9c5a32b54c513f"},
        {55U, "2900465fcb533e05a158fd2b3be0e5e3b03740d83060aa3580e0d98a96bf2384"},
        {56U, "31454ff48ef36af2f08fd511bdc37d9d5855ac23e992e5ff5445cb6b7674a674"},
        {63U, "5f6401b96532c36de4e65beec0409b69b1d181864c8009b7a04f43e5d56350d1"},
        {64U, "94eb5de4943613fd048dc93393ab06877405faa39c11f53e9386083339833e7e"},
        {65U, "fc518669b6eb4b4dd91827ecacef86689c725bd5bab888fd3b26dbb196eec954"},
        {1048576U, "3a814d27145f9540c495715da2f85208f3805c323d7da3e9867df2992be4432d"}};
    const std::vector<std::uint8_t> abc{'a', 'b', 'c'};
    EXPECT_EQ(fincore::detail::sha256_hex(abc),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    for (const auto& answer : answers) {
        std::vector<std::uint8_t> payload(answer.size);
        for (std::size_t index = 0; index < payload.size(); ++index) {
            payload[index] =
                static_cast<std::uint8_t>((index * 37U + 11U) % 256U);
        }
        EXPECT_EQ(fincore::detail::sha256_hex(payload), answer.hex);
        for (const std::size_t chunk : {std::size_t{1U}, std::size_t{7U},
                                        std::size_t{64U},
                                        std::size_t{4093U}}) {
            fincore::detail::sha256_hasher hasher;
            for (std::size_t offset = 0; offset < payload.size();
                 offset += chunk) {
                const std::size_t take =
                    std::min(chunk, payload.size() - offset);
                hasher.update(std::span<const std::uint8_t>(
                    payload.data() + offset, take));
            }
            const auto first = hasher.finish_hex();
            EXPECT_EQ(first, answer.hex) << "chunk " << chunk;
            EXPECT_EQ(hasher.finish_hex(), first);
            std::vector<std::uint8_t> extended = payload;
            extended.push_back(static_cast<std::uint8_t>('x'));
            hasher.update(std::span<const std::uint8_t>(
                extended.data() + payload.size(), 1U));
            EXPECT_EQ(hasher.finish_hex(),
                      fincore::detail::sha256_hex(extended));
        }
    }
}

TEST(FactorAnalysis, DisabledPortfolioInputsBuildNothing) {
    auto prepared = prepared_fixture();
    fincore::factor_analysis_config config;
    config.periods = {"1D"};
    config.include_portfolio_inputs = false;
    fincore::detail::reset_factor_compute_diagnostics();
    const auto model = fincore::analyze_factor(prepared, config);
    const auto counts = fincore::detail::get_factor_compute_diagnostics();
    EXPECT_FALSE(model.portfolio_inputs.has_value());
    EXPECT_EQ(counts.portfolio_input_builds, 0U);
    EXPECT_EQ(counts.position_builds, 1U);
}
