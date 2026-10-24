#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

fincore::dataframe make_mixed_frame() {
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2),
      fincore::utc_timestamp(2020, 1, 3),
      fincore::utc_timestamp(2020, 1, 6)});
  frame.load_column<double>(
      "returns", std::vector<double>{0.01, std::nan(""), 0.03},
      hmdf::nan_policy::dont_pad_with_nans);
  frame.load_column<std::string>(
      "symbol", std::vector<std::string>{"AAA", "AAA", "BBB"},
      hmdf::nan_policy::dont_pad_with_nans);
  frame.load_column<bool>(
      "eligible", std::vector<bool>{true, false, true},
      hmdf::nan_policy::dont_pad_with_nans);
  return frame;
}

TEST(DataframeSemantics, PreservesIndexOrderAndNanValues) {
  const auto frame = make_mixed_frame();
  const auto& index = frame.get_index();
  ASSERT_EQ(index.size(), 3U);
  EXPECT_EQ(index[0], fincore::utc_timestamp(2020, 1, 2));
  EXPECT_EQ(index[2], fincore::utc_timestamp(2020, 1, 6));
  const auto values = fincore::require_double_column(frame, "returns");
  EXPECT_DOUBLE_EQ(values[0], 0.01);
  EXPECT_TRUE(std::isnan(values[1]));
  EXPECT_DOUBLE_EQ(values[2], 0.03);
}

TEST(DataframeSemantics, AbsentColumnDiffersFromPresentNanCell) {
  const auto frame = make_mixed_frame();
  EXPECT_TRUE(frame.has_column("returns"));
  EXPECT_FALSE(frame.has_column("absent_factor"));
  const auto values = fincore::require_double_column(frame, "returns");
  EXPECT_TRUE(std::isnan(values[1]));
}

TEST(DataframeSemantics, PadsAnExplicitlyShortColumnWithNaN) {
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2), fincore::utc_timestamp(2020, 1, 3),
      fincore::utc_timestamp(2020, 1, 6)});
  frame.load_column<double>("sparse", std::vector<double>{1.0},
                            hmdf::nan_policy::pad_with_nans);
  const auto values = fincore::require_double_column(frame, "sparse");
  ASSERT_EQ(values.size(), 3U);
  EXPECT_DOUBLE_EQ(values[0], 1.0);
  EXPECT_TRUE(std::isnan(values[1]));
  EXPECT_TRUE(std::isnan(values[2]));
}

TEST(DataframeSemantics, PreservesDtypeClasses) {
  const auto frame = make_mixed_frame();
  EXPECT_EQ(frame.get_column<std::string>("symbol")[0], "AAA");
  EXPECT_FALSE(frame.get_column<bool>("eligible")[1]);
  EXPECT_THROW(fincore::require_double_column(frame, "symbol"),
               fincore::input_error);
  EXPECT_THROW(fincore::require_double_column(frame, "eligible"),
               fincore::input_error);
}

TEST(DataframeSemantics, PreservesDuplicateTimestampsInInsertionOrder) {
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2), fincore::utc_timestamp(2020, 1, 2),
      fincore::utc_timestamp(2020, 1, 3)});
  frame.load_column<double>("x", std::vector<double>{1.0, 2.0, 3.0},
                            hmdf::nan_policy::dont_pad_with_nans);
  const auto& index = frame.get_index();
  ASSERT_EQ(index.size(), 3U);
  EXPECT_EQ(index[0], index[1]);
  EXPECT_DOUBLE_EQ(frame.get_column<double>("x")[1], 2.0);
}

TEST(DataframeSemantics, CopyOwnsValuesIndependently) {
  auto original = make_mixed_frame();
  auto copy = original;
  original.get_column<double>("returns")[0] = 9.0;
  original.get_column<std::string>("symbol")[0] = "MUT";
  EXPECT_DOUBLE_EQ(copy.get_column<double>("returns")[0], 0.01);
  EXPECT_EQ(copy.get_column<std::string>("symbol")[0], "AAA");
}

TEST(DataframeSemantics, NonfiniteValuesRemainInTheOwningFrame) {
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2)});
  frame.load_column<double>(
      "x", std::vector<double>{std::numeric_limits<double>::infinity()},
      hmdf::nan_policy::dont_pad_with_nans);
  const auto values = fincore::require_double_column(frame, "x");
  ASSERT_EQ(values.size(), 1U);
  EXPECT_TRUE(std::isinf(values[0]));
}

TEST(DataframeSemantics, UtcTimestampsAreIntegerNanoseconds) {
  const fincore::timestamp timestamp =
      fincore::utc_timestamp(2020, 1, 2, 12, 30, 15, 123456789);
  EXPECT_EQ(timestamp, 1577968215123456789LL);
  EXPECT_EQ(fincore::utc_year(timestamp), 2020);
}

TEST(DataframeSemantics, EmptyFrameHasNoRowsOrColumns) {
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{});
  frame.load_column<double>("x", std::vector<double>{},
                            hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_EQ(frame.shape().first, 0U);
  EXPECT_TRUE(fincore::require_double_column(frame, "x").empty());
}

}  // namespace
