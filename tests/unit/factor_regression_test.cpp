#include <fincore/attribution/factor_regression.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using fincore::timestamp;

constexpr std::array<std::string_view, 3U> factor_names{"MKT", "SMB", "HML"};
constexpr std::array<std::string_view, 3U> reordered_factor_names{
    "HML", "MKT", "SMB"};
constexpr std::array<timestamp, 8U> oracle_index{
    1704067200000000000LL, 1704153600000000000LL, 1704240000000000000LL,
    1704326400000000000LL, 1704412800000000000LL, 1704499200000000000LL,
    1704585600000000000LL, 1704672000000000000LL};

struct factor_fixture {
    fincore::dataframe returns;
    fincore::dataframe factors;
};

fincore::dataframe make_returns(std::vector<timestamp> index,
                                std::vector<double> values,
                                std::string_view name = "returns") {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>(std::string{name}.c_str(), std::move(values),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe make_factors(std::vector<timestamp> index,
                                std::vector<double> mkt,
                                std::vector<double> smb,
                                std::vector<double> hml) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>("MKT", std::move(mkt),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("SMB", std::move(smb),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("HML", std::move(hml),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

factor_fixture oracle_fixture() {
    const std::vector<double> mkt{.01, -.02, .03, .00, .015, -.01, .025, .005};
    const std::vector<double> smb{.02, .01, -.01, .03, .00, .015, -.02, .01};
    const std::vector<double> hml{.00, .02, .01, -.01, .025, -.005, .015, -.02};
    const std::vector<double> residual{.0005, -.001, .0, .0015,
                                       -.0005, .00025, -.00075, .00025};
    std::vector<double> values;
    values.reserve(oracle_index.size());
    for (std::size_t row = 0; row < oracle_index.size(); ++row) {
        values.push_back(.001 + 1.5 * mkt[row] - .5 * smb[row] +
                         .25 * hml[row] + residual[row]);
    }

    auto factors = make_factors(
        std::vector<timestamp>{oracle_index.begin(), oracle_index.end()}, mkt, smb,
        hml);
    factors.load_column<int>("unrelated_malformed", {7},
                             hmdf::nan_policy::dont_pad_with_nans);
    return {make_returns(std::vector<timestamp>{oracle_index.begin(), oracle_index.end()},
                         std::move(values)),
            std::move(factors)};
}

void expect_coefficient_schema(const fincore::factor_regression_result& result,
                               std::span<const std::string_view> expected_names) {
    const auto string_columns = result.coefficients.get_columns_info<std::string>();
    ASSERT_EQ(string_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(string_columns[0]).c_str(),
                 std::string{fincore::factor_regression_factor_column}.c_str());
    EXPECT_EQ(std::get<2>(string_columns[0]), typeid(std::string));

    const auto double_columns = result.coefficients.get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(double_columns[0]).c_str(),
                 std::string{fincore::factor_regression_coefficient_column}.c_str());
    EXPECT_EQ(std::get<2>(double_columns[0]), typeid(double));

    ASSERT_EQ(result.coefficients.get_index().size(), expected_names.size() + 1U);
    for (std::size_t row = 0; row < result.coefficients.get_index().size(); ++row) {
        EXPECT_EQ(result.coefficients.get_index()[row],
                  static_cast<timestamp>(row));
    }
    const auto& labels = result.coefficients.get_column<std::string>(
        fincore::factor_regression_factor_column.data());
    ASSERT_EQ(labels.size(), expected_names.size() + 1U);
    EXPECT_EQ(labels[0], fincore::factor_regression_alpha_name);
    for (std::size_t row = 0; row < expected_names.size(); ++row) {
        EXPECT_EQ(labels[row + 1U], expected_names[row]);
    }
}

void expect_single_double_schema(const fincore::dataframe& frame,
                                 std::string_view expected_name,
                                 const std::vector<timestamp>& expected_index) {
    const auto columns = frame.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(columns[0]).c_str(), std::string{expected_name}.c_str());
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
    EXPECT_EQ(frame.get_index(), expected_index);
}

TEST(FactorRegression, MatchesFamaFrenchFitOracleAndOwnsTypedResults) {
    auto fixture = oracle_fixture();
    const auto expected_index = fixture.returns.get_index();
    const auto result = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);

    expect_coefficient_schema(result, factor_names);
    expect_single_double_schema(result.fitted_returns,
                                fincore::factor_regression_fitted_column,
                                expected_index);
    expect_single_double_schema(result.residuals,
                                fincore::factor_regression_residual_column,
                                expected_index);

    const auto& coefficients = result.coefficients.get_column<double>(
        fincore::factor_regression_coefficient_column.data());
    ASSERT_EQ(coefficients.size(), 4U);
    EXPECT_NEAR(coefficients[0], .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(coefficients[1], 1.5310634180729992, 1.0e-12);
    EXPECT_NEAR(coefficients[2], -.45298603018400957, 1.0e-12);
    EXPECT_NEAR(coefficients[3], .23286251744428613, 1.0e-12);
    EXPECT_NEAR(result.alpha, coefficients[0], 1.0e-12);
    EXPECT_NEAR(result.r_squared, .9999467109765429, 1.0e-12);

    constexpr std::array<double, 8U> expected_residuals{
        -.0003203580214942679, -.00007556613003010387,
        .00014016753708259722, .00034876163551868407,
        -.00010695865164660895, .00020029277626710043,
        .00010131173838608121, -.00028765088408364426};
    const auto& residuals = result.residuals.get_column<double>(
        fincore::factor_regression_residual_column.data());
    const auto& fitted = result.fitted_returns.get_column<double>(
        fincore::factor_regression_fitted_column.data());
    const auto& returns = fixture.returns.get_column<double>("returns");
    ASSERT_EQ(residuals.size(), expected_residuals.size());
    ASSERT_EQ(fitted.size(), expected_residuals.size());
    for (std::size_t row = 0; row < expected_residuals.size(); ++row) {
        EXPECT_NEAR(residuals[row], expected_residuals[row], 1.0e-12);
        EXPECT_NEAR(fitted[row] + residuals[row], returns[row], 1.0e-12);
    }

    fixture.returns.get_column<double>("returns")[0] = 99.0;
    fixture.factors.get_column<double>("MKT")[0] = -99.0;
    EXPECT_NEAR(coefficients[0], .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(fitted[0], .006820358021494268, 1.0e-12);
    EXPECT_NEAR(residuals[0], expected_residuals[0], 1.0e-12);
}

TEST(FactorRegression,
     PreservesSameNonMonotonicUniqueRawTimestampOrderInOutputSeries) {
    const std::vector<timestamp> source_order{
        1704240000000000000LL, 1704067200000000000LL,
        1704412800000000000LL, 1704153600000000000LL,
        1704326400000000000LL};
    const std::vector<double> source_returns{.071, .028, .1215, .049, .0985};
    const auto returns = make_returns(source_order, source_returns);
    const auto factors = make_factors(
        source_order, {.03, .01, .05, .02, .04}, {.02, .01, .03, .00, .04},
        {.00, .02, -.01, .01, -.02});
    constexpr std::array<std::string_view, 1U> one_factor{"MKT"};

    const auto result =
        fincore::fit_factor_regression(returns, factors, one_factor);

    EXPECT_EQ(result.fitted_returns.get_index(), source_order);
    EXPECT_EQ(result.residuals.get_index(), source_order);
    const auto& fitted = result.fitted_returns.get_column<double>(
        fincore::factor_regression_fitted_column.data());
    const auto& residuals = result.residuals.get_column<double>(
        fincore::factor_regression_residual_column.data());
    ASSERT_EQ(fitted.size(), source_returns.size());
    ASSERT_EQ(residuals.size(), source_returns.size());
    for (std::size_t row = 0; row < source_returns.size(); ++row) {
        EXPECT_NEAR(fitted[row] + residuals[row], source_returns[row], 1.0e-12);
    }
}

TEST(FactorRegression, PreservesRequestedFactorOrderAndAcceptsExactFits) {
    const auto fixture = oracle_fixture();
    const auto reordered = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, reordered_factor_names);
    expect_coefficient_schema(reordered, reordered_factor_names);
    const auto& reordered_coefficients = reordered.coefficients.get_column<double>(
        fincore::factor_regression_coefficient_column.data());
    ASSERT_EQ(reordered_coefficients.size(), 4U);
    EXPECT_NEAR(reordered_coefficients[0], .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(reordered_coefficients[1], .23286251744428613, 1.0e-12);
    EXPECT_NEAR(reordered_coefficients[2], 1.5310634180729992, 1.0e-12);
    EXPECT_NEAR(reordered_coefficients[3], -.45298603018400957, 1.0e-12);

    const auto exact_returns = make_returns({1, 2}, {3.0, 5.0});
    const auto exact_factors = make_factors({1, 2}, {1.0, 2.0}, {0.0, 1.0},
                                            {1.0, 0.0});
    constexpr std::array<std::string_view, 1U> one_factor{"MKT"};
    const auto exact = fincore::fit_factor_regression(exact_returns, exact_factors,
                                                       one_factor);
    EXPECT_NEAR(exact.alpha, 1.0, 1.0e-12);
    EXPECT_NEAR(exact.coefficients.get_column<double>(
                    fincore::factor_regression_coefficient_column.data())[1],
                2.0, 1.0e-12);
    EXPECT_NEAR(exact.r_squared, 1.0, 1.0e-12);
}

TEST(FactorRegression, ReportsZeroRSquaredForConstantReturnsWithFullRankDesign) {
    const auto returns = make_returns({1, 2, 3}, {.1, .1, .1});
    const auto factors = make_factors({1, 2, 3}, {1.0, 2.0, 3.0},
                                      {0.0, 1.0, 0.0}, {1.0, 0.0, 1.0});
    constexpr std::array<std::string_view, 1U> one_factor{"MKT"};
    const auto result = fincore::fit_factor_regression(returns, factors, one_factor);
    EXPECT_DOUBLE_EQ(result.r_squared, .0);
    EXPECT_NEAR(result.alpha, .1, 1.0e-12);
    EXPECT_NEAR(result.coefficients.get_column<double>(
                    fincore::factor_regression_coefficient_column.data())[1],
                .0, 1.0e-12);
}

TEST(FactorRegression, RejectsInvalidNamesIndexesAndSelectedColumns) {
    const auto fixture = oracle_fixture();
    constexpr std::array<std::string_view, 0U> no_factors{};
    constexpr std::array<std::string_view, 1U> empty_factor{""};
    constexpr std::array<std::string_view, 2U> duplicate_factor{"MKT", "MKT"};
    constexpr std::array<std::string_view, 1U> alpha_factor{"alpha"};
    constexpr std::array<std::string_view, 1U> missing_factor{"missing"};
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                no_factors),
                 fincore::input_error);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                empty_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                duplicate_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                alpha_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                missing_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                factor_names, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, fixture.factors,
                                                factor_names, "missing_returns"),
                 fincore::input_error);

    auto wrong_return_type = fixture.returns;
    wrong_return_type.remove_column<double>("returns");
    wrong_return_type.load_column<int>("returns", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::fit_factor_regression(wrong_return_type, fixture.factors,
                                                factor_names),
                 fincore::input_error);

    auto wrong_factor_type = fixture.factors;
    wrong_factor_type.remove_column<double>("MKT");
    wrong_factor_type.load_column<int>("MKT", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, wrong_factor_type,
                                                factor_names),
                 fincore::input_error);

    auto short_factor = fixture.factors;
    short_factor.remove_column<double>("SMB");
    short_factor.load_column<double>("SMB", {.1},
                                         hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, short_factor,
                                                factor_names),
                 fincore::input_error);

    auto short_returns = fixture.returns;
    short_returns.remove_column<double>("returns");
    short_returns.load_column<double>("returns", {.1},
                                          hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::fit_factor_regression(short_returns, fixture.factors,
                                                factor_names),
                 fincore::input_error);

    auto duplicate_returns_index = fixture.returns;
    duplicate_returns_index.load_index(
        {oracle_index[0], oracle_index[0], oracle_index[2], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::fit_factor_regression(duplicate_returns_index,
                                                fixture.factors, factor_names),
                 fincore::input_error);

    auto duplicate_factor_index = fixture.factors;
    duplicate_factor_index.load_index(
        {oracle_index[0], oracle_index[1], oracle_index[1], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns,
                                                duplicate_factor_index, factor_names),
                 fincore::input_error);

    auto mismatched_index = fixture.factors;
    mismatched_index.load_index(
        {oracle_index[1], oracle_index[0], oracle_index[2], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns, mismatched_index,
                                                factor_names),
                 fincore::input_error);
}

