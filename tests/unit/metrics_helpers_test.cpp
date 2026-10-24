#include <fincore/calendar.hpp>
#include <fincore/metrics/helpers.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

using fincore::dataframe;
using fincore::frequency;
using fincore::matrix;
using fincore::timestamp;

dataframe two_column_frame() {
    dataframe frame;
    frame.load_index(std::vector<timestamp>{1, 2});
    frame.load_column<double>("a", std::vector<double>{1.0, 2.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("b", std::vector<double>{3.0, 4.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(MetricsHelpers, MatrixStoresRowMajorValuesWithBoundsChecks) {
    matrix values{2, 3, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}};
    EXPECT_EQ(values.rows(), 2U);
    EXPECT_EQ(values.columns(), 3U);
    ASSERT_EQ(values.values().size(), 6U);
    EXPECT_DOUBLE_EQ(values.values()[0], 1.0);
    EXPECT_DOUBLE_EQ(values.values()[5], 6.0);
    EXPECT_DOUBLE_EQ(values.at(0, 2), 3.0);
    EXPECT_DOUBLE_EQ(values.at(1, 0), 4.0);

    const auto second_row = values.row(1);
    ASSERT_EQ(second_row.size(), 3U);
    EXPECT_DOUBLE_EQ(second_row[0], 4.0);
    EXPECT_DOUBLE_EQ(second_row[2], 6.0);

    EXPECT_THROW(static_cast<void>(matrix(2, 3, {1.0})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(values.at(2, 0)), fincore::input_error);
    EXPECT_THROW(static_cast<void>(values.at(0, 3)), fincore::input_error);
    EXPECT_THROW(static_cast<void>(values.row(2)), fincore::input_error);
}

TEST(MetricsHelpers, AlignedSeriesUsesLeftOrderAndDropsMissingTimestamps) {
    const auto left = fincore::make_series({30, 10, 20}, "a", {3.0, 1.0, 2.0});
    const auto right = fincore::make_series({10, 30, 40}, "b",
                                            {10.0, 30.0, 40.0});

    const auto [aligned_left, aligned_right] =
        fincore::aligned_series(left, "a", right, "b");

    EXPECT_EQ(aligned_left.get_index(),
              (std::vector<timestamp>{30, 10}));
    EXPECT_EQ(aligned_right.get_index(),
              (std::vector<timestamp>{30, 10}));
    EXPECT_EQ(aligned_left.get_column<double>("a"),
              (std::vector<double>{3.0, 1.0}));
    EXPECT_EQ(aligned_right.get_column<double>("b"),
              (std::vector<double>{30.0, 10.0}));
}

TEST(MetricsHelpers, AlignedSeriesKeepsNaNButRequiresUniqueTimestamps) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto left = fincore::make_series({5, 6}, "a", {1.0, nan});
    const auto right = fincore::make_series({5, 6}, "b", {2.0, 3.0});

    const auto [aligned_left, aligned_right] =
        fincore::aligned_series(left, "a", right, "b");
    EXPECT_EQ(aligned_left.get_index(), (std::vector<timestamp>{5, 6}));
    EXPECT_TRUE(std::isnan(aligned_left.get_column<double>("a")[1]));

    const auto duplicate_left = fincore::make_series({5, 5}, "a", {1.0, 2.0});
    EXPECT_THROW(
        static_cast<void>(
            fincore::aligned_series(duplicate_left, "a", right, "b")),
        fincore::input_error);

    const auto duplicate_right =
        fincore::make_series({5, 5}, "b", {2.0, 3.0});
    EXPECT_THROW(
        static_cast<void>(fincore::aligned_series(left, "a",
                                                  duplicate_right, "b")),
        fincore::input_error);

    const auto nat_left = fincore::make_series(
        {std::numeric_limits<timestamp>::min()}, "a", {1.0});
    const auto single_right = fincore::make_series({5}, "b", {2.0});
    EXPECT_THROW(
        static_cast<void>(fincore::aligned_series(nat_left, "a",
                                                  single_right, "b")),
        fincore::input_error);
}

TEST(MetricsHelpers, AlignedSeriesHandlesDisjointAndEmptyOverlap) {
    const auto left = fincore::make_series({1}, "a", {1.0});
    const auto right = fincore::make_series({2}, "b", {2.0});

    const auto [aligned_left, aligned_right] =
        fincore::aligned_series(left, "a", right, "b");
    EXPECT_TRUE(aligned_left.get_index().empty());
    EXPECT_TRUE(aligned_right.get_index().empty());
    EXPECT_TRUE(aligned_left.get_column<double>("a").empty());
    EXPECT_TRUE(aligned_right.get_column<double>("b").empty());

    const auto same = fincore::make_series({1}, "a", {1.0});
    const auto [again_left, again_right] =
        fincore::aligned_series(same, "a", same, "a");
    EXPECT_EQ(again_left.get_index(), (std::vector<timestamp>{1}));
    EXPECT_EQ(again_right.get_column<double>("a"),
              (std::vector<double>{1.0}));
}

TEST(MetricsHelpers, AlignedFramesKeepEveryDoubleColumn) {
    dataframe left;
    left.load_index(std::vector<timestamp>{5, 3, 1});
    left.load_column<double>("a", std::vector<double>{50.0, 30.0, 10.0},
                             hmdf::nan_policy::dont_pad_with_nans);
    left.load_column<double>("b", std::vector<double>{51.0, 31.0, 11.0},
                             hmdf::nan_policy::dont_pad_with_nans);

    dataframe right;
    right.load_index(std::vector<timestamp>{3, 5, 9});
    right.load_column<double>("c", std::vector<double>{300.0, 500.0, 900.0},
                              hmdf::nan_policy::dont_pad_with_nans);

    const auto [aligned_left, aligned_right] =
        fincore::aligned_series(left, right);

    EXPECT_EQ(aligned_left.get_index(), (std::vector<timestamp>{5, 3}));
    EXPECT_EQ(aligned_right.get_index(), (std::vector<timestamp>{5, 3}));
    ASSERT_EQ(aligned_left.shape().second, 2U);
    ASSERT_EQ(aligned_right.shape().second, 1U);

    const auto left_info = aligned_left.get_columns_info<double>();
    EXPECT_STREQ(std::get<0>(left_info[0]).c_str(), "a");
    EXPECT_STREQ(std::get<0>(left_info[1]).c_str(), "b");
    EXPECT_EQ(aligned_left.get_column<double>("a"),
              (std::vector<double>{50.0, 30.0}));
    EXPECT_EQ(aligned_left.get_column<double>("b"),
              (std::vector<double>{51.0, 31.0}));
    EXPECT_EQ(aligned_right.get_column<double>("c"),
              (std::vector<double>{500.0, 300.0}));

    dataframe relocated = left;
    relocated.get_column<double>("a")[0] = 999.0;
    EXPECT_EQ(aligned_left.get_column<double>("a")[0], 50.0);
}

TEST(MetricsHelpers, AlignedFramesRejectNonDoubleColumnsAndMismatches) {
    dataframe left;
    left.load_index(std::vector<timestamp>{1, 2});
    left.load_column<double>("a", std::vector<double>{1.0, 2.0},
                             hmdf::nan_policy::dont_pad_with_nans);
    left.load_column<int>("integer", std::vector<int>{1, 2},
                          hmdf::nan_policy::dont_pad_with_nans);

    dataframe right;
    right.load_index(std::vector<timestamp>{1, 2});
    right.load_column<double>("b", std::vector<double>{3.0, 4.0},
                              hmdf::nan_policy::dont_pad_with_nans);

    EXPECT_THROW(static_cast<void>(fincore::aligned_series(left, right)),
                 fincore::input_error);

    dataframe short_column;
    short_column.load_index(std::vector<timestamp>{1, 2});
    short_column.load_column<double>("b", std::vector<double>{3.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        static_cast<void>(fincore::aligned_series(right, short_column)),
        fincore::input_error);
}

TEST(MetricsHelpers, EnsureDatetimeIndexValidatesStrictOrdering) {
    const auto valid =
        fincore::make_series({0, 86'400'000'000'000LL}, "returns",
                             {0.01, 0.02});
    const auto normalized = fincore::ensure_datetime_index_series(valid);
    EXPECT_EQ(normalized.get_index(),
              (std::vector<timestamp>{0, 86'400'000'000'000LL}));
    EXPECT_EQ(normalized.get_column<double>("returns"),
              (std::vector<double>{0.01, 0.02}));

    const auto unsorted =
        fincore::make_series({2, 1}, "returns", {0.01, 0.02});
    EXPECT_THROW(static_cast<void>(
                     fincore::ensure_datetime_index_series(unsorted)),
                 fincore::input_error);

    const auto duplicate =
        fincore::make_series({1, 1}, "returns", {0.01, 0.02});
    EXPECT_THROW(static_cast<void>(
                     fincore::ensure_datetime_index_series(duplicate)),
                 fincore::input_error);

    const auto nat = fincore::make_series(
        {std::numeric_limits<timestamp>::min()}, "returns", {0.01});
    EXPECT_THROW(
        static_cast<void>(fincore::ensure_datetime_index_series(nat)),
        fincore::input_error);
}

TEST(MetricsHelpers, EnsureDatetimeIndexIsAnOwningCopy) {
    auto source = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    auto normalized = fincore::ensure_datetime_index_series(source);
    source.get_column<double>("returns")[0] = 0.99;
    EXPECT_EQ(normalized.get_column<double>("returns")[0], 0.01);
}

TEST(MetricsHelpers, EnsureDatetimeIndexGeneratesSourceDateRange) {
    const std::vector<double> values{1.0, 2.0, 3.0};

    const auto daily = fincore::ensure_datetime_index_series(
        values, frequency::daily, "values");
    EXPECT_EQ(daily.get_index(),
              (std::vector<timestamp>{
                  fincore::utc_timestamp(1970, 1, 1),
                  fincore::utc_timestamp(1970, 1, 2),
                  fincore::utc_timestamp(1970, 1, 3)}));
    EXPECT_EQ(daily.get_column<double>("values"),
              (std::vector<double>{1.0, 2.0, 3.0}));

    const auto weekly = fincore::ensure_datetime_index_series(
        values, frequency::weekly, "values");
    EXPECT_EQ(weekly.get_index(),
              (std::vector<timestamp>{
                  fincore::utc_timestamp(1970, 1, 4),
                  fincore::utc_timestamp(1970, 1, 11),
                  fincore::utc_timestamp(1970, 1, 18)}));

    const auto monthly = fincore::ensure_datetime_index_series(
        std::vector<double>{1.0, 2.0, 3.0, 4.0}, frequency::monthly, "values");
    EXPECT_EQ(monthly.get_index(),
              (std::vector<timestamp>{
                  fincore::utc_timestamp(1970, 1, 31),
                  fincore::utc_timestamp(1970, 2, 28),
                  fincore::utc_timestamp(1970, 3, 31),
                  fincore::utc_timestamp(1970, 4, 30)}));

    const auto quarterly = fincore::ensure_datetime_index_series(
        std::vector<double>{1.0, 2.0, 3.0, 4.0}, frequency::quarterly,
        "values");
    EXPECT_EQ(quarterly.get_index(),
              (std::vector<timestamp>{
                  fincore::utc_timestamp(1970, 3, 31),
                  fincore::utc_timestamp(1970, 6, 30),
                  fincore::utc_timestamp(1970, 9, 30),
                  fincore::utc_timestamp(1970, 12, 31)}));

    const auto yearly = fincore::ensure_datetime_index_series(
        values, frequency::yearly, "values");
    EXPECT_EQ(yearly.get_index(),
              (std::vector<timestamp>{
                  fincore::utc_timestamp(1970, 12, 31),
                  fincore::utc_timestamp(1971, 12, 31),
                  fincore::utc_timestamp(1972, 12, 31)}));

    EXPECT_THROW(
        static_cast<void>(fincore::ensure_datetime_index_series(
            values, static_cast<frequency>(99), "values")),
        fincore::input_error);
}

TEST(MetricsHelpers, EnsureDatetimeIndexHandlesEmptyAndUnindexedFrames) {
    const auto empty_values = fincore::ensure_datetime_index_series(
        std::span<const double>{}, frequency::daily);
    EXPECT_TRUE(empty_values.get_index().empty());
    EXPECT_TRUE(empty_values.get_column<double>("returns").empty());

    dataframe empty_frame;
    empty_frame.load_index(std::vector<timestamp>{});
    const auto normalized =
        fincore::ensure_datetime_index_series(empty_frame);
    EXPECT_TRUE(normalized.get_index().empty());
    EXPECT_EQ(normalized.shape().second, 0U);

    dataframe one_empty_column;
    one_empty_column.load_index(std::vector<timestamp>{});
    one_empty_column.load_column<double>(
        "only", std::vector<double>{}, hmdf::nan_policy::dont_pad_with_nans);
    const auto generated = fincore::ensure_datetime_index_series(one_empty_column);
    EXPECT_TRUE(generated.get_index().empty());
    EXPECT_TRUE(generated.get_column<double>("only").empty());
}

TEST(MetricsHelpers, FlattenIsRowMajorInColumnInsertionOrder) {
    const auto frame = two_column_frame();
    EXPECT_EQ(fincore::flatten(frame),
              (std::vector<double>{1.0, 3.0, 2.0, 4.0}));

    const double nan = std::numeric_limits<double>::quiet_NaN();
    dataframe with_nan;
    with_nan.load_index(std::vector<timestamp>{1, 2});
    with_nan.load_column<double>("a", std::vector<double>{nan, 2.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
    with_nan.load_column<double>("b", std::vector<double>{3.0, 4.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
    const auto flattened = fincore::flatten(with_nan);
    ASSERT_EQ(flattened.size(), 4U);
    EXPECT_TRUE(std::isnan(flattened[0]));
    EXPECT_DOUBLE_EQ(flattened[1], 3.0);

    dataframe no_columns;
    no_columns.load_index(std::vector<timestamp>{});
    EXPECT_TRUE(fincore::flatten(no_columns).empty());

    const matrix values{2, 3, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}};
    EXPECT_EQ(fincore::flatten(values),
              (std::vector<double>{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}));
}

TEST(MetricsHelpers, FlattenRejectsNonDoubleAndMismatchedColumns) {
    dataframe integer;
    integer.load_index(std::vector<timestamp>{1});
    integer.load_column<int>("integer", std::vector<int>{1},
                             hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::flatten(integer)),
                 fincore::input_error);

    dataframe short_column;
    short_column.load_index(std::vector<timestamp>{1, 2});
    short_column.load_column<double>("a", std::vector<double>{1.0},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::flatten(short_column)),
                 fincore::input_error);
}

TEST(MetricsHelpers, ToDataframeConvertsOneAndTwoDimensionalInput) {
    const auto series = fincore::to_dataframe(std::vector<double>{5.0, 6.0});
    EXPECT_EQ(series.get_index(), (std::vector<timestamp>{0, 1}));
    EXPECT_EQ(series.get_column<double>("returns"),
              (std::vector<double>{5.0, 6.0}));

    const auto named =
        fincore::to_dataframe(std::vector<double>{7.0}, "custom");
    EXPECT_EQ(named.get_index(), (std::vector<timestamp>{0}));
    EXPECT_EQ(named.get_column<double>("custom"), (std::vector<double>{7.0}));

    const matrix values{2, 3, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}};
    const auto frame = fincore::to_dataframe(values);
    EXPECT_EQ(frame.get_index(), (std::vector<timestamp>{0, 1}));
    EXPECT_EQ(frame.get_column<double>("column_0"),
              (std::vector<double>{1.0, 4.0}));
    EXPECT_EQ(frame.get_column<double>("column_1"),
              (std::vector<double>{2.0, 5.0}));
    EXPECT_EQ(frame.get_column<double>("column_2"),
              (std::vector<double>{3.0, 6.0}));

    const auto info = frame.get_columns_info<double>();
    ASSERT_EQ(info.size(), 3U);
    EXPECT_STREQ(std::get<0>(info[0]).c_str(), "column_0");
    EXPECT_STREQ(std::get<0>(info[1]).c_str(), "column_1");
    EXPECT_STREQ(std::get<0>(info[2]).c_str(), "column_2");

    const auto prefixed = fincore::to_dataframe(values, "series");
    EXPECT_EQ(prefixed.get_column<double>("series_2"),
              (std::vector<double>{3.0, 6.0}));

    EXPECT_THROW(
        static_cast<void>(fincore::to_dataframe(values, std::string_view{})),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::to_dataframe(std::vector<double>{},
                                                         std::string_view{})),
                 fincore::input_error);
}

TEST(MetricsHelpers, ToDataframeHandlesZeroRowsAndColumns) {
    const auto zero_rows =
        fincore::to_dataframe(matrix{0, 2, {}});
    EXPECT_TRUE(zero_rows.get_index().empty());
    EXPECT_EQ(zero_rows.shape().second, 2U);
    EXPECT_TRUE(zero_rows.get_column<double>("column_0").empty());
    EXPECT_TRUE(zero_rows.get_column<double>("column_1").empty());

    const auto zero_columns = fincore::to_dataframe(matrix{2, 0, {}});
    EXPECT_EQ(zero_columns.get_index(), (std::vector<timestamp>{0, 1}));
    EXPECT_EQ(zero_columns.shape().second, 0U);
}

TEST(MetricsHelpers, ToDataframeCopiesExistingFrames) {
    auto source = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    auto copied = fincore::to_dataframe(source);
    source.get_column<double>("returns")[0] = 0.99;
    EXPECT_EQ(copied.get_column<double>("returns")[0], 0.01);
    EXPECT_EQ(copied.get_index(), (std::vector<timestamp>{1, 2}));
}

}  // namespace
