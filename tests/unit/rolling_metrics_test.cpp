#include <fincore/dataframe.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/rolling.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-12;

const std::vector<fincore::timestamp> reference_index{1, 2, 3, 4, 5, 6};
const std::vector<double> reference_returns{.01, -.02, .03, 0.0, -.01, .02};

fincore::dataframe reference_series(std::string_view column = "returns") {
    return fincore::make_series(reference_index, column, reference_returns);
}

void expect_values(const fincore::dataframe& actual,
                   std::string_view column,
                   const std::vector<double>& expected) {
    const auto& values = actual.get_column<double>(std::string{column}.c_str());
    ASSERT_EQ(values.size(), expected.size());
    for (std::size_t position = 0; position < expected.size(); ++position) {
        if (std::isnan(expected[position])) {
            EXPECT_TRUE(std::isnan(values[position]));
        } else {
            EXPECT_NEAR(values[position], expected[position], tolerance);
        }
    }
}

void expect_empty_typed_series(const fincore::dataframe& actual,
                               std::string_view column) {
    EXPECT_TRUE(actual.get_index().empty());
    EXPECT_TRUE(actual.has_column(std::string{column}.c_str()));
    EXPECT_TRUE(actual.get_column<double>(std::string{column}.c_str()).empty());
    EXPECT_EQ(actual.get_columns_info<double>().size(), 1U);
}

template <typename Function>
void expect_input_errors(Function&& function) {
    const auto missing = fincore::make_series({1}, "other", {.01});
    EXPECT_THROW(function(missing), fincore::input_error);

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(function(wrong_type), fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(function(malformed), fincore::input_error);
}

TEST(RollingMetrics, MatchesPythonGoldensWithEndpointAndFullIndexShapes) {
    const auto returns = reference_series();

    const auto roll_sharpe = fincore::roll_sharpe_ratio(returns, 3U);
    EXPECT_EQ(roll_sharpe.get_index(),
              (std::vector<fincore::timestamp>{3, 4, 5, 6}));
    expect_values(roll_sharpe, "returns",
                  {4.205259864302774, 2.1026299321513866,
                   5.083911274417939, 3.464101615137753});

    const auto roll_drawdown = fincore::roll_max_drawdown(returns, 3U);
    EXPECT_EQ(roll_drawdown.get_index(),
              (std::vector<fincore::timestamp>{3, 4, 5, 6}));
    expect_values(roll_drawdown, "returns",
                  {-.019999999999999997, -.020000000000000018,
                   -.009999999999999976, -.010000000000000009});

    const auto volatility = fincore::rolling_volatility(returns, 3U);
    EXPECT_EQ(volatility.get_index(), reference_index);
    expect_values(volatility, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   .3994996871087636, .3994996871087636,
                   .3304542328371662, .24248711305964288});

    const auto sharpe = fincore::rolling_sharpe(returns, 3U);
    EXPECT_EQ(sharpe.get_index(), reference_index);
    expect_values(sharpe, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   4.205259864302774, 2.1026299321513866,
                   5.083911274417939, 3.464101615137753});
}

TEST(RollingMetrics, SupportsAnnualizationFrequencyAndNamedColumns) {
    const auto returns = reference_series("strategy");

    const auto sharpe = fincore::roll_sharpe_ratio(
        returns, 3U, 0.0, fincore::frequency::daily, 100.0, "strategy");
    EXPECT_EQ(sharpe.get_index(),
              (std::vector<fincore::timestamp>{3, 4, 5, 6}));
    expect_values(sharpe, "strategy",
                  {2.649064714130087, 1.3245323570650434,
                   3.2025630761017414, 2.1821789023599227});

    const auto volatility = fincore::rolling_volatility(
        returns, 3U, fincore::frequency::weekly, std::nullopt, "strategy");
    EXPECT_EQ(volatility.get_index(), reference_index);
    expect_values(volatility, "strategy",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   .18147543451754933, .18147543451754933,
                   .1501110699893027, .11015141094572206});

    const auto rolling_sharpe = fincore::rolling_sharpe(
        returns, 3U, fincore::frequency::weekly, std::nullopt, "strategy");
    expect_values(rolling_sharpe, "strategy",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   1.9102677317636767, .9551338658818381,
                   2.309401076758502, 1.5735915849388855});
}

