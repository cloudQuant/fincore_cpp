#include <fincore/calendar.hpp>
#include <fincore/portfolio/round_trips.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <vector>

namespace {

constexpr fincore::timestamp jan_2_2024_09 = 1704186000000000000LL;
constexpr auto hour = std::chrono::hours{1};
constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double infinity = std::numeric_limits<double>::infinity();

using namespace std::chrono_literals;

fincore::timestamp at(const std::chrono::nanoseconds offset) {
    return jan_2_2024_09 + offset.count();
}

void expect_double_values(const std::vector<double>& actual,
                          const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t position = 0U; position < expected.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(actual[position])) << "position " << position;
        } else if (std::isinf(expected[position])) {
            EXPECT_EQ(actual[position], expected[position]) << "position " << position;
        } else {
            EXPECT_NEAR(actual[position], expected[position], 1.0e-12)
                << "position " << position;
        }
    }
}

void expect_schema(
    const fincore::dataframe& frame,
    const std::vector<std::pair<std::string_view, std::type_index>>& expected) {
    const auto columns =
        frame.get_columns_info<double, std::string, bool, fincore::timestamp>();
    ASSERT_EQ(columns.size(), expected.size());
    EXPECT_EQ(frame.shape().second, expected.size());
    for (std::size_t position = 0U; position < expected.size(); ++position) {
        EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                     std::string{expected[position].first}.c_str());
        EXPECT_EQ(std::get<2>(columns[position]), expected[position].second);
    }
}

