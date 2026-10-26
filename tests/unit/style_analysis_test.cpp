#include <fincore/attribution/style.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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

constexpr double tolerance{1.0e-12};

constexpr std::array<std::string_view, 9U> oracle_style_names{
    fincore::style_large_column,
    fincore::style_mid_column,
    fincore::style_small_column,
    fincore::style_winner_column,
    fincore::style_loser_column,
    fincore::style_value_column,
    fincore::style_growth_column,
    fincore::style_high_column,
    fincore::style_low_column};

constexpr std::array<std::string_view, 7U> default_style_names{
    fincore::style_equal_weight_column,
    fincore::style_winner_column,
    fincore::style_loser_column,
    fincore::style_value_column,
    fincore::style_growth_column,
    fincore::style_high_column,
    fincore::style_low_column};

struct oracle_fixture {
    fincore::dataframe returns;
    std::vector<fincore::timestamp> index;
    std::vector<double> market_caps;
    std::vector<double> book_to_price;
    std::vector<double> value_scores;
};

oracle_fixture make_oracle_fixture() {
    constexpr std::size_t row_count{60U};
    std::vector<fincore::timestamp> index;
    std::vector<double> alpha;
    std::vector<double> beta;
    std::vector<double> gamma;
    index.reserve(row_count);
    alpha.reserve(row_count);
    beta.reserve(row_count);
    gamma.reserve(row_count);
    for (std::size_t row = 0U; row < row_count; ++row) {
        if (row == 0U) {
            index.push_back(42);
        } else if (row == 1U) {
            index.push_back(-7);
        } else if (row == 2U) {
            index.push_back(42);
        } else {
            index.push_back(static_cast<fincore::timestamp>(row + 100U));
        }
        alpha.push_back(row % 3U == 0U ? .010 : -.004);
        beta.push_back(row % 4U == 0U ? -.006 : .003);
        gamma.push_back(.002 + .0005 *
                             (static_cast<double>(row % 5U) - 2.0));
    }

    auto expected_index{index};
    fincore::dataframe returns;
    returns.load_index(std::move(index));
    returns.load_column<double>("ALPHA", std::move(alpha),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("BETA", std::move(beta),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("GAMMA", std::move(gamma),
                                hmdf::nan_policy::dont_pad_with_nans);
    return {std::move(returns), std::move(expected_index), {100.0, 40.0, 10.0},
            {.8, .2, .5}, {.2, .8, .5}};
}

fincore::style_analysis_options oracle_options(const oracle_fixture& fixture) {
    return {.market_caps = std::span<const double>{fixture.market_caps},
            .book_to_price = std::span<const double>{fixture.book_to_price},
            .value_scores = std::span<const double>{fixture.value_scores},
            .momentum_window = 5U,
            .size_quantiles = {.3, .7}};
}

void expect_ordinal_index(const std::vector<fincore::timestamp>& actual,
                          const std::size_t count) {
    ASSERT_EQ(actual.size(), count);
    for (std::size_t row = 0U; row < count; ++row) {
        EXPECT_EQ(actual[row], static_cast<fincore::timestamp>(row));
    }
}

void expect_exposure_schema(
    const fincore::dataframe& exposures,
    const std::span<const std::string_view> expected_style_names) {
    EXPECT_EQ(exposures.shape().second, expected_style_names.size() + 1U);
    const auto string_columns = exposures.get_columns_info<std::string>();
    ASSERT_EQ(string_columns.size(), 1U);
    EXPECT_EQ(std::string{std::get<0>(string_columns[0]).c_str()},
              std::string{fincore::style_asset_column});
    EXPECT_EQ(std::get<2>(string_columns[0]), typeid(std::string));

    const auto double_columns = exposures.get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), expected_style_names.size());
    for (std::size_t column = 0U; column < expected_style_names.size(); ++column) {
        EXPECT_EQ(std::string{std::get<0>(double_columns[column]).c_str()},
                  std::string{expected_style_names[column]});
        EXPECT_EQ(std::get<2>(double_columns[column]), typeid(double));
    }
}

