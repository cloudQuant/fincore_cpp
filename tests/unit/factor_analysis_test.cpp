#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/exceptions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
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
