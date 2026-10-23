#include <fincore/calendar.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/performance.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
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

TEST(FactorPerformance, InferenceMatchesOracle) {
    const auto prepared = prepare();
    const auto information =
        fincore::factor_information_coefficient(prepared);
    std::vector<double> ic;
    for (std::size_t date = 0; date < information.dates.size(); ++date) {
        ic.push_back(information.values[date * 2U]);
    }
    EXPECT_NEAR(fincore::ic_mean(ic), -0.17833333333333332, 1e-12);
    EXPECT_NEAR(fincore::ic_t_stat(ic), -1.9944670862798386, 1e-12);
    const auto interval = fincore::ic_confidence_interval(ic);
    EXPECT_NEAR(interval.lower, -0.35358482569327854, 1e-12);
    EXPECT_NEAR(interval.upper, -0.0030818409733881225, 1e-12);

    const auto result = fincore::information_coefficient_inference(
        information);
    ASSERT_EQ(result.hypotheses.size(), 2U);
    EXPECT_EQ(result.hypotheses[0].label, "1D");
    EXPECT_EQ(result.hypotheses[0].n_observations, 60U);
    EXPECT_TRUE(result.hypotheses[0].testable);
    EXPECT_NEAR(result.hypotheses[0].mean_ic, -0.17833333333333332, 1e-12);
    EXPECT_NEAR(result.hypotheses[0].t_stat, -1.9944670862798386, 1e-12);
    EXPECT_NEAR(result.hypotheses[0].p_value, 0.050727983191477095, 1e-10);
    EXPECT_NEAR(result.hypotheses[0].adjusted_p_value,
                0.10145596638295419, 1e-10);
    EXPECT_FALSE(result.hypotheses[0].rejected);
    EXPECT_NEAR(result.hypotheses[1].p_value, 0.2602986016902284, 1e-10);
    EXPECT_NEAR(result.hypotheses[1].adjusted_p_value, 0.2602986016902284,
                1e-10);

    const std::vector<double> p_values{0.01, 0.04, 0.03, 0.2};
    const auto correction = fincore::benjamini_hochberg(p_values);
    ASSERT_EQ(correction.adjusted_p_values.size(), 4U);
    EXPECT_NEAR(correction.adjusted_p_values[0], 0.04, 1e-15);
    EXPECT_NEAR(correction.adjusted_p_values[1], 0.05333333333333334,
                1e-15);
    EXPECT_NEAR(correction.adjusted_p_values[2], 0.05333333333333334,
                1e-15);
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

}  // namespace