void expect_summary_schema(
    const fincore::dataframe& summaries,
    const std::span<const std::string_view> expected_style_names) {
    EXPECT_EQ(summaries.shape().second, 2U);
    expect_ordinal_index(summaries.get_index(), expected_style_names.size());
    const auto string_columns = summaries.get_columns_info<std::string>();
    ASSERT_EQ(string_columns.size(), 1U);
    EXPECT_EQ(std::string{std::get<0>(string_columns[0]).c_str()},
              std::string{fincore::style_name_column});
    const auto double_columns = summaries.get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), 1U);
    EXPECT_EQ(std::string{std::get<0>(double_columns[0]).c_str()},
              std::string{fincore::style_return_column});

    const auto& labels = summaries.get_column<std::string>(
        fincore::style_name_column.data());
    ASSERT_EQ(labels.size(), expected_style_names.size());
    for (std::size_t row = 0U; row < expected_style_names.size(); ++row) {
        EXPECT_EQ(labels[row], expected_style_names[row]);
    }
}

std::vector<double> expected_overall_values() {
    constexpr std::size_t row_count{60U};
    std::vector<double> values;
    values.reserve(row_count);
    for (std::size_t row = 0U; row < row_count; ++row) {
        const double alpha{row % 3U == 0U ? .010 : -.004};
        const double beta{row % 4U == 0U ? -.006 : .003};
        const double gamma{.002 + .0005 *
                                      (static_cast<double>(row % 5U) - 2.0)};
        values.push_back((alpha + beta + gamma) / 3.0);
    }
    return values;
}

TEST(StyleAnalysis, PublishesPublicTypesAndDefaultCall) {
    const auto fixture = make_oracle_fixture();
    const style_analysis_function function = &fincore::style_analysis;
    const auto result = fincore::style_analysis(fixture.returns);
    const auto through_pointer = function(fixture.returns, {});

    EXPECT_EQ(result.overall_returns.get_index(), fixture.index);
    EXPECT_EQ(through_pointer.overall_returns.get_index(), fixture.index);
    expect_exposure_schema(result.exposures, default_style_names);
    expect_summary_schema(result.returns_by_style, default_style_names);
}

TEST(StyleAnalysis, MatchesPythonSourceOracleWithOrderedValueOwningFrames) {
    const auto fixture = make_oracle_fixture();
    const auto result = fincore::style_analysis(fixture.returns,
                                                oracle_options(fixture));

    expect_ordinal_index(result.exposures.get_index(), 3U);
    expect_exposure_schema(result.exposures, oracle_style_names);
    EXPECT_EQ(result.exposures.get_column<std::string>(
                  fincore::style_asset_column.data()),
              (std::vector<std::string>{"ALPHA", "BETA", "GAMMA"}));

    constexpr std::array<std::array<double, 3U>, 9U> expected_exposures{{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0},
        {0.0, .5, .5},
        {1.0, 0.0, 0.0},
        {0.0, .5, .5},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0},
        {.5, .5, 0.0},
    }};
    for (std::size_t style = 0U; style < oracle_style_names.size(); ++style) {
        const auto& values = result.exposures.get_column<double>(
            oracle_style_names[style].data());
        ASSERT_EQ(values.size(), 3U);
        for (std::size_t asset = 0U; asset < values.size(); ++asset) {
            EXPECT_NEAR(values[asset], expected_exposures[style][asset], tolerance);
        }
    }

    expect_summary_schema(result.returns_by_style, oracle_style_names);
    constexpr std::array<double, 9U> expected_summary{
        .0006666666666666668, .0007500000000000001, .002,
        .0013750000000000004, .0006666666666666668,
        .0013750000000000004, .0006666666666666668, .002,
        .0007083333333333334};
    const auto& summaries = result.returns_by_style.get_column<double>(
        fincore::style_return_column.data());
    ASSERT_EQ(summaries.size(), expected_summary.size());
    for (std::size_t style = 0U; style < summaries.size(); ++style) {
        EXPECT_NEAR(summaries[style], expected_summary[style], tolerance);
    }

    EXPECT_EQ(result.overall_returns.get_index(), fixture.index);
    EXPECT_EQ(result.overall_returns.shape().second, 1U);
    const auto& overall = result.overall_returns.get_column<double>(
        fincore::style_return_column.data());
    const auto expected_overall = expected_overall_values();
    ASSERT_EQ(overall.size(), expected_overall.size());
    for (std::size_t row = 0U; row < overall.size(); ++row) {
        EXPECT_NEAR(overall[row], expected_overall[row], tolerance);
    }
}

