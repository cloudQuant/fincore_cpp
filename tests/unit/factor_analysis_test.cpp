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
    EXPECT_TRUE(std::isnan(model.quantile_turnover[0].values[0]));
    ASSERT_EQ(model.rank_autocorrelation.dates.size(), 60U);
    EXPECT_NEAR(model.rank_autocorrelation.values[1],
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
