#include <fincore/error.hpp>
#include <fincore/metrics/positions.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

void expect_values(const std::vector<double>& actual,
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

void expect_double_columns(
    const fincore::dataframe& actual,
    const std::vector<std::string_view>& expected) {
    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected.size());
    EXPECT_EQ(actual.shape().second, expected.size());
    for (std::size_t position = 0U; position < expected.size(); ++position) {
        EXPECT_STREQ(std::get<0>(columns[position]).c_str(),
                     std::string{expected[position]}.c_str())
            << "position " << position;
    }
}

fincore::dataframe reference_positions() {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{10, 20, 30});
    positions.load_column<double>("A", std::vector<double>{10.0, -5.0, nan},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<int>("metadata", std::vector<int>{1, 2, 3},
                               hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("B", std::vector<double>{-5.0, -2.0, 3.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{95.0, 100.0, 0.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

fincore::dataframe zero_net_positions() {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{40, 50});
    positions.load_column<double>("A", std::vector<double>{10.0, 10.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{-10.0, 0.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

fincore::dataframe custom_cash_positions() {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{60, 70});
    positions.load_column<double>("asset", std::vector<double>{2.0, -3.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<int>("metadata", std::vector<int>{7, 8},
                               hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("reserve", std::vector<double>{8.0, 9.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

TEST(Positions, PercentAllocationPreservesIndexAndOrderedNumericSchema) {
    const auto actual = fincore::get_percent_alloc(reference_positions());

    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{10, 20, 30}));
    expect_double_columns(actual, {"A", "B", "cash"});
    expect_values(actual.get_column<double>("A"),
                  {.1, -.053763440860215055, nan});
    expect_values(actual.get_column<double>("B"),
                  {-.05, -.021505376344086023, 1.0});
    expect_values(actual.get_column<double>("cash"),
                  {.95, 1.075268817204301, 0.0});
    EXPECT_FALSE(actual.has_column("metadata"));

    const auto zero_net = fincore::get_percent_alloc(zero_net_positions());
    EXPECT_EQ(zero_net.get_index(),
              (std::vector<fincore::timestamp>{40, 50}));
    expect_double_columns(zero_net, {"A", "cash"});
    expect_values(zero_net.get_column<double>("A"), {nan, 1.0});
    expect_values(zero_net.get_column<double>("cash"), {nan, 0.0});
}

TEST(Positions, LongShortExposureExcludesCashAndTreatsNaNAsZero) {
    const auto actual = fincore::get_long_short_pos(reference_positions());

    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{10, 20, 30}));
    expect_double_columns(actual, {"long", "short"});
    expect_values(actual.get_column<double>("long"), {10.0, 0.0, 3.0});
    expect_values(actual.get_column<double>("short"), {5.0, 7.0, 0.0});
}

TEST(Positions, GrossLeverageMatchesFiniteAndZeroNetPythonGoldens) {
    const auto actual = fincore::gross_lev(reference_positions());
    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{10, 20, 30}));
    expect_double_columns(actual, {"gross_lev"});
    expect_values(actual.get_column<double>("gross_lev"),
                  {.15, .07526881720430108, 1.0});

    const auto zero_net = fincore::gross_lev(zero_net_positions());
    EXPECT_EQ(zero_net.get_index(),
              (std::vector<fincore::timestamp>{40, 50}));
    expect_double_columns(zero_net, {"gross_lev"});
    expect_values(zero_net.get_column<double>("gross_lev"), {nan, 1.0});
}

TEST(Positions, ConcentrationMatchesFiniteAndZeroNetPythonGoldens) {
    const auto actual =
        fincore::get_max_median_position_concentration(reference_positions());

    EXPECT_EQ(actual.get_index(),
              (std::vector<fincore::timestamp>{10, 20, 30}));
    expect_double_columns(actual,
                          {"max_long", "median_long", "median_short", "max_short"});
    expect_values(actual.get_column<double>("max_long"), {.1, nan, 1.0});
    expect_values(actual.get_column<double>("median_long"), {.1, nan, 1.0});
    expect_values(actual.get_column<double>("median_short"),
                  {-.05, -.03763440860215054, nan});
    expect_values(actual.get_column<double>("max_short"),
                  {-.05, -.053763440860215055, nan});

    const auto zero_net =
        fincore::get_max_median_position_concentration(zero_net_positions());
    expect_double_columns(zero_net,
                          {"max_long", "median_long", "median_short", "max_short"});
    expect_values(zero_net.get_column<double>("max_long"), {nan, 1.0});
    expect_values(zero_net.get_column<double>("median_long"), {nan, 1.0});
    expect_values(zero_net.get_column<double>("median_short"), {nan, nan});
    expect_values(zero_net.get_column<double>("max_short"), {nan, nan});
}

TEST(Positions, SupportsCustomCashAndAnAbsentLongShortCashName) {
    const auto positions = custom_cash_positions();
    const auto long_short = fincore::get_long_short_pos(positions, "reserve");
    expect_double_columns(long_short, {"long", "short"});
    expect_values(long_short.get_column<double>("long"), {2.0, 0.0});
    expect_values(long_short.get_column<double>("short"), {0.0, 3.0});

    const auto absent_cash = fincore::get_long_short_pos(positions, "absent");
    expect_values(absent_cash.get_column<double>("long"), {10.0, 9.0});
    expect_values(absent_cash.get_column<double>("short"), {0.0, 3.0});

    const auto gross = fincore::gross_lev(positions, "reserve");
    expect_double_columns(gross, {"gross_lev"});
    expect_values(gross.get_column<double>("gross_lev"), {.2, .5});

    const auto concentration =
        fincore::get_max_median_position_concentration(positions, "reserve");
    expect_values(concentration.get_column<double>("max_long"), {.2, nan});
    expect_values(concentration.get_column<double>("median_long"), {.2, nan});
    expect_values(concentration.get_column<double>("median_short"), {nan, -.5});
    expect_values(concentration.get_column<double>("max_short"), {nan, -.5});
}

TEST(Positions, ProducesTypedEmptyOutputFrames) {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{});
    positions.load_column<double>("asset", std::vector<double>{},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{},
                                  hmdf::nan_policy::dont_pad_with_nans);

    const auto allocation = fincore::get_percent_alloc(positions);
    EXPECT_TRUE(allocation.get_index().empty());
    expect_double_columns(allocation, {"asset", "cash"});

    const auto long_short = fincore::get_long_short_pos(positions);
    EXPECT_TRUE(long_short.get_index().empty());
    expect_double_columns(long_short, {"long", "short"});

    const auto gross = fincore::gross_lev(positions);
    EXPECT_TRUE(gross.get_index().empty());
    expect_double_columns(gross, {"gross_lev"});

    const auto concentration =
        fincore::get_max_median_position_concentration(positions);
    EXPECT_TRUE(concentration.get_index().empty());
    expect_double_columns(concentration,
                          {"max_long", "median_long", "median_short", "max_short"});
}

TEST(Positions, RejectsNoGenuineDoubleColumnsAndInvalidCashNames) {
    fincore::dataframe metadata_only;
    metadata_only.load_index(std::vector<fincore::timestamp>{1});
    metadata_only.load_column<int>("metadata", std::vector<int>{1},
                                   hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(fincore::get_percent_alloc(metadata_only), fincore::input_error);
    EXPECT_THROW(fincore::get_long_short_pos(metadata_only), fincore::input_error);
    EXPECT_THROW(fincore::gross_lev(metadata_only), fincore::input_error);
    EXPECT_THROW(fincore::get_max_median_position_concentration(metadata_only),
                 fincore::input_error);

    const auto positions = reference_positions();
    EXPECT_THROW(fincore::get_long_short_pos(positions, ""), fincore::input_error);
    EXPECT_THROW(fincore::gross_lev(positions, ""), fincore::input_error);
    EXPECT_THROW(fincore::get_max_median_position_concentration(positions, ""),
                 fincore::input_error);
}

TEST(Positions, RejectsWrongTypedAndMalformedCashOrNumericColumns) {
    fincore::dataframe wrong_cash;
    wrong_cash.load_index(std::vector<fincore::timestamp>{1, 2});
    wrong_cash.load_column<double>("asset", std::vector<double>{1.0, 2.0},
                                   hmdf::nan_policy::dont_pad_with_nans);
    wrong_cash.load_column<int>("cash", std::vector<int>{1, 2},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::gross_lev(wrong_cash), fincore::input_error);
    EXPECT_THROW(fincore::get_max_median_position_concentration(wrong_cash),
                 fincore::input_error);

    fincore::dataframe malformed_cash;
    malformed_cash.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed_cash.load_column<double>("asset", std::vector<double>{1.0, 2.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    malformed_cash.load_column<double>("cash", std::vector<double>{1.0},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_percent_alloc(malformed_cash), fincore::input_error);
    EXPECT_THROW(fincore::get_long_short_pos(malformed_cash), fincore::input_error);
    EXPECT_THROW(fincore::gross_lev(malformed_cash), fincore::input_error);
    EXPECT_THROW(fincore::get_max_median_position_concentration(malformed_cash),
                 fincore::input_error);

    fincore::dataframe malformed_asset;
    malformed_asset.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed_asset.load_column<double>("asset", std::vector<double>{1.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    malformed_asset.load_column<double>("cash", std::vector<double>{2.0, 3.0},
                                        hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::get_percent_alloc(malformed_asset), fincore::input_error);
    EXPECT_THROW(fincore::get_long_short_pos(malformed_asset), fincore::input_error);
    EXPECT_THROW(fincore::gross_lev(malformed_asset), fincore::input_error);
    EXPECT_THROW(fincore::get_max_median_position_concentration(malformed_asset),
                 fincore::input_error);
}

TEST(Positions, HonorsNaNAndInfinityBoundaries) {
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{1, 2, 3});
    positions.load_column<double>(
        "asset", std::vector<double>{std::numeric_limits<double>::infinity(),
                                      -std::numeric_limits<double>::infinity(), nan},
        hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("cash", std::vector<double>{1.0, 1.0, 1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);

    const auto allocation = fincore::get_percent_alloc(positions);
    expect_values(allocation.get_column<double>("asset"), {nan, nan, nan});
    expect_values(allocation.get_column<double>("cash"), {0.0, 0.0, 1.0});

    const auto long_short = fincore::get_long_short_pos(positions);
    expect_values(long_short.get_column<double>("long"),
                  {std::numeric_limits<double>::infinity(), 0.0, 0.0});
    expect_values(long_short.get_column<double>("short"),
                  {0.0, std::numeric_limits<double>::infinity(), 0.0});

    const auto gross = fincore::gross_lev(positions);
    expect_values(gross.get_column<double>("gross_lev"), {nan, nan, 0.0});

    const auto concentration =
        fincore::get_max_median_position_concentration(positions);
    for (const std::string_view column :
         {"max_long", "median_long", "median_short", "max_short"}) {
        expect_values(concentration.get_column<double>(std::string{column}.c_str()),
                      {nan, nan, nan});
    }
}

fincore::dataframe residual_positions() {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{1, 2, 3, 4});
    frame.load_column<double>(
        "AAA", {100.0, -50.0, 30.0,
                std::numeric_limits<double>::quiet_NaN()},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("BBB", {-200.0, 80.0, -20.0, 40.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("CCC", {50.0, 0.0, 90.0, -10.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("cash", std::vector<double>(4, 1000.0),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(Positions, TopLongShortAbsoluteMatchesPythonOracle) {
    const auto positions = residual_positions();
    const auto top = fincore::get_top_long_short_abs(positions, 2U);

    ASSERT_EQ(top.long_positions.size(), 2U);
    EXPECT_EQ(top.long_positions[0].symbol, "AAA");
    EXPECT_DOUBLE_EQ(top.long_positions[0].value, 100.0);
    EXPECT_EQ(top.long_positions[1].symbol, "CCC");
    EXPECT_DOUBLE_EQ(top.long_positions[1].value, 90.0);

    ASSERT_EQ(top.short_positions.size(), 2U);
    EXPECT_EQ(top.short_positions[0].symbol, "BBB");
    EXPECT_DOUBLE_EQ(top.short_positions[0].value, -200.0);
    EXPECT_EQ(top.short_positions[1].symbol, "AAA");
    EXPECT_DOUBLE_EQ(top.short_positions[1].value, -50.0);

    ASSERT_EQ(top.absolute_positions.size(), 2U);
    EXPECT_EQ(top.absolute_positions[0].symbol, "BBB");
    EXPECT_DOUBLE_EQ(top.absolute_positions[0].value, 200.0);
    EXPECT_EQ(top.absolute_positions[1].symbol, "AAA");
    EXPECT_DOUBLE_EQ(top.absolute_positions[1].value, 100.0);

    const std::array<char, 13> embedded_storage{
        'c', 'a', 's', 'h', ' ', 'x', '!', '!', '!', '!', '!', '!', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};
    EXPECT_THROW(static_cast<void>(fincore::get_top_long_short_abs(
                     positions, 2U, embedded_selector)),
                 fincore::input_error);
}

TEST(Positions, LongShortNotionalMatchesPythonOracle) {
    const auto positions = residual_positions();
    const auto notionals = fincore::get_long_short_notional(positions);
    ASSERT_EQ(notionals.first.size(), 4U);
    EXPECT_DOUBLE_EQ(notionals.first[0], 150.0);
    EXPECT_DOUBLE_EQ(notionals.first[1], 80.0);
    EXPECT_DOUBLE_EQ(notionals.first[2], 120.0);
    EXPECT_DOUBLE_EQ(notionals.first[3], 40.0);
    EXPECT_DOUBLE_EQ(notionals.second[0], 200.0);
    EXPECT_DOUBLE_EQ(notionals.second[1], 50.0);
    EXPECT_DOUBLE_EQ(notionals.second[2], 20.0);
    EXPECT_DOUBLE_EQ(notionals.second[3], 10.0);
}

TEST(Positions, StackPositionsMatchesPythonOracleIncludingNanCells) {
    const auto positions = residual_positions();
    const auto stacked = fincore::stack_positions(positions);
    ASSERT_EQ(stacked.size(), 12U);
    EXPECT_EQ(stacked.front().date, 1);
    EXPECT_EQ(stacked.front().symbol, "AAA");
    EXPECT_DOUBLE_EQ(stacked.front().value, 100.0);
    EXPECT_EQ(stacked[3].symbol, "AAA");
    EXPECT_DOUBLE_EQ(stacked[3].value, -50.0);
    EXPECT_EQ(stacked[4].symbol, "BBB");
    EXPECT_DOUBLE_EQ(stacked[4].value, 80.0);
    const auto& last_row_start = stacked[9];
    EXPECT_EQ(last_row_start.date, 4);
    EXPECT_EQ(last_row_start.symbol, "AAA");
    EXPECT_TRUE(std::isnan(last_row_start.value));
    EXPECT_EQ(stacked[11].symbol, "CCC");
    EXPECT_DOUBLE_EQ(stacked[11].value, -10.0);
    for (const auto& record : stacked) {
        EXPECT_NE(record.symbol, "cash");
    }
}

}  // namespace