TEST(StyleAnalysis, UsesDefaultBookToPriceAndValueScorePrecedence) {
    const auto fixture = make_oracle_fixture();

    const auto defaults = fincore::style_analysis(fixture.returns);
    const auto& equal_weight = defaults.exposures.get_column<double>(
        fincore::style_equal_weight_column.data());
    const auto& default_value = defaults.exposures.get_column<double>(
        fincore::style_value_column.data());
    const auto& default_growth = defaults.exposures.get_column<double>(
        fincore::style_growth_column.data());
    for (std::size_t asset = 0U; asset < equal_weight.size(); ++asset) {
        EXPECT_NEAR(equal_weight[asset], 1.0 / 3.0, tolerance);
        EXPECT_NEAR(default_value[asset], 1.0 / 3.0, tolerance);
        EXPECT_NEAR(default_growth[asset], 1.0 / 3.0, tolerance);
    }

    const fincore::style_analysis_options book_to_price_options{
        .market_caps = std::span<const double>{fixture.market_caps},
        .book_to_price = std::span<const double>{fixture.book_to_price},
        .momentum_window = 5U,
        .size_quantiles = {.3, .7}};
    const auto book_to_price = fincore::style_analysis(
        fixture.returns, book_to_price_options);
    EXPECT_EQ(book_to_price.exposures.get_column<double>(
                  fincore::style_value_column.data()),
              (std::vector<double>{.5, 0.0, .5}));
    EXPECT_EQ(book_to_price.exposures.get_column<double>(
                  fincore::style_growth_column.data()),
              (std::vector<double>{0.0, 1.0, 0.0}));

    const auto value_scores = fincore::style_analysis(fixture.returns,
                                                       oracle_options(fixture));
    EXPECT_EQ(value_scores.exposures.get_column<double>(
                  fincore::style_value_column.data()),
              (std::vector<double>{0.0, .5, .5}));
    EXPECT_EQ(value_scores.exposures.get_column<double>(
                  fincore::style_growth_column.data()),
              (std::vector<double>{1.0, 0.0, 0.0}));
}

