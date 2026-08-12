#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using style_analysis_function = fincore::style_analysis_result (*) (
    const fincore::dataframe&, const fincore::style_analysis_options&);

static_assert(std::is_same_v<decltype(&fincore::style_analysis),
                             style_analysis_function>);
static_assert(std::is_same_v<
              decltype(fincore::style_analysis(
                  std::declval<const fincore::dataframe&>())),
              fincore::style_analysis_result>);

constexpr std::array<std::string_view, 9U> style_names{
    fincore::style_large_column,
    fincore::style_mid_column,
    fincore::style_small_column,
    fincore::style_winner_column,
    fincore::style_loser_column,
    fincore::style_value_column,
    fincore::style_growth_column,
    fincore::style_high_column,
    fincore::style_low_column};

TEST(AttributionStyleAnalysisUmbrella,
     ExportsValueOwningStyleAnalysisThroughAttributionFacade) {
    EXPECT_EQ(fincore::style_asset_column, "asset");
    EXPECT_EQ(fincore::style_name_column, "style");
    EXPECT_EQ(fincore::style_return_column, "return");

    constexpr std::size_t row_count{60U};
    std::vector<fincore::timestamp> index;
    std::vector<double> alpha;
    std::vector<double> beta;
    std::vector<double> gamma;
    index.reserve(row_count);
    alpha.reserve(row_count);
    beta.reserve(row_count);
    gamma.reserve(row_count);
    for (std::size_t row{0U}; row < row_count; ++row) {
        index.push_back(row == 0U ? 42 : row == 1U ? -7 : row == 2U ? 42 :
                        static_cast<fincore::timestamp>(row + 100U));
        alpha.push_back(row % 3U == 0U ? .010 : -.004);
        beta.push_back(row % 4U == 0U ? -.006 : .003);
        gamma.push_back(.002 + .0005 *
                                   (static_cast<double>(row % 5U) - 2.0));
    }

    fincore::dataframe returns;
    returns.load_index(std::move(index));
    returns.load_column<double>("ALPHA", std::move(alpha),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("BETA", std::move(beta),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("GAMMA", std::move(gamma),
                                hmdf::nan_policy::dont_pad_with_nans);
    const std::vector<double> market_caps{100.0, 40.0, 10.0};
    const std::vector<double> value_scores{.2, .8, .5};
    const fincore::style_analysis_options options{
        .market_caps = std::span<const double>{market_caps},
        .value_scores = std::span<const double>{value_scores},
        .momentum_window = 5U,
        .size_quantiles = {.3, .7}};

    const style_analysis_function function = &fincore::style_analysis;
    const auto result = function(returns, options);
    const auto default_result = fincore::style_analysis(returns);

    EXPECT_EQ(result.exposures.shape(),
              (std::pair<std::size_t, std::size_t>{3U, 10U}));
    EXPECT_EQ(result.exposures.get_index(),
              (std::vector<fincore::timestamp>{0, 1, 2}));
    EXPECT_EQ(result.exposures.get_column<std::string>(
                  fincore::style_asset_column.data()),
              (std::vector<std::string>{"ALPHA", "BETA", "GAMMA"}));
    EXPECT_EQ(result.exposures.get_columns_info<std::string>().size(), 1U);
    const auto exposure_columns = result.exposures.get_columns_info<double>();
    ASSERT_EQ(exposure_columns.size(), style_names.size());
    for (std::size_t position{0U}; position < style_names.size(); ++position) {
        EXPECT_EQ(std::string{std::get<0>(exposure_columns[position]).c_str()},
                  std::string{style_names[position]});
        EXPECT_EQ(std::get<1>(exposure_columns[position]), 3U);
        EXPECT_EQ(std::get<2>(exposure_columns[position]), typeid(double));
    }
    EXPECT_EQ(result.exposures.get_column<double>(
                  fincore::style_large_column.data()),
              (std::vector<double>{1.0, 0.0, 0.0}));
    EXPECT_EQ(result.exposures.get_column<double>(
                  fincore::style_winner_column.data()),
              (std::vector<double>{0.0, .5, .5}));

    EXPECT_EQ(result.returns_by_style.shape(),
              (std::pair<std::size_t, std::size_t>{9U, 2U}));
    EXPECT_EQ(result.returns_by_style.get_index(),
              (std::vector<fincore::timestamp>{0, 1, 2, 3, 4, 5, 6, 7, 8}));
    EXPECT_EQ(result.returns_by_style.get_columns_info<std::string>().size(), 1U);
    EXPECT_EQ(result.returns_by_style.get_columns_info<double>().size(), 1U);
    EXPECT_EQ(result.returns_by_style.get_column<std::string>(
                  fincore::style_name_column.data()),
              (std::vector<std::string>{"large", "mid", "small", "winner",
                                        "loser", "value", "growth", "high",
                                        "low"}));
    const auto& style_returns = result.returns_by_style.get_column<double>(
        fincore::style_return_column.data());
    EXPECT_NEAR(style_returns.front(), .0006666666666666668, 1.0e-12);
    EXPECT_NEAR(style_returns.back(), .0007083333333333334, 1.0e-12);

    EXPECT_EQ(result.overall_returns.shape(),
              (std::pair<std::size_t, std::size_t>{60U, 1U}));
    EXPECT_EQ(result.overall_returns.get_index().front(), 42);
    EXPECT_EQ(result.overall_returns.get_index()[1], -7);
    EXPECT_EQ(result.overall_returns.get_index()[2], 42);
    EXPECT_EQ(result.overall_returns.get_index().back(), 159);
    EXPECT_EQ(result.overall_returns.get_columns_info<double>().size(), 1U);
    EXPECT_NEAR(result.overall_returns.get_column<double>(
                    fincore::style_return_column.data()).front(),
                .0016666666666666663, 1.0e-12);

    EXPECT_EQ(default_result.exposures.shape().second, 8U);
    EXPECT_TRUE(default_result.exposures.has_column(
        fincore::style_equal_weight_column.data()));
}

}  // namespace
