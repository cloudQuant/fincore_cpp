#include <fincore/error.hpp>
#include <fincore/metrics/positions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
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

}  // namespace