TEST(StyleAnalysis, DefaultsUse252MomentumAndMedianSizeQuantiles) {
    constexpr std::size_t row_count{253U};
    std::vector<fincore::timestamp> index;
    std::vector<double> alpha(row_count, 0.0);
    std::vector<double> beta(row_count, 0.0);
    std::vector<double> gamma(row_count, 0.0);
    index.reserve(row_count);
    for (std::size_t row = 0U; row < row_count; ++row) {
        index.push_back(static_cast<fincore::timestamp>(row));
    }
    alpha[0U] = -.50;
    beta[0U] = -.50;
    gamma[0U] = -.20;
    alpha[1U] = -.50;
    beta[1U] = -.20;
    gamma[1U] = -.50;
    alpha[2U] = -.20;
    beta[2U] = -.50;
    gamma[2U] = -.50;

    fincore::dataframe returns;
    returns.load_index(std::move(index));
    returns.load_column<double>("ALPHA", std::move(alpha),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("BETA", std::move(beta),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("GAMMA", std::move(gamma),
                                hmdf::nan_policy::dont_pad_with_nans);
    const std::vector<double> market_caps{1.0, 1.0, 2.0};

    const fincore::style_analysis_options default_options{
        .market_caps = std::span<const double>{market_caps}};
    const fincore::style_analysis_options explicit_options{
        .market_caps = std::span<const double>{market_caps},
        .momentum_window = 252U,
        .size_quantiles = {.5, .5}};
    EXPECT_EQ(default_options.momentum_window, 252U);
    EXPECT_EQ(default_options.size_quantiles, (std::array<double, 2U>{.5, .5}));
    const auto default_result = fincore::style_analysis(returns, default_options);
    const auto explicit_result = fincore::style_analysis(returns, explicit_options);

    EXPECT_EQ(default_result.exposures.get_column<double>(
                  fincore::style_large_column.data()),
              (std::vector<double>{1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}));
    EXPECT_EQ(default_result.exposures.get_column<double>(
                  fincore::style_mid_column.data()),
              (std::vector<double>{0.0, 0.0, 0.0}));
    EXPECT_EQ(default_result.exposures.get_column<double>(
                  fincore::style_small_column.data()),
              (std::vector<double>{0.0, 0.0, 0.0}));
    EXPECT_EQ(default_result.exposures.get_column<double>(
                  fincore::style_winner_column.data()),
              (std::vector<double>{.5, .5, 0.0}));
    EXPECT_EQ(default_result.exposures.get_column<double>(
                  fincore::style_loser_column.data()),
              (std::vector<double>{0.0, 0.0, 1.0}));

    for (const std::string_view style : oracle_style_names) {
        EXPECT_EQ(default_result.exposures.get_column<double>(style.data()),
                  explicit_result.exposures.get_column<double>(style.data()));
    }
}

TEST(StyleAnalysis, UsesTrailingSixtyObservationsForRollingVolatility) {
    constexpr std::size_t row_count{61U};
    std::vector<fincore::timestamp> index;
    std::vector<double> alpha(row_count, 0.0);
    std::vector<double> beta(row_count, 0.0);
    std::vector<double> gamma(row_count, 0.0);
    index.reserve(row_count);
    for (std::size_t row = 0U; row < row_count; ++row) {
        index.push_back(static_cast<fincore::timestamp>(row));
    }
    alpha[0U] = -2.0;
    alpha[40U] = -3.0;

    fincore::dataframe returns;
    returns.load_index(std::move(index));
    returns.load_column<double>("ALPHA", std::move(alpha),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("BETA", std::move(beta),
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("GAMMA", std::move(gamma),
                                hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::style_analysis(
        returns, {.momentum_window = row_count});
    EXPECT_EQ(result.exposures.get_column<double>(fincore::style_high_column.data()),
              (std::vector<double>{0.0, .5, .5}));
    EXPECT_EQ(result.exposures.get_column<double>(fincore::style_low_column.data()),
              (std::vector<double>{1.0, 0.0, 0.0}));
}

TEST(StyleAnalysis, PreservesShortHistoryZeroWeightNanStyleSummaries) {
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>{9, -4, 9});
    returns.load_column<double>("ALPHA", {.01, .02, -.01},
                                hmdf::nan_policy::dont_pad_with_nans);
    returns.load_column<double>("BETA", {-.02, .01, .03},
                                hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::style_analysis(returns);
    expect_exposure_schema(result.exposures, default_style_names);
    EXPECT_EQ(result.overall_returns.get_index(), returns.get_index());
    EXPECT_EQ(result.exposures.get_column<double>(fincore::style_high_column.data()),
              (std::vector<double>{0.0, 0.0}));
    EXPECT_EQ(result.exposures.get_column<double>(fincore::style_low_column.data()),
              (std::vector<double>{0.0, 0.0}));

    const auto& summary = result.returns_by_style.get_column<double>(
        fincore::style_return_column.data());
    ASSERT_EQ(summary.size(), default_style_names.size());
    EXPECT_TRUE(std::isnan(summary[5]));
    EXPECT_TRUE(std::isnan(summary[6]));
}

TEST(StyleAnalysis, RejectsStructuralSelectorAndFiniteBoundaries) {
    fincore::dataframe no_rows;
    no_rows.load_index(std::vector<fincore::timestamp>{});
    no_rows.load_column<double>("ALPHA", {}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(no_rows)),
                 fincore::input_error);

    fincore::dataframe no_columns;
    no_columns.load_index(std::vector<fincore::timestamp>{1});
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(no_columns)),
                 fincore::input_error);

    fincore::dataframe non_double;
    non_double.load_index(std::vector<fincore::timestamp>{1});
    non_double.load_column<int>("ALPHA", {1}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(non_double)),
                 fincore::input_error);

    fincore::dataframe empty_asset_name;
    empty_asset_name.load_index(std::vector<fincore::timestamp>{1});
    empty_asset_name.load_column<double>("", {.01},
                                              hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(empty_asset_name)),
                 fincore::input_error);

    fincore::dataframe mixed;
    mixed.load_index(std::vector<fincore::timestamp>{1});
    mixed.load_column<double>("ALPHA", {.01}, hmdf::nan_policy::dont_pad_with_nans);
    mixed.load_column<int>("metadata", {7}, hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(mixed)),
                 fincore::input_error);

    fincore::dataframe short_column;
    short_column.load_index(std::vector<fincore::timestamp>{1, 2});
    short_column.load_column<double>("ALPHA", {.01},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(short_column)),
                 fincore::input_error);

    for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()}) {
        const auto invalid_return = fincore::make_series({1}, "ALPHA", {invalid});
        EXPECT_THROW(static_cast<void>(fincore::style_analysis(invalid_return)),
                     fincore::input_error);
    }

    const auto fixture = make_oracle_fixture();
    const std::vector<double> mismatched{1.0, 2.0};
    const fincore::style_analysis_options mismatched_options{
        .market_caps = std::span<const double>{mismatched}};
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(
                     fixture.returns, mismatched_options)),
                 fincore::input_error);
    const fincore::style_analysis_options mismatched_book_to_price{
        .book_to_price = std::span<const double>{mismatched}};
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(
                     fixture.returns, mismatched_book_to_price)),
                 fincore::input_error);
    const fincore::style_analysis_options mismatched_value_scores{
        .value_scores = std::span<const double>{mismatched}};
    EXPECT_THROW(static_cast<void>(fincore::style_analysis(
                     fixture.returns, mismatched_value_scores)),
                 fincore::input_error);

    for (const double invalid : {std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()}) {
        const std::vector<double> invalid_caps{100.0, 40.0, invalid};
        const fincore::style_analysis_options options{
            .market_caps = std::span<const double>{invalid_caps}};
        EXPECT_THROW(static_cast<void>(fincore::style_analysis(fixture.returns, options)),
                     fincore::input_error);

        const std::vector<double> invalid_book_to_price{.8, .2, invalid};
        const fincore::style_analysis_options book_to_price_options{
            .book_to_price = std::span<const double>{invalid_book_to_price}};
        EXPECT_THROW(static_cast<void>(fincore::style_analysis(
                         fixture.returns, book_to_price_options)),
                     fincore::input_error);

        const std::vector<double> invalid_value_scores{.2, .8, invalid};
        const fincore::style_analysis_options value_score_options{
            .value_scores = std::span<const double>{invalid_value_scores}};
        EXPECT_THROW(static_cast<void>(fincore::style_analysis(
                         fixture.returns, value_score_options)),
                     fincore::input_error);
    }

    for (const std::array<double, 2U> quantiles :
         {std::array<double, 2U>{-.01, .7},
          std::array<double, 2U>{.3, 1.01},
          std::array<double, 2U>{std::numeric_limits<double>::quiet_NaN(), .7}}) {
        const fincore::style_analysis_options options{.size_quantiles = quantiles};
        EXPECT_THROW(static_cast<void>(fincore::style_analysis(fixture.returns, options)),
                     fincore::input_error);
    }
}