TEST(RollingMetrics, PreservesOriginalUnsortedInputOrder) {
    const auto returns = fincore::make_series(
        {30, 10, 20, 5, 50, 40}, "returns", reference_returns);

    EXPECT_EQ(fincore::roll_sharpe_ratio(returns, 3U).get_index(),
              (std::vector<fincore::timestamp>{20, 5, 50, 40}));
    EXPECT_EQ(fincore::roll_max_drawdown(returns, 3U).get_index(),
              (std::vector<fincore::timestamp>{20, 5, 50, 40}));
    EXPECT_EQ(fincore::rolling_volatility(returns, 3U).get_index(),
              returns.get_index());
    EXPECT_EQ(fincore::rolling_sharpe(returns, 3U).get_index(),
              returns.get_index());
}

TEST(RollingMetrics, HandlesShortAndExactlyOneCompleteWindow) {
    const auto short_series =
        fincore::make_series({1, 2}, "returns", {.01, -.02});
    expect_empty_typed_series(fincore::roll_sharpe_ratio(short_series, 3U),
                              "returns");
    expect_empty_typed_series(fincore::roll_max_drawdown(short_series, 3U),
                              "returns");
    const auto short_volatility = fincore::rolling_volatility(short_series, 3U);
    const auto short_sharpe = fincore::rolling_sharpe(short_series, 3U);
    EXPECT_EQ(short_volatility.get_index(), short_series.get_index());
    EXPECT_EQ(short_sharpe.get_index(), short_series.get_index());
    expect_values(short_volatility, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
    expect_values(short_sharpe, "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});

    const auto one_window =
        fincore::make_series({1, 2, 3}, "returns", {.01, -.02, .03});
    EXPECT_EQ(fincore::roll_sharpe_ratio(one_window, 3U).get_index(),
              (std::vector<fincore::timestamp>{3}));
    EXPECT_EQ(fincore::roll_max_drawdown(one_window, 3U).get_index(),
              (std::vector<fincore::timestamp>{3}));
    EXPECT_EQ(fincore::rolling_volatility(one_window, 3U).get_index(),
              one_window.get_index());
    EXPECT_EQ(fincore::rolling_sharpe(one_window, 3U).get_index(),
              one_window.get_index());
}

TEST(RollingMetrics, AppliesTheRequiredNanPolicies) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {.01, std::numeric_limits<double>::quiet_NaN(), -.01});

    expect_values(fincore::roll_sharpe_ratio(returns, 3U), "returns",
                  {0.0});
    expect_values(fincore::roll_max_drawdown(returns, 3U), "returns", {0.0});
    expect_values(fincore::rolling_volatility(returns, 3U), "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
    expect_values(fincore::rolling_sharpe(returns, 3U), "returns",
                  {std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN(),
                   std::numeric_limits<double>::quiet_NaN()});
}

TEST(RollingMetrics, AcceptsEmptyDoubleColumnsAndRejectsInvalidInputs) {
    const auto empty = fincore::make_series({}, "returns", {});
    expect_empty_typed_series(fincore::roll_sharpe_ratio(empty, 3U), "returns");
    expect_empty_typed_series(fincore::roll_max_drawdown(empty, 3U), "returns");
    expect_empty_typed_series(fincore::rolling_volatility(empty, 3U), "returns");
    expect_empty_typed_series(fincore::rolling_sharpe(empty, 3U), "returns");

    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::roll_sharpe_ratio(frame, 3U); });
    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::roll_max_drawdown(frame, 3U); });
    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::rolling_volatility(frame, 3U); });
    expect_input_errors(
        [](const fincore::dataframe& frame) { return fincore::rolling_sharpe(frame, 3U); });

    const auto returns = reference_series();
    EXPECT_THROW(fincore::roll_sharpe_ratio(returns, 0U), fincore::input_error);
    EXPECT_THROW(fincore::roll_max_drawdown(returns, 0U), fincore::input_error);
    EXPECT_THROW(fincore::rolling_volatility(returns, 0U), fincore::input_error);
    EXPECT_THROW(fincore::rolling_sharpe(returns, 0U), fincore::input_error);
}

}  // namespace
