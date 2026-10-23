#include <fincore/calendar.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/exceptions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

fincore::factor_series factor_fixture() {
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;
    fincore::factor_series factor;
    const std::vector<std::vector<double>> values{
        {0.1, -0.2, 0.3}, {0.2, -0.1, 0.4}, {0.3, 0.0, 0.5},
        {0.4, 0.1, 0.6}, {0.5, 0.2,
                         std::numeric_limits<double>::quiet_NaN()}};
    const char* assets[] = {"A", "B", "C"};
    for (int date = 0; date < 5; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            factor.dates.push_back(start + date * day);
            factor.assets.push_back(assets[asset]);
            factor.values.push_back(
                values[static_cast<std::size_t>(date)]
                      [static_cast<std::size_t>(asset)]);
        }
    }
    return factor;
}

fincore::dataframe price_fixture() {
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    const auto day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 8; ++position) {
        index.push_back(start + position * day);
    }
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>(
        "A", std::vector<double>{100, 101, 102, 103, 104, 105, 106, 107},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "B", std::vector<double>{100, 99, 98, 97, 96, 95, 94, 93},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "C", std::vector<double>{50, 51, 52, 53, 54, 55, 56, 57},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::factor_grouping fixture_grouping() {
    fincore::factor_grouping grouping;
    grouping.group_by_asset = {{"A", "x"}, {"B", "x"}, {"C", "y"}};
    return grouping;
}

fincore::prepare_factor_config prepare_config() {
    fincore::prepare_factor_config config;
    config.periods = {1, 2};
    config.quantiles = 3;
    config.max_loss = 0.5;
    config.grouping = fixture_grouping();
    return config;
}

TEST(FactorData, ForwardReturnsMatchOracle) {
    const auto forward = fincore::compute_forward_returns(
        factor_fixture(), price_fixture(), {1, 2});
    ASSERT_EQ(forward.labels.size(), 2U);
    EXPECT_EQ(forward.labels[0], "1D");
    EXPECT_EQ(forward.labels[1], "2D");
    ASSERT_EQ(forward.dates.size(), 15U);
    EXPECT_NEAR(forward.values[0][0], 0.010000000000000009, 1e-15);
    EXPECT_NEAR(forward.values[1][0], 0.020000000000000018, 1e-15);
    EXPECT_NEAR(forward.values[0][1], -0.010000000000000009, 1e-15);
    EXPECT_NEAR(forward.values[0][2], 0.020000000000000018, 1e-15);
    EXPECT_NEAR(forward.values[0][14], 0.0185185185185186, 1e-15);
    EXPECT_NEAR(forward.values[1][14], 0.03703703703703698, 1e-15);
    EXPECT_NEAR(forward.values[0][12], 0.009615384615384581, 1e-15);
}

TEST(FactorData, PrepareFactorDataMatchesOracle) {
    const auto prepared = fincore::prepare_factor_data(
        factor_fixture(), price_fixture(), prepare_config());
    ASSERT_EQ(prepared.rows.size(), 14U);
    ASSERT_EQ(prepared.forward_labels.size(), 2U);
    EXPECT_EQ(prepared.rows.front().date, fincore::utc_timestamp(2021, 1, 4));
    EXPECT_EQ(prepared.rows.front().asset, "A");
    EXPECT_NEAR(prepared.rows.front().factor, 0.1, 1e-15);
    EXPECT_EQ(prepared.rows.front().quantile, 2);
    EXPECT_TRUE(prepared.rows.front().has_group);
    EXPECT_EQ(prepared.rows.front().group, "x");
    EXPECT_NEAR(prepared.rows.front().forward_returns[0],
                0.010000000000000009, 1e-15);
    EXPECT_EQ(prepared.rows[1].asset, "B");
    EXPECT_EQ(prepared.rows[1].quantile, 1);
    EXPECT_EQ(prepared.rows[2].asset, "C");
    EXPECT_EQ(prepared.rows[2].quantile, 3);
    EXPECT_EQ(prepared.rows[2].group, "y");
    EXPECT_EQ(prepared.rows.back().date, fincore::utc_timestamp(2021, 1, 8));
    EXPECT_EQ(prepared.rows.back().asset, "B");
    EXPECT_EQ(prepared.rows.back().quantile, 1);
    EXPECT_NEAR(prepared.rows.back().forward_returns[0],
                -0.01041666666666663, 1e-15);
    EXPECT_EQ(prepared.loss_report.input_count, 15U);
    EXPECT_EQ(prepared.loss_report.finite_factor_count, 14U);
    EXPECT_EQ(prepared.loss_report.forward_returns_count, 14U);
    EXPECT_EQ(prepared.loss_report.binning_count, 14U);
    EXPECT_NEAR(prepared.loss_report.factor_input_loss,
                0.06666666666666667, 1e-15);
    EXPECT_NEAR(prepared.loss_report.forward_returns_loss, 0.0, 1e-15);
    EXPECT_NEAR(prepared.loss_report.binning_loss, 0.0, 1e-15);
    EXPECT_NEAR(prepared.loss_report.total_loss, 0.06666666666666667, 1e-15);
}

TEST(FactorData, PrepareByHorizonQuantizesBeforeAvailability) {
    auto config = prepare_config();
    config.grouping = {};
    const auto result = fincore::prepare_factor_data_by_horizon(
        factor_fixture(), price_fixture(), config);
    ASSERT_EQ(result.by_horizon.size(), 2U);
    EXPECT_EQ(result.by_horizon[0].first, "1D");
    EXPECT_EQ(result.by_horizon[1].first, "2D");
    for (const auto& entry : result.by_horizon) {
        ASSERT_EQ(entry.second.rows.size(), 14U);
        EXPECT_EQ(entry.second.forward_labels.size(), 1U);
        EXPECT_EQ(entry.second.rows[0].quantile, 2);
        EXPECT_EQ(entry.second.rows[1].quantile, 1);
        EXPECT_EQ(entry.second.rows[2].quantile, 3);
        EXPECT_EQ(entry.second.rows.back().asset, "B");
        EXPECT_EQ(entry.second.rows.back().quantile, 1);
        EXPECT_EQ(entry.second.loss_report.input_count, 15U);
        EXPECT_EQ(entry.second.loss_report.finite_factor_count, 14U);
        EXPECT_EQ(entry.second.loss_report.forward_returns_count, 14U);
        EXPECT_EQ(entry.second.loss_report.binning_count, 14U);
    }

    auto filtered = config;
    filtered.filter_zscore = 2.0;
    EXPECT_THROW(
        static_cast<void>(fincore::prepare_factor_data_by_horizon(
            factor_fixture(), price_fixture(), filtered)),
        fincore::factor_data_error);
}

TEST(FactorData, PrepareFromForwardReturnsMatchesPreparedRoute) {
    const auto forward = fincore::compute_forward_returns(
        factor_fixture(), price_fixture(), {1, 2});
    auto config = prepare_config();
    config.grouping = {};
    const auto prepared = fincore::prepare_factor_data_from_forward_returns(
        factor_fixture(), forward, config);
    ASSERT_EQ(prepared.rows.size(), 14U);
    EXPECT_EQ(prepared.rows.front().quantile, 2);
    EXPECT_TRUE(!prepared.rows.front().has_group);
    EXPECT_EQ(prepared.forward_labels.size(), 2U);
    EXPECT_NEAR(prepared.rows.front().forward_returns[1],
                0.020000000000000018, 1e-15);
}

TEST(FactorData, QuantizationMatchesPandasQcutAndCut) {
    fincore::factor_series factor;
    const auto date = fincore::utc_timestamp(2022, 1, 3);
    const double values[] = {1.0, 2.0, 2.0, 3.0, 4.0};
    const char* assets[] = {"A", "B", "C", "D", "E"};
    for (int index = 0; index < 5; ++index) {
        factor.dates.push_back(date);
        factor.assets.push_back(assets[index]);
        factor.values.push_back(values[index]);
    }
    fincore::quantization_config qcut;
    qcut.quantiles = 4;
    EXPECT_THROW(static_cast<void>(
                     fincore::quantize_factor(factor, qcut)),
                 fincore::factor_data_error);
    qcut.quantiles = 3;
    auto labels = fincore::quantize_factor(factor, qcut);
    ASSERT_EQ(labels.size(), 5U);
    EXPECT_EQ(*labels[0], 1);
    EXPECT_EQ(*labels[1], 1);
    EXPECT_EQ(*labels[2], 1);
    EXPECT_EQ(*labels[3], 3);
    EXPECT_EQ(*labels[4], 3);

    fincore::quantization_config cut;
    cut.use_bins = true;
    cut.quantiles = 3;
    labels = fincore::quantize_factor(factor, cut);
    ASSERT_EQ(labels.size(), 5U);
    EXPECT_EQ(*labels[0], 1);
    EXPECT_EQ(*labels[1], 1);
    EXPECT_EQ(*labels[2], 1);
    EXPECT_EQ(*labels[3], 2);
    EXPECT_EQ(*labels[4], 3);

    fincore::factor_series zero_aware;
    const double signed_values[] = {-3.0, -2.0, -1.0, 1.0, 2.0, 3.0};
    const char* signed_assets[] = {"A", "B", "C", "D", "E", "F"};
    for (int index = 0; index < 6; ++index) {
        zero_aware.dates.push_back(date);
        zero_aware.assets.push_back(signed_assets[index]);
        zero_aware.values.push_back(signed_values[index]);
    }
    fincore::quantization_config aware;
    aware.quantiles = 4;
    aware.zero_aware = true;
    labels = fincore::quantize_factor(zero_aware, aware);
    ASSERT_EQ(labels.size(), 6U);
    EXPECT_EQ(*labels[0], 1);
    EXPECT_EQ(*labels[1], 1);
    EXPECT_EQ(*labels[2], 2);
    EXPECT_EQ(*labels[3], 3);
    EXPECT_EQ(*labels[4], 3);
    EXPECT_EQ(*labels[5], 4);
}

TEST(FactorData, AllEqualAndLossFailures) {
    fincore::factor_series constant;
    const auto date = fincore::utc_timestamp(2022, 1, 3);
    for (int index = 0; index < 3; ++index) {
        constant.dates.push_back(date);
        constant.assets.push_back(std::string(1, static_cast<char>('A' + index)));
        constant.values.push_back(1.0);
    }
    fincore::quantization_config config;
    config.quantiles = 3;
    EXPECT_THROW(static_cast<void>(
                     fincore::quantize_factor(constant, config)),
                 fincore::factor_data_error);
    config.no_raise = true;
    const auto labels = fincore::quantize_factor(constant, config);
    ASSERT_EQ(labels.size(), 3U);
    EXPECT_FALSE(labels[0].has_value());
    EXPECT_FALSE(labels[1].has_value());
    EXPECT_FALSE(labels[2].has_value());

    auto prepare = prepare_config();
    prepare.max_loss = 0.01;
    EXPECT_THROW(static_cast<void>(fincore::prepare_factor_data(
                     factor_fixture(), price_fixture(), prepare)),
                 fincore::factor_loss_exceeded);
}

TEST(FactorData, RejectsInvalidFactorAndPriceInputs) {
    auto factor = factor_fixture();
    factor.assets[1] = factor.assets[0];
    EXPECT_THROW(static_cast<void>(fincore::compute_forward_returns(
                     factor, price_fixture())),
                 fincore::factor_data_error);

    auto missing_asset = factor_fixture();
    missing_asset.assets[0] = "Z";
    EXPECT_THROW(static_cast<void>(fincore::compute_forward_returns(
                     missing_asset, price_fixture())),
                 fincore::factor_data_error);

    auto no_shared = factor_fixture();
    for (auto& date : no_shared.dates) {
        date += 10'000'000'000'000LL;
    }
    EXPECT_THROW(static_cast<void>(fincore::compute_forward_returns(
                     no_shared, price_fixture())),
                 fincore::factor_data_error);

    auto bad_periods = std::vector<int>{0};
    EXPECT_THROW(static_cast<void>(fincore::compute_forward_returns(
                     factor_fixture(), price_fixture(), bad_periods)),
                 fincore::factor_data_error);
}

}  // namespace