TEST(StyleAnalysis, OwnsEveryResultAfterSourceAndOptionMutation) {
    const auto retained = [] {
        auto fixture = make_oracle_fixture();
        const auto result = fincore::style_analysis(fixture.returns,
                                                    oracle_options(fixture));
        fixture.returns.get_column<double>("ALPHA")[0] = 42.0;
        fixture.market_caps[0] = -42.0;
        fixture.value_scores[1] = -42.0;
        return result;
    }();

    expect_ordinal_index(retained.exposures.get_index(), 3U);
    expect_exposure_schema(retained.exposures, oracle_style_names);
    expect_summary_schema(retained.returns_by_style, oracle_style_names);
    EXPECT_EQ(retained.overall_returns.get_index(),
              (std::vector<fincore::timestamp>{42, -7, 42, 103, 104, 105,
                                                106, 107, 108, 109, 110, 111,
                                                112, 113, 114, 115, 116, 117,
                                                118, 119, 120, 121, 122, 123,
                                                124, 125, 126, 127, 128, 129,
                                                130, 131, 132, 133, 134, 135,
                                                136, 137, 138, 139, 140, 141,
                                                142, 143, 144, 145, 146, 147,
                                                148, 149, 150, 151, 152, 153,
                                                154, 155, 156, 157, 158, 159}));
    EXPECT_NEAR(retained.exposures.get_column<double>(
                    fincore::style_large_column.data())[0],
                1.0, tolerance);
    EXPECT_NEAR(retained.returns_by_style.get_column<double>(
                    fincore::style_return_column.data())[8],
                .0007083333333333334, tolerance);
    EXPECT_NEAR(retained.overall_returns.get_column<double>(
                    fincore::style_return_column.data())[0],
                .0016666666666666663, tolerance);
}

}  // namespace