TEST(FactorRegression, RejectsInsufficientNonfiniteAndRankDeficientInputs) {
    const auto fixture = oracle_fixture();
    const auto too_short_returns = make_returns({1, 2, 3}, {.01, .02, .03});
    const auto too_short_factors = make_factors(
        {1, 2, 3}, {.01, .02, .03}, {.02, .03, .04}, {.03, .04, .05});
    EXPECT_THROW(fincore::fit_factor_regression(too_short_returns,
                                                too_short_factors, factor_names),
                 fincore::input_error);

    auto nan_returns = fixture.returns;
    nan_returns.get_column<double>("returns")[0] =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::fit_factor_regression(nan_returns, fixture.factors,
                                                factor_names),
                 fincore::input_error);

    auto positive_infinite_factor = fixture.factors;
    positive_infinite_factor.get_column<double>("MKT")[0] =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns,
                                                positive_infinite_factor, factor_names),
                 fincore::input_error);

    auto negative_infinite_factor = fixture.factors;
    negative_infinite_factor.get_column<double>("SMB")[0] =
        -std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns,
                                                negative_infinite_factor, factor_names),
                 fincore::input_error);

    auto rank_deficient_factors = fixture.factors;
    rank_deficient_factors.get_column<double>("SMB") =
        rank_deficient_factors.get_column<double>("MKT");
    EXPECT_THROW(fincore::fit_factor_regression(fixture.returns,
                                                rank_deficient_factors, factor_names),
                 fincore::input_error);
}

