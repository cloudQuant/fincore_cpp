#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/portfolio.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

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


fincore::timestamp legacy_normalize_day(const fincore::timestamp value) {
    constexpr fincore::timestamp day_ns = 86'400'000'000'000LL;
    fincore::timestamp day = value / day_ns;
    if (value % day_ns < 0) {
        --day;
    }
    return day * day_ns;
}

bool legacy_business_day(const fincore::timestamp day) {
    constexpr fincore::timestamp day_ns = 86'400'000'000'000LL;
    fincore::timestamp weekday = (day / day_ns + 4) % 7;
    if (weekday < 0) {
        weekday += 7;
    }
    return weekday != 0 && weekday != 6;
}

fincore::timestamp legacy_add_business_days(const fincore::timestamp start,
                                            const int days) {
    constexpr fincore::timestamp day_ns = 86'400'000'000'000LL;
    fincore::timestamp day = legacy_normalize_day(start);
    int remaining = days;
    if (!legacy_business_day(day)) {
        while (!legacy_business_day(day)) {
            day += day_ns;
        }
        remaining = days > 0 ? days - 1 : 0;
    }
    while (remaining > 0) {
        day += day_ns;
        if (legacy_business_day(day)) {
            --remaining;
        }
    }
    return day;
}

int legacy_period_days(const std::string_view label) {
    int total = 0;
    std::size_t index = 0;
    while (index < label.size()) {
        int component = 0;
        const std::size_t begin = index;
        while (index < label.size() && label[index] >= '0' &&
               label[index] <= '9') {
            component = component * 10 + (label[index] - '0');
            ++index;
        }
        if (index == begin || index == label.size() || label[index] != 'D') {
            return -1;
        }
        ++index;
        total += component;
    }
    return total;
}

std::vector<fincore::factor_weight_row> legacy_positions(
    const std::string_view label,
    const std::vector<fincore::factor_weight_row>& weights) {
    struct active_trade {
        fincore::timestamp expires;
        std::vector<fincore::factor_weight_row> weights;
    };
    const int days = legacy_period_days(label);
    std::map<fincore::timestamp,
             std::vector<fincore::factor_weight_row>>
        trades;
    std::set<std::string> assets;
    for (const auto& row : weights) {
        trades[row.date].push_back(row);
        assets.insert(row.asset);
    }
    std::vector<active_trade> active;
    std::set<fincore::timestamp> all_dates;
    for (const auto& entry : trades) {
        all_dates.insert(entry.first);
        all_dates.insert(legacy_add_business_days(entry.first, days));
    }
    std::vector<fincore::factor_weight_row> positions;
    for (const fincore::timestamp date : all_dates) {
        const auto trade = trades.find(date);
        if (trade != trades.end()) {
            active.push_back(
                {legacy_add_business_days(date, days), trade->second});
        }
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [date](const active_trade& entry) {
                                        return entry.expires <= date;
                                    }),
                     active.end());
        std::map<std::string, double> totals;
        for (const auto& entry : active) {
            for (const auto& row : entry.weights) {
                totals[row.asset] += row.weight;
            }
        }
        double gross = 0.0;
        for (const auto& entry : totals) {
            gross += std::abs(entry.second);
        }
        for (const auto& asset : assets) {
            const double value = gross > 0.0 ? totals[asset] / gross : 0.0;
            positions.push_back({date, asset, value});
        }
    }
    return positions;
}

void expect_bitwise_positions(
    const std::vector<fincore::factor_weight_row>& actual,
    const std::vector<fincore::factor_weight_row>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        EXPECT_EQ(actual[index].date, expected[index].date) << index;
        EXPECT_EQ(actual[index].asset, expected[index].asset) << index;
        if (std::isnan(expected[index].weight)) {
            EXPECT_TRUE(std::isnan(actual[index].weight)) << index;
        } else {
            std::uint64_t actual_bits = 0;
            std::uint64_t expected_bits = 0;
            std::memcpy(&actual_bits, &actual[index].weight,
                        sizeof(actual_bits));
            std::memcpy(&expected_bits, &expected[index].weight,
                        sizeof(expected_bits));
            EXPECT_EQ(actual_bits, expected_bits) << index;
        }
    }
}

TEST(FactorPortfolio, DensePositionsMatchIndependentLegacyReducer) {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D2D"};
    const fincore::timestamp dates[] = {
        fincore::utc_timestamp(2022, 1, 7),
        fincore::utc_timestamp(2022, 1, 8),
        fincore::utc_timestamp(2022, 1, 10),
        fincore::utc_timestamp(2022, 1, 11)};
    const char* assets[] = {"A", "B", "C"};
    for (int date = 0; date < 4; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            fincore::factor_row row;
            row.date = dates[date];
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset + 1;
            row.forward_returns = {0.01};
            data.rows.push_back(row);
        }
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const std::vector<double> weights{
        0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::vector<fincore::factor_weight_row> raw;
    const double values[4][3] = {
        {0.5, -0.5, nan}, {inf, -1.0, 0.0},
        {-0.0, 0.25, -0.75}, {1.0, 1.0, -2.0}};
    for (int date = 0; date < 4; ++date) {
        for (int asset = 0; asset < 3; ++asset) {
            raw.push_back(
                {dates[date], assets[asset], values[date][asset]});
        }
    }
    static_cast<void>(weights);
    const auto dense = fincore::factor_positions(
        data, "1D2D", true, false, false, {}, {}, raw);
    const auto legacy = legacy_positions("1D2D", raw);
    expect_bitwise_positions(dense, legacy);
}

}  // namespace
