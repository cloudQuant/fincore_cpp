#include <fincore/attribution/performance.hpp>
#include <fincore/calendar.hpp>
#include <fincore/error.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

TEST(AttributionPerformance, AlignsAssetsAndLoadingDatesWithWarnings) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{start, start + day});
    positions.load_column<double>(
        "A", {1.0, 2.0}, hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "B", {3.0, 4.0}, hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "C", {5.0, 6.0}, hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "cash", {0.0, 0.0}, hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{start, start + day});
    returns.load_column<double>(
        "returns", {0.01, 0.02}, hmdf::nan_policy::dont_pad_with_nans);
    fincore::dataframe factor_returns;
    factor_returns.load_index(
        std::vector<fincore::timestamp>{start, start + day});
    factor_returns.load_column<double>(
        "F1", {0.03, 0.04}, hmdf::nan_policy::dont_pad_with_nans);
    factor_returns.load_column<double>(
        "F2", {-0.01, 0.02}, hmdf::nan_policy::dont_pad_with_nans);
    fincore::factor_loading_table loadings;
    loadings.dates = {start};
    loadings.assets = {"A", "B"};
    loadings.factors = {"F1", "F2"};
    loadings.values = {1.0, 2.0, 3.0, 4.0};

    const auto aligned =
        fincore::align_and_warn(returns, positions, factor_returns, loadings);
    ASSERT_EQ(aligned.warnings.size(), 2U);
    EXPECT_NE(aligned.warnings[0].find("0.333"), std::string::npos);
    EXPECT_NE(aligned.warnings[0].find("C"), std::string::npos);
    EXPECT_NE(aligned.warnings[1].find("1 dates"), std::string::npos);
    ASSERT_EQ(aligned.positions.get_index().size(), 1U);
    EXPECT_EQ(aligned.positions.get_index()[0], start);
    EXPECT_NO_THROW(static_cast<void>(
        aligned.positions.get_column<double>("A")));
    EXPECT_NO_THROW(static_cast<void>(
        aligned.positions.get_column<double>("B")));
    EXPECT_THROW(static_cast<void>(
                     aligned.positions.get_column<double>("C")),
                 hmdf::ColNotFound);
    ASSERT_EQ(aligned.returns.get_index().size(), 1U);
    ASSERT_EQ(aligned.factor_returns.get_index().size(), 1U);
    EXPECT_DOUBLE_EQ(aligned.factor_returns.get_column<double>("F1")[0],
                     0.03);
    EXPECT_EQ(aligned.factor_loadings.values.size(), 4U);

    fincore::factor_loading_table unrelated;
    unrelated.dates = {start};
    unrelated.assets = {"X"};
    unrelated.factors = {"F1"};
    unrelated.values = {1.0};
    EXPECT_THROW(static_cast<void>(fincore::align_and_warn(
                     returns, positions, factor_returns, unrelated)),
                 fincore::input_error);
    auto malformed = loadings;
    malformed.values.pop_back();
    EXPECT_THROW(static_cast<void>(fincore::align_and_warn(
                     returns, positions, factor_returns, malformed)),
                 fincore::input_error);
}

}  // namespace