fincore::dataframe transactions(
    std::vector<fincore::timestamp> index,
    std::vector<std::string> symbols,
    std::vector<double> amounts,
    std::vector<double> prices) {
    fincore::dataframe result;
    result.load_index(std::move(index));
    result.load_column<std::string>("symbol", std::move(symbols),
                                    hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>("amount", std::move(amounts),
                               hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>("price", std::move(prices),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

fincore::dataframe interleaved_transactions() {
    return transactions(
        {at(0ns), at(2 * hour), at(4 * hour), at(6 * hour), at(hour),
         at(3 * hour)},
        {"A", "A", "A", "A", "B", "B"}, {2.0, 2.0, -3.0, -1.0, -2.0, 2.0},
        {10.0, 20.0, 25.0, 5.0, 30.0, 20.0});
}

TEST(RoundTrips, ConsolidatesInterleavedSymbolsIntoTypedBlocks) {
    const auto blocks = fincore::groupby_consecutive_transactions(
        interleaved_transactions());

    EXPECT_EQ(blocks.get_index(),
              (std::vector<fincore::timestamp>{at(0ns), at(4 * hour), at(hour),
                                                at(3 * hour)}));
    expect_schema(blocks, {{"amount", typeid(double)},
                           {"symbol", typeid(std::string)},
                           {"price", typeid(double)}});
    EXPECT_EQ(blocks.get_column<std::string>("symbol"),
              (std::vector<std::string>{"A", "A", "B", "B"}));
    expect_double_values(blocks.get_column<double>("amount"), {4.0, -4.0, -2.0, 2.0});
    expect_double_values(blocks.get_column<double>("price"), {15.0, 20.0, 30.0, 20.0});
}

TEST(RoundTrips, ExtractsFifoRoundTripsWithDocumentedSchema) {
    const auto result = fincore::extract_round_trips(interleaved_transactions());

    EXPECT_EQ(result.get_index(), (std::vector<fincore::timestamp>{0, 1}));
    expect_schema(result, {{"pnl", typeid(double)},
                           {"open_dt", typeid(fincore::timestamp)},
                           {"close_dt", typeid(fincore::timestamp)},
                           {"long", typeid(bool)},
                           {"rt_returns", typeid(double)},
                           {"symbol", typeid(std::string)},
                           {"duration", typeid(fincore::timestamp)}});
    expect_double_values(result.get_column<double>("pnl"), {20.0, 20.0});
    EXPECT_EQ(result.get_column<fincore::timestamp>("open_dt"),
              (std::vector<fincore::timestamp>{at(0ns), at(hour)}));
    EXPECT_EQ(result.get_column<fincore::timestamp>("close_dt"),
              (std::vector<fincore::timestamp>{at(4 * hour), at(3 * hour)}));
    EXPECT_EQ(result.get_column<bool>("long"), (std::vector<bool>{true, false}));
    expect_double_values(result.get_column<double>("rt_returns"),
                         {1.0 / 3.0, 1.0 / 3.0});
    EXPECT_EQ(result.get_column<std::string>("symbol"),
              (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(result.get_column<fincore::timestamp>("duration"),
              (std::vector<fincore::timestamp>{
                  std::chrono::duration_cast<std::chrono::nanoseconds>(4 * hour).count(),
                  std::chrono::duration_cast<std::chrono::nanoseconds>(2 * hour).count()}));
}

TEST(RoundTrips, HonorsMaxDeltaAndStableTimestampTieOrdering) {
    const auto blocks = fincore::groupby_consecutive_transactions(transactions(
        {at(0ns), at(8 * hour), at(16 * hour + 1ns), at(24 * hour), at(24 * hour)},
        {"A", "A", "A", "B", "B"}, {1.0, 1.0, 1.0, 1.0, -1.0},
        {10.0, 20.0, 30.0, 40.0, 50.0}));

    EXPECT_EQ(blocks.get_index(),
              (std::vector<fincore::timestamp>{at(0ns), at(16 * hour + 1ns),
                                                at(24 * hour), at(24 * hour)}));
    EXPECT_EQ(blocks.get_column<std::string>("symbol"),
              (std::vector<std::string>{"A", "A", "B", "B"}));
    expect_double_values(blocks.get_column<double>("amount"), {2.0, 1.0, 1.0, -1.0});
    expect_double_values(blocks.get_column<double>("price"), {15.0, 30.0, 40.0, 50.0});

    const auto negative_delta = fincore::groupby_consecutive_transactions(
        transactions({at(0ns), at(0ns)}, {"A", "A"}, {1.0, 1.0}, {10.0, 20.0}),
        -1ns);
    EXPECT_EQ(negative_delta.get_index(),
              (std::vector<fincore::timestamp>{at(0ns), at(0ns)}));
    expect_double_values(negative_delta.get_column<double>("price"), {10.0, 20.0});
}

TEST(RoundTrips, HandlesCrossingsAndPartialFillsAsSeparateEvents) {
    const auto crossing = fincore::extract_round_trips(transactions(
        {at(0ns), at(24 * hour), at(48 * hour)}, {"A", "A", "A"},
        {2.0, -4.0, 3.0}, {10.0, 15.0, 20.0}));
    expect_double_values(crossing.get_column<double>("pnl"), {10.0, -10.0});
    EXPECT_EQ(crossing.get_column<bool>("long"), (std::vector<bool>{true, false}));
    expect_double_values(crossing.get_column<double>("rt_returns"), {0.5, -1.0 / 3.0});

    const auto partial = fincore::extract_round_trips(transactions(
        {at(0ns), at(hour), at(10 * hour)}, {"A", "A", "A"}, {3.0, -1.0, -2.0},
        {10.0, 12.0, 12.0}));
    EXPECT_EQ(partial.get_index(), (std::vector<fincore::timestamp>{0, 1}));
    expect_double_values(partial.get_column<double>("pnl"), {2.0, 4.0});
    EXPECT_EQ(partial.get_column<fincore::timestamp>("close_dt"),
              (std::vector<fincore::timestamp>{at(hour), at(10 * hour)}));
}

TEST(RoundTrips, PreservesZeroAndNonfiniteArithmeticAndSkipsNegativePrices) {
    const auto zero_invested = fincore::extract_round_trips(
        transactions({at(0ns), at(hour)}, {"A", "A"}, {1.0, -1.0}, {0.0, 5.0}));
    expect_double_values(zero_invested.get_column<double>("pnl"), {5.0});
    expect_double_values(zero_invested.get_column<double>("rt_returns"), {0.0});

    const auto negative_price = fincore::extract_round_trips(
        transactions({at(0ns), at(hour)}, {"A", "A"}, {1.0, -1.0}, {-1.0, 5.0}));
    EXPECT_TRUE(negative_price.get_index().empty());

    const auto nonfinite = fincore::groupby_consecutive_transactions(transactions(
        {at(0ns), at(hour), at(2 * hour), at(3 * hour)}, {"A", "A", "B", "B"},
        {1.0, 1.0, 1.0, 1.0}, {nan, 1.0, infinity, 1.0}));
    expect_double_values(nonfinite.get_column<double>("price"), {nan, infinity});
}

TEST(RoundTrips, PreservesPythonSignedZeroVwapAndFifoDirection) {
    const auto zero_sell = transactions({at(0ns), at(9 * hour)}, {"A", "A"},
                                        {1.0, -1.0}, {10.0, 0.0});
    const auto sell_blocks = fincore::groupby_consecutive_transactions(zero_sell);
    const auto& sell_prices = sell_blocks.get_column<double>("price");
    ASSERT_EQ(sell_prices.size(), 2U);
    EXPECT_EQ(sell_prices[1], 0.0);
    EXPECT_TRUE(std::signbit(sell_prices[1]));
    const auto sell_events = fincore::extract_round_trips(zero_sell);
    EXPECT_TRUE(sell_events.get_index().empty());
    expect_schema(sell_events, {{"pnl", typeid(double)},
                                {"open_dt", typeid(fincore::timestamp)},
                                {"close_dt", typeid(fincore::timestamp)},
                                {"long", typeid(bool)},
                                {"rt_returns", typeid(double)},
                                {"symbol", typeid(std::string)},
                                {"duration", typeid(fincore::timestamp)}});

    const auto zero_buy = transactions({at(0ns), at(9 * hour)}, {"A", "A"},
                                       {-1.0, 1.0}, {10.0, 0.0});
    const auto buy_blocks = fincore::groupby_consecutive_transactions(zero_buy);
    const auto& buy_prices = buy_blocks.get_column<double>("price");
    ASSERT_EQ(buy_prices.size(), 2U);
    EXPECT_EQ(buy_prices[1], 0.0);
    EXPECT_FALSE(std::signbit(buy_prices[1]));
    const auto buy_events = fincore::extract_round_trips(zero_buy);
    EXPECT_EQ(buy_events.get_index(), (std::vector<fincore::timestamp>{0}));
    expect_double_values(buy_events.get_column<double>("pnl"), {10.0});
    EXPECT_EQ(buy_events.get_column<bool>("long"), (std::vector<bool>{false}));
    expect_double_values(buy_events.get_column<double>("rt_returns"), {1.0});
}

TEST(RoundTrips, RejectsUnrepresentableDuration) {
    const auto earliest = std::numeric_limits<fincore::timestamp>::min() + 1;
    const auto latest = std::numeric_limits<fincore::timestamp>::max();
    EXPECT_THROW(fincore::extract_round_trips(
                     transactions({earliest, latest}, {"A", "A"}, {1.0, -1.0},
                                  {10.0, 20.0})),
                 fincore::input_error);
}

TEST(RoundTrips, PopulatedOneDirectionInputReturnsTypedEmptyEvents) {
    const auto result = fincore::extract_round_trips(transactions(
        {at(0ns), at(hour), at(10 * hour)}, {"A", "A", "A"}, {1.0, 2.0, 3.0},
        {10.0, 20.0, 30.0}));

    EXPECT_TRUE(result.get_index().empty());
    expect_schema(result, {{"pnl", typeid(double)},
                           {"open_dt", typeid(fincore::timestamp)},
                           {"close_dt", typeid(fincore::timestamp)},
                           {"long", typeid(bool)},
                           {"rt_returns", typeid(double)},
                           {"symbol", typeid(std::string)},
                           {"duration", typeid(fincore::timestamp)}});
}

TEST(RoundTrips, SupportsTypedEmptyCustomColumnsAndZeroVwap) {
    const auto empty = transactions({}, {}, {}, {});
    const auto empty_blocks = fincore::groupby_consecutive_transactions(empty);
    EXPECT_TRUE(empty_blocks.get_index().empty());
    expect_schema(empty_blocks, {{"amount", typeid(double)},
                                  {"symbol", typeid(std::string)},
                                  {"price", typeid(double)}});

    const auto empty_events = fincore::extract_round_trips(empty);
    EXPECT_TRUE(empty_events.get_index().empty());
    expect_schema(empty_events, {{"pnl", typeid(double)},
                                  {"open_dt", typeid(fincore::timestamp)},
                                  {"close_dt", typeid(fincore::timestamp)},
                                  {"long", typeid(bool)},
                                  {"rt_returns", typeid(double)},
                                  {"symbol", typeid(std::string)},
                                  {"duration", typeid(fincore::timestamp)}});

    auto custom = transactions({at(0ns), at(hour)}, {"A", "A"}, {1.0, 1.0},
                               {10.0, -10.0});
    custom.load_column<std::string>("ticker", custom.get_column<std::string>("symbol"),
                                    hmdf::nan_policy::dont_pad_with_nans);
    custom.load_column<double>("quantity", custom.get_column<double>("amount"),
                               hmdf::nan_policy::dont_pad_with_nans);
    custom.load_column<double>("execution", custom.get_column<double>("price"),
                               hmdf::nan_policy::dont_pad_with_nans);
    const auto blocks = fincore::groupby_consecutive_transactions(
        custom, fincore::default_round_trip_max_delta, "ticker", "quantity", "execution");
    expect_schema(blocks, {{"quantity", typeid(double)},
                           {"ticker", typeid(std::string)},
                           {"execution", typeid(double)}});
    EXPECT_EQ(blocks.get_column<std::string>("ticker"),
              (std::vector<std::string>{"A"}));
    expect_double_values(blocks.get_column<double>("quantity"), {2.0});
    expect_double_values(blocks.get_column<double>("execution"), {0.0});

    const auto zero_vwap = fincore::groupby_consecutive_transactions(transactions(
        {at(2 * hour), at(3 * hour)}, {"B", "B"}, {0.0, 0.0}, {10.0, 20.0}));
    expect_double_values(zero_vwap.get_column<double>("price"), {nan});
}

TEST(RoundTrips, RejectsInvalidFieldsAndIgnoresUnrelatedMetadata) {
    auto valid = interleaved_transactions();
    valid.load_column<int>("metadata", std::vector<int>{1, 2, 3, 4, 5, 6},
                           hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_NO_THROW(fincore::groupby_consecutive_transactions(valid));
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "", "amount",
                                                            "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "",
                                                            "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "amount",
                                                            ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "symbol",
                                                            "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "amount",
                                                            "symbol"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "amount",
                                                            "amount"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "absent", "amount",
                                                            "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "absent",
                                                            "price"),
                 fincore::input_error);
    EXPECT_THROW(fincore::groupby_consecutive_transactions(valid, 8 * hour, "symbol", "amount",
                                                            "absent"),
                 fincore::input_error);

    auto wrong_symbol = valid;
    wrong_symbol.load_column<int>("ticker", std::vector<int>{1, 2, 3, 4, 5, 6},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::extract_round_trips(wrong_symbol, 8 * hour, "ticker"),
                 fincore::input_error);

    auto wrong_amount = valid;
    wrong_amount.load_column<int>("quantity", std::vector<int>{1, 2, 3, 4, 5, 6},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::extract_round_trips(wrong_amount, 8 * hour, "symbol", "quantity"),
                 fincore::input_error);

    auto wrong_price = valid;
    wrong_price.load_column<int>("execution", std::vector<int>{1, 2, 3, 4, 5, 6},
                                 hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::extract_round_trips(wrong_price, 8 * hour, "symbol", "amount",
                                              "execution"),
                 fincore::input_error);

    auto malformed = valid;
    malformed.load_column<std::string>("ticker", std::vector<std::string>{"A"},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::extract_round_trips(malformed, 8 * hour, "ticker"),
                 fincore::input_error);
    malformed.load_column<double>("quantity", std::vector<double>{1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::extract_round_trips(malformed, 8 * hour, "symbol", "quantity"),
                 fincore::input_error);
    malformed.load_column<double>("execution", std::vector<double>{1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::extract_round_trips(malformed, 8 * hour, "symbol", "amount",
                                              "execution"),
                 fincore::input_error);
    EXPECT_THROW(fincore::extract_round_trips(valid, 8 * hour, "absent"),
                 fincore::input_error);
}

fincore::dataframe round_trip_stats_fixture() {
    const std::vector<fincore::timestamp> opens{
        fincore::utc_timestamp(2021, 1, 1), fincore::utc_timestamp(2021, 1, 2),
        fincore::utc_timestamp(2021, 1, 3), fincore::utc_timestamp(2021, 1, 4),
        fincore::utc_timestamp(2021, 1, 5), fincore::utc_timestamp(2021, 1, 6)};
    const std::vector<fincore::timestamp> closes{
        fincore::utc_timestamp(2021, 1, 3),  fincore::utc_timestamp(2021, 1, 5),
        fincore::utc_timestamp(2021, 1, 4),  fincore::utc_timestamp(2021, 1, 8),
        fincore::utc_timestamp(2021, 1, 5),  fincore::utc_timestamp(2021, 1, 10)};
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{1, 2, 3, 4, 5, 6});
    frame.load_column<double>("pnl", {100.0, -40.0, 25.0, -10.0, 0.0, 60.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<fincore::timestamp>("open_dt", opens,
                                          hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<fincore::timestamp>("close_dt", closes,
                                          hmdf::nan_policy::dont_pad_with_nans);
    std::vector<bool> long_flags{true, false, true, false, true, false};
    frame.load_column<bool>("long", long_flags,
                            hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("rt_returns",
                              {0.1, -0.05, 0.02, -0.01, 0.0, 0.06},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<std::string>("symbol",
                                   {"AAA", "AAA", "BBB", "BBB", "CCC", "CCC"},
                                   hmdf::nan_policy::dont_pad_with_nans);
    std::vector<fincore::timestamp> durations;
    durations.reserve(6);
    for (std::size_t position = 0; position < 6; ++position) {
        durations.push_back(closes[position] - opens[position]);
    }
    frame.load_column<fincore::timestamp>("duration", durations,
                                          hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(RoundTrips, GenerateRoundTripStatsMatchesPythonOracle) {
    const auto stats = fincore::gen_round_trip_stats(round_trip_stats_fixture());

    ASSERT_EQ(stats.pnl.row_labels.size(), 3U);
    EXPECT_EQ(stats.pnl.row_labels[0], "All trades");
    EXPECT_EQ(stats.pnl.row_labels[1], "short");
    EXPECT_EQ(stats.pnl.row_labels[2], "long");
    ASSERT_EQ(stats.pnl.values.size(), 3U);
    EXPECT_DOUBLE_EQ(stats.pnl.values[0][0], 135.0);
    EXPECT_DOUBLE_EQ(stats.pnl.values[0][1], 185.0);
    EXPECT_DOUBLE_EQ(stats.pnl.values[0][2], -50.0);
    EXPECT_NEAR(stats.pnl.values[0][3], 3.7, 1e-15);
    EXPECT_NEAR(stats.pnl.values[0][4], 22.5, 1e-15);
    EXPECT_TRUE(std::isnan(stats.pnl.values[2][3]));
    EXPECT_DOUBLE_EQ(stats.pnl.values[2][8], 100.0);

    EXPECT_DOUBLE_EQ(stats.summary.values[0][0], 6.0);
    EXPECT_DOUBLE_EQ(stats.summary.values[0][1], 0.5);
    EXPECT_DOUBLE_EQ(stats.summary.values[1][1], 1.0 / 3.0);
    EXPECT_DOUBLE_EQ(stats.summary.values[2][1], 2.0 / 3.0);

    EXPECT_NEAR(stats.duration.values[0][0], 2.3333333333333335, 1e-15);
    EXPECT_NEAR(stats.duration.values[0][1], 2.5, 1e-15);
    EXPECT_DOUBLE_EQ(stats.duration.values[0][2], 4.0);
    EXPECT_DOUBLE_EQ(stats.duration.values[0][3], 0.0);
    EXPECT_DOUBLE_EQ(stats.duration.values[0][4], 2.0 / 3.0);
    EXPECT_DOUBLE_EQ(stats.duration.values[0][5], 14.0);
    EXPECT_NEAR(stats.duration.values[1][0], 3.6666666666666665, 1e-15);
    EXPECT_NEAR(stats.duration.values[1][4], 0.375, 1e-15);

    EXPECT_NEAR(stats.returns.values[0][0], 0.02, 1e-15);
    EXPECT_NEAR(stats.returns.values[0][1], 0.06, 1e-15);
    EXPECT_NEAR(stats.returns.values[0][2], -0.030000000000000002, 1e-15);
    EXPECT_NEAR(stats.returns.values[0][3], 0.01, 1e-15);
    EXPECT_DOUBLE_EQ(stats.returns.values[0][6], 0.1);
    EXPECT_DOUBLE_EQ(stats.returns.values[0][7], -0.05);

    ASSERT_EQ(stats.symbol_labels.size(), 3U);
    EXPECT_EQ(stats.symbol_labels[0], "AAA");
    EXPECT_EQ(stats.symbol_labels[1], "BBB");
    EXPECT_EQ(stats.symbol_labels[2], "CCC");
    ASSERT_EQ(stats.symbol_values.size(), 8U);
    EXPECT_NEAR(stats.symbol_values[0][0], 0.025, 1e-15);
    EXPECT_NEAR(stats.symbol_values[0][1], 0.005, 1e-15);
    EXPECT_NEAR(stats.symbol_values[0][2], 0.03, 1e-15);
    EXPECT_DOUBLE_EQ(stats.symbol_values[7][2], 0.0);

    fincore::dataframe empty;
    empty.load_index(std::vector<fincore::timestamp>{});
    const auto empty_stats = fincore::gen_round_trip_stats(empty);
    EXPECT_TRUE(empty_stats.pnl.row_labels.empty());
    EXPECT_TRUE(empty_stats.pnl.column_labels.empty());
}

}  // namespace
