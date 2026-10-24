#include <fincore/calendar.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/performance.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

struct factor_fixture {
    fincore::factor_series factor;
    fincore::dataframe prices;
};

factor_fixture make_fixture() {
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C", "D", "E"};
    factor_fixture fixture;
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
        fixture.prices.load_column<double>(
            assets[asset], std::move(values),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return fixture;
}

fincore::prepared_factor_data prepare() {
    auto fixture = make_fixture();
    fincore::prepare_factor_config config;
    config.periods = {1, 5};
    config.quantiles = 3;
    config.max_loss = 0.5;
    return fincore::prepare_factor_data(fixture.factor, fixture.prices,
                                        config);
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

std::vector<std::string> split_fixture_csv(const std::string& line) {
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

fincore::prepared_factor_data load_group_snapshot_fixture() {
    std::ifstream input{group_snapshot_fixture_path("unequal_groups.csv")};
    if (!input) {
        throw std::runtime_error{"unable to open unequal group fixture"};
    }
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D", "5D"};
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        const auto fields = split_fixture_csv(line);
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

using group_kernel_key =
    std::tuple<std::string, fincore::timestamp, std::string, int,
               std::string>;
using group_kernel_values = std::pair<double, double>;

struct group_kernel_oracle_row {
    std::string view;
    std::string mode;
    std::string date;
    std::string group;
    int quantile = -1;
    std::string label;
    double value = 0.0;
    double standard_error = 0.0;
};

std::vector<group_kernel_oracle_row> load_group_kernel_oracle() {
    std::ifstream input{group_snapshot_fixture_path("python_oracle.csv")};
    if (!input) {
        throw std::runtime_error{"unable to open Python group oracle fixture"};
    }
    std::vector<group_kernel_oracle_row> rows;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        const auto fields = split_fixture_csv(line);
        if (fields.size() != 8U) {
            throw std::runtime_error{"invalid Python group oracle row"};
        }
        group_kernel_oracle_row row;
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

TEST(FactorPerformance, InformationCoefficientMatchesOracle) {
    const auto prepared = prepare();
    ASSERT_EQ(prepared.rows.size(), 300U);
    const auto information =
        fincore::factor_information_coefficient(prepared);
    ASSERT_EQ(information.dates.size(), 60U);
    ASSERT_EQ(information.labels.size(), 2U);
    EXPECT_NEAR(information.values[0], 0.9999999999999999, 1e-12);
    EXPECT_NEAR(information.values[1], 0.8999999999999998, 1e-12);
    EXPECT_NEAR(information.values[2], 0.8999999999999998, 1e-12);
    EXPECT_NEAR(information.values[3], 0.8999999999999998, 1e-12);
    const auto summary =
        fincore::mean_information_coefficient(prepared);
    ASSERT_EQ(summary.means.size(), 2U);
    EXPECT_NEAR(summary.means[0], -0.17833333333333332, 1e-12);
    EXPECT_NEAR(summary.means[1], -0.10333333333333333, 1e-12);
}

TEST(FactorPerformance, WeightsAndFactorReturnsMatchOracle) {
    const auto prepared = prepare();
    const auto weights = fincore::factor_weights(prepared);
    ASSERT_EQ(weights.size(), 300U);
    EXPECT_EQ(weights[0].asset, "A");
    EXPECT_NEAR(weights[0].weight, 0.004757198346676114, 1e-12);
    EXPECT_NEAR(weights[1].weight, 0.28611343322574456, 1e-12);
    EXPECT_NEAR(weights[2].weight, 0.20912936842757943, 1e-12);
    EXPECT_NEAR(weights[3].weight, -0.11190529968370366, 1e-12);
    EXPECT_NEAR(weights[4].weight, -0.3880947003162962, 1e-12);

    const auto returns = fincore::factor_returns(prepared);
    ASSERT_EQ(returns.dates.size(), 60U);
    ASSERT_EQ(returns.values.size(), 120U);
    EXPECT_NEAR(returns.values[0], 0.003392085591589239, 1e-12);
    EXPECT_NEAR(returns.values[1], 0.016343613215012726, 1e-12);
    EXPECT_NEAR(returns.values[118], -0.0011912205938170094, 1e-12);
    EXPECT_NEAR(returns.values[119], -0.00851199079401172, 1e-12);
}

TEST(FactorPerformance, AlphaBetaMatchesOracle) {
    const auto prepared = prepare();
    const auto results = fincore::factor_alpha_beta(prepared);
    ASSERT_EQ(results.size(), 2U);
    EXPECT_EQ(results[0].label, "1D");
    EXPECT_NEAR(results[0].annual_alpha, -0.4490675338819351, 1e-10);
    EXPECT_NEAR(results[0].beta, 0.16261645466483737, 1e-12);
    EXPECT_EQ(results[1].label, "5D");
    EXPECT_NEAR(results[1].annual_alpha, -0.09732173534174071, 1e-10);
    EXPECT_NEAR(results[1].beta, -0.12217052082079653, 1e-12);
}

TEST(FactorPerformance, QuantileMeansMatchOracle) {
    const auto prepared = prepare();
    const auto table = fincore::mean_return_by_quantile(prepared);
    ASSERT_EQ(table.quantiles.size(), 3U);
    ASSERT_EQ(table.labels.size(), 2U);
    EXPECT_EQ(table.quantiles[0], 1);
    EXPECT_NEAR(table.means[0], 0.00201525411346395, 1e-12);
    EXPECT_NEAR(table.means[1], 0.003725411319908001, 1e-12);
    EXPECT_NEAR(table.means[2], -0.00046307197887511623, 1e-12);
    EXPECT_NEAR(table.means[3], -0.0018155105473475256, 1e-12);
    EXPECT_NEAR(table.means[4], -0.001783718124026392, 1e-12);
    EXPECT_NEAR(table.means[5], -0.002817656046234239, 1e-12);
    EXPECT_NEAR(table.standard_errors[0], 0.0005338805510157927, 1e-12);
    EXPECT_NEAR(table.standard_errors[1], 0.0025665960550222905, 1e-12);
    EXPECT_NEAR(table.standard_errors[2], 0.0007997093415032477, 1e-12);
    EXPECT_NEAR(table.standard_errors[5], 0.0026833705384821994, 1e-12);

    EXPECT_THROW(
        static_cast<void>(fincore::mean_return_by_quantile(
            prepared, true, false, true, false)),
        fincore::factor_data_error);
}

TEST(FactorPerformance, ByGroupKernelsPreserveGroupRowsAgainstOracle) {
    const auto prepared = load_group_snapshot_fixture();
    const auto oracle = load_group_kernel_oracle();
    for (const bool group_adjust : {false, true}) {
        const std::string mode = group_adjust ? "group_neutral" : "ordinary";
        std::map<group_kernel_key, group_kernel_values> actual;
        const auto means = fincore::mean_return_by_quantile(
            prepared, false, true, true, group_adjust);
        ASSERT_EQ(means.quantiles.size(), means.groups.size());
        for (std::size_t row = 0; row < means.quantiles.size(); ++row) {
            for (std::size_t label = 0; label < means.labels.size(); ++label) {
                const std::size_t position = row * means.labels.size() + label;
                actual.emplace(
                    group_kernel_key{"quantile_mean", 0, means.groups[row],
                                     means.quantiles[row], means.labels[label]},
                    group_kernel_values{means.means[position],
                                        means.standard_errors[position]});
            }
        }

        const auto by_date = fincore::mean_return_by_quantile_by_date(
            prepared, true, group_adjust, true);
        ASSERT_EQ(by_date.dates.size(), by_date.groups.size());
        for (std::size_t row = 0; row < by_date.dates.size(); ++row) {
            for (std::size_t quantile = 0;
                 quantile < by_date.quantiles.size(); ++quantile) {
                for (std::size_t label = 0; label < by_date.labels.size();
                     ++label) {
                    const std::size_t position =
                        (row * by_date.quantiles.size() + quantile) *
                            by_date.labels.size() +
                        label;
                    actual.emplace(
                        group_kernel_key{"quantile_by_date",
                                         by_date.dates[row], by_date.groups[row],
                                         by_date.quantiles[quantile],
                                         by_date.labels[label]},
                        group_kernel_values{by_date.means[position],
                                            by_date.standard_errors[position]});
                }
            }
        }

        const auto spread = fincore::mean_return_spread(means, 3, 1);
        ASSERT_EQ(spread.labels.size(), spread.groups.size());
        ASSERT_EQ(spread.labels.size(), spread.spreads.size());
        ASSERT_EQ(spread.labels.size(), spread.standard_errors.size());
        for (std::size_t row = 0; row < spread.labels.size(); ++row) {
            actual.emplace(
                group_kernel_key{"spread", 0, spread.groups[row], -1,
                                 spread.labels[row]},
                group_kernel_values{spread.spreads[row],
                                    spread.standard_errors[row]});
        }

        const auto information =
            fincore::factor_information_coefficient(prepared, group_adjust,
                                                    true);
        ASSERT_EQ(information.dates.size(), information.groups.size());
        for (std::size_t row = 0; row < information.dates.size(); ++row) {
            for (std::size_t label = 0; label < information.labels.size();
                 ++label) {
                actual.emplace(
                    group_kernel_key{"ic", information.dates[row],
                                     information.groups[row], -1,
                                     information.labels[label]},
                    group_kernel_values{
                        information.values[row * information.labels.size() +
                                           label],
                        std::nan("")});
            }
        }
        const auto mean_ic =
            fincore::mean_information_coefficient(prepared, group_adjust, true);
        ASSERT_EQ(mean_ic.groups.size() * mean_ic.labels.size(),
                  mean_ic.means.size());
        for (std::size_t group = 0; group < mean_ic.groups.size(); ++group) {
            for (std::size_t label = 0; label < mean_ic.labels.size(); ++label) {
                actual.emplace(
                    group_kernel_key{"mean_ic", 0, mean_ic.groups[group], -1,
                                     mean_ic.labels[label]},
                    group_kernel_values{
                        mean_ic.means[group * mean_ic.labels.size() + label],
                        std::nan("")});
            }
        }

        std::map<group_kernel_key, group_kernel_values> expected;
        for (const auto& row : oracle) {
            if (row.mode != mode || row.view == "monthly_ic" ||
                row.view == "summary_ic") {
                continue;
            }
            const fincore::timestamp date =
                row.date.empty() ? 0 : parse_fixture_date(row.date);
            expected.emplace(
                group_kernel_key{row.view, date, row.group, row.quantile,
                                 row.label},
                group_kernel_values{row.value, row.standard_error});
        }
        for (const auto& [key, expected_values] : expected) {
            const auto found = actual.find(key);
            ASSERT_NE(found, actual.end()) << std::get<0>(key) << " "
                                           << std::get<2>(key);
            expect_oracle_value(found->second.first, expected_values.first);
            expect_oracle_value(found->second.second,
                                expected_values.second);
        }
        for (const auto& [key, values] : actual) {
            if (expected.contains(key)) {
                continue;
            }
            // Python omits absent quantile/date/group tuples. The native cube
            // retains those cells and represents them with NaN.
            ASSERT_EQ(std::get<0>(key), "quantile_by_date");
            EXPECT_TRUE(std::isnan(values.first));
            EXPECT_TRUE(std::isnan(values.second));
        }
    }

    const auto aggregate_ic =
        fincore::factor_information_coefficient(prepared, false, false);
    EXPECT_TRUE(aggregate_ic.groups.empty());
    EXPECT_EQ(aggregate_ic.dates.size(), 4U);
    const auto aggregate_quantiles =
        fincore::mean_return_by_quantile(prepared, false, false, true, false);
    EXPECT_TRUE(aggregate_quantiles.groups.empty());
    EXPECT_EQ(aggregate_quantiles.quantiles,
              (std::vector<int>{1, 2, 3}));

    auto missing_group = prepared;
    for (auto& row : missing_group.rows) {
        row.has_group = false;
    }
    EXPECT_THROW(static_cast<void>(fincore::factor_information_coefficient(
                     missing_group, false, true)),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::mean_return_by_quantile(
                     missing_group, false, true, true, false)),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::mean_return_by_quantile_by_date(
                     missing_group, true, false, true)),
                 fincore::factor_data_error);
    auto partial_group = prepared;
    partial_group.rows[0].has_group = false;
    EXPECT_THROW(static_cast<void>(fincore::factor_information_coefficient(
                     partial_group, false, true)),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::mean_return_by_quantile(
                     partial_group, false, true, true, false)),
                 fincore::factor_data_error);
    EXPECT_THROW(static_cast<void>(fincore::mean_return_by_quantile_by_date(
                     partial_group, true, false, true)),
                 fincore::factor_data_error);
}

TEST(FactorPerformance, TurnoverAndRankAutocorrelationMatchOracle) {
    const auto prepared = prepare();
    const auto turnover = fincore::quantile_turnover(prepared, 1, 1);
    ASSERT_EQ(turnover.dates.size(), 60U);
    EXPECT_TRUE(std::isnan(turnover.values[0]));
    EXPECT_DOUBLE_EQ(turnover.values[1], 0.0);
    EXPECT_DOUBLE_EQ(turnover.values[10], 0.5);
    EXPECT_DOUBLE_EQ(turnover.values[58], 0.0);

    const auto autocorrelation =
        fincore::factor_rank_autocorrelation(prepared, 1);
    ASSERT_EQ(autocorrelation.dates.size(), 60U);
    EXPECT_TRUE(std::isnan(autocorrelation.values[0]));
    EXPECT_NEAR(autocorrelation.values[1], 0.9999999999999998, 1e-12);
    EXPECT_NEAR(autocorrelation.values[2], 0.8999999999999998, 1e-12);
}

TEST(FactorPerformance, NonFiniteSuppliedRowAffectsTurnoverMembershipOnly) {
  const auto first_date = fincore::utc_timestamp(2022, 1, 3);
  const auto second_date = first_date + 86'400'000'000'000LL;
  fincore::prepared_factor_data with_nonfinite_member;
  with_nonfinite_member.forward_labels = {"1D"};

  // These rows are supplied directly to the public kernels. Normal factor
  // preparation drops rows with a non-finite factor or forward return.
  const auto append_row =
      [&with_nonfinite_member](const fincore::timestamp date, std::string asset,
                               const double factor, const int quantile,
                               const double forward) {
        fincore::factor_row row;
        row.date = date;
        row.asset = std::move(asset);
        row.factor = factor;
        row.quantile = quantile;
        row.forward_returns = {forward};
        with_nonfinite_member.rows.push_back(std::move(row));
      };
  append_row(first_date, "A", 1.0, 2, 0.01);
  append_row(first_date, "B", 2.0, 1, 0.02);
  append_row(first_date, "C", 3.0, 2, 0.03);
  append_row(second_date, "A", 1.1, 2, 0.01);
  append_row(second_date, "B", 2.1, 1, 0.02);
  append_row(second_date, "C", 2.9, 2, 0.03);
  append_row(second_date, "D", std::numeric_limits<double>::quiet_NaN(), 2,
             std::numeric_limits<double>::quiet_NaN());

  auto without_member = with_nonfinite_member;
  without_member.rows.pop_back();

  const auto included_turnover =
      fincore::quantile_turnover(with_nonfinite_member, 2, 1);
  const auto absent_turnover = fincore::quantile_turnover(without_member, 2, 1);
  EXPECT_EQ(included_turnover.dates,
            (std::vector<fincore::timestamp>{first_date, second_date}));
  EXPECT_EQ(absent_turnover.dates, included_turnover.dates);
  ASSERT_EQ(included_turnover.values.size(), 2U);
  ASSERT_EQ(absent_turnover.values.size(), 2U);
  EXPECT_TRUE(std::isnan(included_turnover.values[0]));
  EXPECT_TRUE(std::isnan(absent_turnover.values[0]));
  EXPECT_NEAR(included_turnover.values[1], 1.0 / 3.0, 1e-15);
  EXPECT_DOUBLE_EQ(absent_turnover.values[1], 0.0);

  const auto included_rank =
      fincore::factor_rank_autocorrelation(with_nonfinite_member, 1);
  const auto absent_rank =
      fincore::factor_rank_autocorrelation(without_member, 1);
  EXPECT_EQ(included_rank.dates, included_turnover.dates);
  EXPECT_EQ(absent_rank.dates, included_rank.dates);
  ASSERT_EQ(included_rank.values.size(), 2U);
  ASSERT_EQ(absent_rank.values.size(), 2U);
  EXPECT_TRUE(std::isnan(included_rank.values[0]));
  EXPECT_TRUE(std::isnan(absent_rank.values[0]));
  EXPECT_DOUBLE_EQ(included_rank.values[1], 1.0);
  EXPECT_DOUBLE_EQ(absent_rank.values[1], 1.0);
}

TEST(FactorPerformance, LongUnbalancedTurnoverMatchesExplicitMembershipOracle) {
  constexpr std::size_t date_count = 325U;
  constexpr int asset_count = 17;
  const auto start = fincore::utc_timestamp(2020, 1, 2);
  const auto day = 86'400'000'000'000LL;
  fincore::prepared_factor_data prepared;
  std::vector<fincore::timestamp> expected_dates;
  std::vector<std::set<std::string>> selected_by_date(date_count);
  expected_dates.reserve(date_count);

  for (std::size_t date_index = 0; date_index < date_count; ++date_index) {
    const auto date = start + static_cast<fincore::timestamp>(date_index) * day;
    expected_dates.push_back(date);

    fincore::factor_row anchor;
    anchor.date = date;
    anchor.asset = "anchor";
    anchor.factor = -1.0;
    anchor.quantile = 1;
    prepared.rows.push_back(std::move(anchor));

    // Leave every twenty-ninth date without this quantile. The other
    // dates have changing, unbalanced set sizes and membership.
    const std::size_t threshold =
        date_index % 29U == 0U ? 0U : 1U + date_index % 8U;
    for (int asset_index = 0; asset_index < asset_count; ++asset_index) {
      const auto residue =
          (date_index * 17U + static_cast<std::size_t>(asset_index) * 7U) % 23U;
      if (residue >= threshold) {
        continue;
      }
      const std::string asset = "asset-" + std::to_string(asset_index);
      selected_by_date[date_index].insert(asset);
      fincore::factor_row row;
      row.date = date;
      row.asset = asset;
      row.factor = static_cast<double>(asset_index);
      row.quantile = 2;
      prepared.rows.push_back(std::move(row));
    }
  }

  for (const int period : {1, 3}) {
    const auto actual = fincore::quantile_turnover(prepared, 2, period);
    ASSERT_EQ(actual.dates, expected_dates);
    ASSERT_EQ(actual.values.size(), date_count);
    std::vector<double> expected(date_count,
                                 std::numeric_limits<double>::quiet_NaN());
    for (std::size_t date_index = static_cast<std::size_t>(period);
         date_index < date_count; ++date_index) {
      const auto &current = selected_by_date[date_index];
      const auto &previous =
          selected_by_date[date_index - static_cast<std::size_t>(period)];
      if (current.empty()) {
        continue;
      }
      std::size_t entrants = 0U;
      for (const auto &asset : current) {
        if (previous.find(asset) == previous.end()) {
          ++entrants;
        }
      }
      expected[date_index] =
          static_cast<double>(entrants) / static_cast<double>(current.size());
    }
    for (std::size_t date_index = 0; date_index < date_count; ++date_index) {
      if (std::isnan(expected[date_index])) {
        EXPECT_TRUE(std::isnan(actual.values[date_index]))
            << "period=" << period << " date_index=" << date_index;
      } else {
        EXPECT_DOUBLE_EQ(actual.values[date_index], expected[date_index])
            << "period=" << period << " date_index=" << date_index;
      }
    }
  }
}

TEST(FactorPerformance, InferenceMatchesOracle) {
  const auto prepared = prepare();
  const auto information = fincore::factor_information_coefficient(prepared);
  std::vector<double> ic;
  for (std::size_t date = 0; date < information.dates.size(); ++date) {
    ic.push_back(information.values[date * 2U]);
  }
  EXPECT_NEAR(fincore::ic_mean(ic), -0.17833333333333332, 1e-12);
  EXPECT_NEAR(fincore::ic_t_stat(ic), -1.9944670862798386, 1e-12);
  const auto interval = fincore::ic_confidence_interval(ic);
  EXPECT_NEAR(interval.lower, -0.35358482569327854, 1e-12);
  EXPECT_NEAR(interval.upper, -0.0030818409733881225, 1e-12);

  const auto result = fincore::information_coefficient_inference(information);
  ASSERT_EQ(result.hypotheses.size(), 2U);
  EXPECT_EQ(result.hypotheses[0].label, "1D");
  EXPECT_EQ(result.hypotheses[0].n_observations, 60U);
  EXPECT_TRUE(result.hypotheses[0].testable);
  EXPECT_NEAR(result.hypotheses[0].mean_ic, -0.17833333333333332, 1e-12);
  EXPECT_NEAR(result.hypotheses[0].t_stat, -1.9944670862798386, 1e-12);
  EXPECT_NEAR(result.hypotheses[0].p_value, 0.050727983191477095, 1e-10);
  EXPECT_NEAR(result.hypotheses[0].adjusted_p_value, 0.10145596638295419,
              1e-10);
  EXPECT_FALSE(result.hypotheses[0].rejected);
  EXPECT_NEAR(result.hypotheses[1].p_value, 0.2602986016902284, 1e-10);
  EXPECT_NEAR(result.hypotheses[1].adjusted_p_value, 0.2602986016902284, 1e-10);

  const std::vector<double> p_values{0.01, 0.04, 0.03, 0.2};
  const auto correction = fincore::benjamini_hochberg(p_values);
  ASSERT_EQ(correction.adjusted_p_values.size(), 4U);
  EXPECT_NEAR(correction.adjusted_p_values[0], 0.04, 1e-15);
  EXPECT_NEAR(correction.adjusted_p_values[1], 0.05333333333333334, 1e-15);
  EXPECT_NEAR(correction.adjusted_p_values[2], 0.05333333333333334, 1e-15);
  EXPECT_NEAR(correction.adjusted_p_values[3], 0.2, 1e-15);
  EXPECT_TRUE(correction.rejected[0]);
  EXPECT_FALSE(correction.rejected[1]);
  EXPECT_FALSE(correction.rejected[2]);
  EXPECT_FALSE(correction.rejected[3]);
}

TEST(FactorPerformance, FamaMacbethMatchesCleanPythonOracle) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> index{start, start + day, start + 2 * day,
                                          start + 3 * day};
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{index});
    returns.load_column<double>("A", {0.010, 0.012, 0.008, 0.011},
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("B", {0.020, 0.018, 0.022, 0.019},
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("C", {0.030, 0.033, 0.028, 0.031},
                                hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe exposures;
    exposures.load_index(std::vector<fincore::timestamp>{index});
    exposures.load_column<double>("A", {1.0, 1.5, 0.5, 1.2},
                                  hmdf::nan_policy::dont_pad_with_nans);
    exposures.load_column<double>("B", {2.0, 2.5, 1.5, 2.2},
                                  hmdf::nan_policy::dont_pad_with_nans);
    exposures.load_column<double>("C", {3.0, 3.5, 2.5, 3.2},
                                  hmdf::nan_policy::dont_pad_with_nans);

    const auto iid = fincore::fama_macbeth(returns, exposures);
    EXPECT_EQ(iid.n_cross_sections, 4U);
    EXPECT_EQ(iid.covariance, fincore::fama_macbeth_covariance::iid);
    EXPECT_NEAR(iid.intercept.mean, -0.0006458333333333327, 1e-15);
    EXPECT_NEAR(iid.intercept.std_error, 0.0019885013782950925, 1e-15);
    EXPECT_NEAR(iid.intercept.t_stat, -0.32478395055831405, 1e-12);
    EXPECT_NEAR(iid.exposure.mean, 0.010125, 1e-15);
    EXPECT_NEAR(iid.exposure.std_error, 0.00012500000000000128, 1e-15);
    EXPECT_NEAR(iid.exposure.t_stat, 80.99999999999918, 1e-12);

    const auto nw1 = fincore::fama_macbeth(
        returns, exposures, fincore::fama_macbeth_covariance::newey_west, 1);
    EXPECT_NEAR(nw1.intercept.std_error, 0.0010145293235111015, 1e-15);
    EXPECT_NEAR(nw1.intercept.t_stat, -0.636584195613214, 1e-12);
    EXPECT_NEAR(nw1.exposure.std_error, 8.267972847077034e-05, 1e-15);
    EXPECT_NEAR(nw1.exposure.t_stat, 122.46048925498683, 1e-9);
    const auto nw2 = fincore::fama_macbeth(
        returns, exposures, fincore::fama_macbeth_covariance::newey_west, 2);
    EXPECT_NEAR(nw2.exposure.std_error, 6.25000000000015e-05, 1e-15);
    EXPECT_NEAR(nw2.exposure.t_stat, 161.9999999999961, 1e-8);

    fincore::dataframe static_exposures;
    static_exposures.load_index(std::vector<fincore::timestamp>{start});
    static_exposures.load_column<double>(
        "A", {1.0}, hmdf::nan_policy::dont_pad_with_nans);
    static_exposures.load_column<double>(
        "B", {2.0}, hmdf::nan_policy::dont_pad_with_nans);
    static_exposures.load_column<double>(
        "C", {3.0}, hmdf::nan_policy::dont_pad_with_nans);
    const auto broadcast =
        fincore::fama_macbeth(returns, static_exposures);
    EXPECT_NEAR(broadcast.intercept.mean, -8.333333333333462e-05, 1e-15);
    EXPECT_NEAR(broadcast.exposure.mean, 0.010125000000000004, 1e-15);
    EXPECT_NEAR(broadcast.exposure.std_error, 0.00012499999999999998,
                1e-15);

    auto missing = exposures;
    missing.load_column<double>(
        "B", {2.0, std::nan(""), 1.5, 2.2},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto skipped = fincore::fama_macbeth(returns, missing);
    EXPECT_NEAR(skipped.intercept.mean, -0.0002708333333333306, 1e-15);
    EXPECT_NEAR(skipped.exposure.mean, 0.010124999999999999, 1e-15);

    EXPECT_THROW(static_cast<void>(fincore::fama_macbeth(
                     returns, exposures,
                     fincore::fama_macbeth_covariance::newey_west, 4)),
                 fincore::factor_data_error);
    fincore::dataframe incomplete;
    incomplete.load_index(std::vector<fincore::timestamp>{index});
    incomplete.load_column<double>("A", {1.0, 1.5, 0.5, 1.2},
                                   hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        static_cast<void>(fincore::fama_macbeth(returns, incomplete)),
        fincore::factor_data_error);
}

TEST(FactorPerformance, CommonStartEventWindowsMatchCleanPythonOracle) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> index;
    for (int offset = 0; offset < 5; ++offset) {
        index.push_back(start + offset * day);
    }
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{index});
    returns.load_column<double>("A", {0.01, 0.02, 0.03, 0.01, 0.04},
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("B", {0.02, 0.01, 0.02, 0.03, 0.01},
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("C", {0.03, 0.04, 0.01, 0.02, 0.03},
                                hmdf::nan_policy::dont_pad_with_nans);

    fincore::factor_series plain;
    plain.dates = {index[1], index[1], index[2]};
    plain.assets = {"A", "B", "C"};
    plain.values = {1.0, 1.0, 2.0};
    const auto raw = fincore::common_start_returns(plain, returns, 1, 2,
                                                   true, false);
    EXPECT_EQ(raw.offsets, (std::vector<int>{-1, 0, 1, 2}));
    EXPECT_EQ(raw.event_dates.size(), 2U);
    EXPECT_EQ(raw.cells.size(), 12U);
    const auto find_cell = [](const fincore::event_window_result& window,
                              const int offset, const fincore::timestamp date,
                              const std::string& asset) {
        for (const auto& cell : window.cells) {
            if (cell.offset == offset && cell.event_date == date &&
                cell.asset == asset) {
                return cell.value;
            }
        }
        return std::nan("");
    };
    EXPECT_DOUBLE_EQ(find_cell(raw, -1, index[1], "A"), 0.01);
    EXPECT_DOUBLE_EQ(find_cell(raw, -1, index[1], "B"), 0.02);
    EXPECT_DOUBLE_EQ(find_cell(raw, 2, index[1], "A"), 0.01);
    EXPECT_DOUBLE_EQ(find_cell(raw, -1, index[2], "C"), 0.04);

    const auto mean = fincore::common_start_returns(plain, returns, 1, 2,
                                                    true, true);
    EXPECT_EQ(mean.cells.size(), 8U);
    EXPECT_DOUBLE_EQ(find_cell(mean, -1, index[1], ""), 0.015);
    EXPECT_DOUBLE_EQ(find_cell(mean, -1, index[2], ""), 0.04);

    fincore::factor_series event;
    event.dates = {index[1]};
    event.assets = {"C"};
    event.values = {2.0};
    fincore::factor_series demean;
    demean.dates = {index[1], index[1]};
    demean.assets = {"A", "B"};
    demean.values = {1.0, 1.0};
    const auto demeaned = fincore::common_start_returns(
        event, returns, 1, 2, true, true, &demean);
    EXPECT_DOUBLE_EQ(find_cell(demeaned, -1, index[1], ""), 0.015);
    EXPECT_DOUBLE_EQ(find_cell(demeaned, 0, index[1], ""), 0.025);
    EXPECT_NEAR(find_cell(demeaned, 1, index[1], ""), -0.015000000000000001,
                1e-15);
    const auto wealth = fincore::common_start_returns(
        event, returns, 1, 2, false, true, nullptr);
    EXPECT_NEAR(find_cell(wealth, -1, index[1], ""), 1.03, 1e-15);
    EXPECT_NEAR(find_cell(wealth, 0, index[1], ""), 1.0712, 1e-15);
    EXPECT_NEAR(find_cell(wealth, 2, index[1], ""), 1.10355024, 1e-15);

    fincore::prepared_factor_data prepared;
    for (const auto& entry :
         {std::tuple{index[1], "A", 1}, std::tuple{index[1], "B", 1},
          std::tuple{index[1], "C", 2}, std::tuple{index[2], "C", 2}}) {
        fincore::factor_row row;
        row.date = std::get<0>(entry);
        row.asset = std::get<1>(entry);
        row.factor = static_cast<double>(std::get<2>(entry));
        row.quantile = std::get<2>(entry);
        prepared.rows.push_back(row);
    }
    const auto statistics =
        fincore::average_cumulative_return_by_quantile(prepared, returns, 1, 2,
                                                       false);
    ASSERT_EQ(statistics.size(), 2U);
    EXPECT_EQ(statistics[0].quantile, 1);
    EXPECT_EQ(statistics[0].offsets, (std::vector<int>{-1, 0, 1, 2}));
    EXPECT_NEAR(statistics[0].means[0], 0.015, 1e-15);
    EXPECT_NEAR(statistics[0].means[3], 0.02, 1e-15);
    EXPECT_TRUE(std::isnan(statistics[0].standard_deviations[0]));
    EXPECT_NEAR(statistics[1].means[0], 0.035, 1e-15);
    EXPECT_NEAR(statistics[1].standard_deviations[0], 0.007071067811865477,
                1e-15);
    const auto demeaned_statistics =
        fincore::average_cumulative_return_by_quantile(prepared, returns, 1, 2,
                                                       true);
    EXPECT_NEAR(demeaned_statistics[0].means[0], -0.005, 1e-15);
    EXPECT_NEAR(demeaned_statistics[0].means[1], -0.008333333333333335,
                1e-15);
    EXPECT_NEAR(demeaned_statistics[1].means[0], 0.004999999999999999,
                1e-15);
    EXPECT_NEAR(demeaned_statistics[1].standard_deviations[0],
                0.007071067811865474, 1e-15);
}

}  // namespace
