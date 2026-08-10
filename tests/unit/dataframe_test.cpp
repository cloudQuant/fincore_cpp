#include <fincore/dataframe.hpp>

#include <gtest/gtest.h>

#include <span>
#include <vector>

TEST(Dataframe, MakesADataframeNativeSeries) {
    const auto series =
        fincore::make_series({10, 20}, "returns", {0.01, -0.02});

    EXPECT_EQ(series.get_index(), (std::vector<fincore::timestamp>{10, 20}));
    EXPECT_EQ(series.get_column<double>("returns"),
              (std::vector<double>{0.01, -0.02}));
}

TEST(Dataframe, RejectsMismatchedIndexAndValueLengths) {
    EXPECT_THROW(fincore::make_series({10}, "returns", {0.01, -0.02}),
                 fincore::input_error);
}

TEST(Dataframe, RejectsAnEmptyColumnName) {
    EXPECT_THROW(fincore::make_series({10}, "", {0.01}),
                 fincore::input_error);
}

TEST(Dataframe, ReturnsAViewOfTheNamedDoubleColumn) {
    const auto series = fincore::make_series({10, 20}, "returns", {0.01, -0.02});
    const std::span<const double> values =
        fincore::require_double_column(series, "returns");

    ASSERT_EQ(values.size(), 2U);
    EXPECT_DOUBLE_EQ(values[0], 0.01);
    EXPECT_DOUBLE_EQ(values[1], -0.02);
}

TEST(Dataframe, RejectsMissingDoubleColumn) {
    const auto series = fincore::make_series({10}, "returns", {0.01});

    EXPECT_THROW(fincore::require_double_column(series, "benchmark"),
                 fincore::input_error);
}

TEST(Dataframe, AlignsBenchmarkPairsOnTheirCommonTimestampIndex) {
    const auto left =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    const auto right =
        fincore::make_series({2, 3, 4}, "benchmark", {0.04, 0.06, 0.08});
    const auto [aligned_left, aligned_right] =
        fincore::align_on_common_index(left, "returns", right, "benchmark");

    EXPECT_EQ(aligned_left.get_index(),
              (std::vector<fincore::timestamp>{2, 3}));
    EXPECT_EQ(aligned_right.get_index(),
              (std::vector<fincore::timestamp>{2, 3}));
    EXPECT_EQ(aligned_left.get_column<double>("returns"),
              (std::vector<double>{0.02, 0.03}));
    EXPECT_EQ(aligned_right.get_column<double>("benchmark"),
              (std::vector<double>{0.04, 0.06}));
}
