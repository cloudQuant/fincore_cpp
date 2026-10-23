#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/portfolio.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace {

fincore::prepared_factor_data tiny_prepared() {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D", "3D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    for (int date = 0; date < 3; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset + 1;
            row.forward_returns = {0.01 * (asset + 1),
                                   0.03 * (asset + 1)};
            data.rows.push_back(row);
        }
    }
    return data;
}

const fincore::factor_weight_row* find_weight(
    const std::vector<fincore::factor_weight_row>& rows,
    const fincore::timestamp date, const std::string& asset) {
    for (const auto& row : rows) {
        if (row.date == date && row.asset == asset) {
            return &row;
        }
    }
    return nullptr;
}

TEST(FactorPortfolio, BuildsOverlappingGrossNormalizedPositions) {
    const auto data = tiny_prepared();
    const auto raw = fincore::factor_positions(data, "3D");
    // 2022-01-03..05 trades plus 01-06, 01-07 and 01-10 business-day expiries.
    ASSERT_EQ(raw.size(), 18U);
    const auto day = 86'400'000'000'000LL;
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    double gross = 0.0;
    for (const auto& row : raw) {
        if (row.date == start) {
            gross += std::abs(row.weight);
        }
    }
    EXPECT_NEAR(gross, 1.0, 1e-15);
    const auto* a = find_weight(raw, start, "A");
    const auto* b = find_weight(raw, start, "B");
    const auto* c = find_weight(raw, start, "C");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_DOUBLE_EQ(a->weight, -0.5);
    EXPECT_DOUBLE_EQ(b->weight, 0.0);
    EXPECT_DOUBLE_EQ(c->weight, 0.5);

    const auto expiry = fincore::utc_timestamp(2022, 1, 10);
    const auto* expired_a = find_weight(raw, expiry, "A");
    const auto* expired_c = find_weight(raw, expiry, "C");
    ASSERT_NE(expired_a, nullptr);
    ASSERT_NE(expired_c, nullptr);
    EXPECT_DOUBLE_EQ(expired_a->weight, 0.0);
    EXPECT_DOUBLE_EQ(expired_c->weight, 0.0);

    const auto cumulative =
        fincore::factor_cumulative_returns(data, "1D");
    ASSERT_EQ(cumulative.dates.size(), 3U);
    EXPECT_NEAR(cumulative.values[0], 1.01, 1e-15);
    EXPECT_NEAR(cumulative.values[1], 1.0201, 1e-15);
    EXPECT_NEAR(cumulative.values[2], 1.030301, 1e-15);

    const auto inputs = fincore::build_factor_portfolio_inputs(data, "1D");
    ASSERT_EQ(inputs.returns.dates.size(), 3U);
    EXPECT_DOUBLE_EQ(inputs.returns.values[0], 0.0);
    EXPECT_NEAR(inputs.returns.values[1], 0.01, 1e-15);
    EXPECT_NEAR(inputs.returns.values[2], 0.01, 1e-15);

    // Four calendar days (01-03..01-06) across three assets plus cash.
    ASSERT_EQ(inputs.positions.size(), 16U);

    const auto* start_a = find_weight(inputs.positions, start, "A");
    const auto* start_cash = find_weight(inputs.positions, start, "cash");
    ASSERT_NE(start_a, nullptr);
    ASSERT_NE(start_cash, nullptr);
    EXPECT_DOUBLE_EQ(start_a->weight, -0.5);
    EXPECT_DOUBLE_EQ(start_cash->weight, 1.0);
    const auto last_position_day = fincore::utc_timestamp(2022, 1, 6);
    const auto* last_a =
        find_weight(inputs.positions, last_position_day, "A");
    const auto* last_cash =
        find_weight(inputs.positions, last_position_day, "cash");
    ASSERT_NE(last_a, nullptr);
    ASSERT_NE(last_cash, nullptr);
    EXPECT_DOUBLE_EQ(last_a->weight, 0.0);
    EXPECT_DOUBLE_EQ(last_cash->weight, 1.0);

    ASSERT_TRUE(inputs.benchmark_returns.has_value());
    ASSERT_EQ(inputs.benchmark_returns->dates.size(), 3U);
    EXPECT_DOUBLE_EQ(inputs.benchmark_returns->values[0], 0.0);
    EXPECT_NEAR(inputs.benchmark_returns->values[1], 0.02, 1e-15);
    EXPECT_NEAR(inputs.benchmark_returns->values[2], 0.02, 1e-15);

    EXPECT_THROW(
        static_cast<void>(fincore::factor_positions(data, "5D")),
        fincore::factor_data_error);
    EXPECT_THROW(
        static_cast<void>(fincore::factor_positions(data, "1H")),
        fincore::factor_data_error);
    EXPECT_THROW(
        static_cast<void>(fincore::factor_cumulative_returns(
            data, "1D", true, false, false, {9})),
        fincore::factor_data_error);
}

TEST(FactorPortfolio, RollsExpiryOffWeekends) {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D"};
    const auto friday = fincore::utc_timestamp(2022, 1, 7);
    for (int asset = 0; asset < 2; ++asset) {
        fincore::factor_row row;
        row.date = friday;
        row.asset = asset == 0 ? "A" : "B";
        row.factor = static_cast<double>(asset + 1);
        row.quantile = asset + 1;
        row.forward_returns = {0.01};
        data.rows.push_back(row);
    }
    const auto raw = fincore::factor_positions(data, "1D");
    ASSERT_EQ(raw.size(), 4U);
    EXPECT_EQ(raw.front().date, friday);
    EXPECT_EQ(raw[2].date, fincore::utc_timestamp(2022, 1, 10));
    EXPECT_DOUBLE_EQ(raw[2].weight, 0.0);
    EXPECT_DOUBLE_EQ(raw[3].weight, 0.0);
}

}  // namespace