TEST(FactorRegression, ResultsSurviveSourceDestruction) {
    const auto result = [] {
        auto fixture = oracle_fixture();
        std::array<std::string, 3U> source_factor_names{"MKT", "SMB", "HML"};
        const std::array<std::string_view, 3U> source_factor_name_views{
            source_factor_names[0], source_factor_names[1], source_factor_names[2]};
        auto output = fincore::fit_factor_regression(
            fixture.returns, fixture.factors, source_factor_name_views);
        fixture.returns.get_column<double>("returns")[0] = 123.0;
        fixture.factors.get_column<double>("MKT")[0] = -123.0;
        fixture.returns.load_index({11, 12, 13, 14, 15, 16, 17, 18});
        fixture.factors.load_index({21, 22, 23, 24, 25, 26, 27, 28});
        source_factor_names[0] = "mutated-market";
        return output;
    }();
    EXPECT_NEAR(result.alpha, .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(result.coefficients.get_column<double>(
                    fincore::factor_regression_coefficient_column.data())[0],
                .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(result.residuals.get_column<double>(
                    fincore::factor_regression_residual_column.data())[0],
                -.0003203580214942679, 1.0e-12);
    EXPECT_NEAR(result.fitted_returns.get_column<double>(
                    fincore::factor_regression_fitted_column.data())[0],
                .006820358021494268, 1.0e-12);
    const std::vector<timestamp> expected_raw_index{oracle_index.begin(),
                                                     oracle_index.end()};
    const std::vector<timestamp> expected_coefficient_index{0, 1, 2, 3};
    EXPECT_EQ(result.fitted_returns.get_index(), expected_raw_index);
    EXPECT_EQ(result.residuals.get_index(), expected_raw_index);
    EXPECT_EQ(result.coefficients.get_index(), expected_coefficient_index);
    const auto& labels = result.coefficients.get_column<std::string>(
        fincore::factor_regression_factor_column.data());
    ASSERT_EQ(labels.size(), 4U);
    EXPECT_EQ(labels[0], fincore::factor_regression_alpha_name);
    EXPECT_EQ(labels[1], "MKT");
    EXPECT_EQ(labels[2], "SMB");
    EXPECT_EQ(labels[3], "HML");
}

}  // namespace
