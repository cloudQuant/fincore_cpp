#include <fincore/metrics/ratios.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using kappa_three_ratio_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<kappa_three_ratio_signature>(
                  &fincore::kappa_three_ratio)),
              kappa_three_ratio_signature>);

TEST(Ratios, PublishesExactKappaThreeSignatureAndDefaultCall) {
    const auto returns =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});

    EXPECT_DOUBLE_EQ(fincore::kappa_three_ratio(returns),
                     fincore::kappa_three_ratio(returns, 0.0, "returns"));
}

TEST(Ratios, MatchesPythonOracleForOrdinaryAndNonzeroMar) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_NEAR(fincore::kappa_three_ratio(returns), 0.1587401051968199,
                1e-15);
    EXPECT_NEAR(fincore::kappa_three_ratio(returns, 0.1),
                -0.9280157765364051, 1e-15);
}

TEST(Ratios, PreservesRawAndCleanObservationCountGates) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto one_clean =
        fincore::make_series({1, 2}, "returns", {nan, 0.01});
    const auto all_nan = fincore::make_series({1, 2}, "returns", {nan, nan});

    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(empty)));
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(singleton)));
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(one_clean)));
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(all_nan)));
}

TEST(Ratios, RemovesNanButRetainsPositiveAndNegativeInfinity) {
    const auto nan_mixed =
        fincore::make_series({1, 2, 3, 4, 5}, "returns",
                             {nan, 0.01, -0.02, nan, 0.03});
    const double positive_infinity = std::numeric_limits<double>::infinity();
    const auto positive_inf = fincore::make_series(
        {1, 2, 3}, "returns", {positive_infinity, 0.01, -0.02});
    const auto negative_inf = fincore::make_series(
        {1, 2, 3}, "returns", {-positive_infinity, 0.01, -0.02});
    const auto both_inf = fincore::make_series(
        {1, 2, 3}, "returns", {positive_infinity, -positive_infinity, 0.01});

    EXPECT_NEAR(fincore::kappa_three_ratio(nan_mixed), 0.4807498567691359,
                1e-15);
    const double positive_result = fincore::kappa_three_ratio(positive_inf);
    EXPECT_TRUE(std::isinf(positive_result));
    EXPECT_GT(positive_result, 0.0);
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(negative_inf)));
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(both_inf)));
}

TEST(Ratios, UsesZeroRiskBranchAndStrictLpm3Threshold) {
    const auto all_positive =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    const auto equal_mar = fincore::make_series({1, 2}, "returns", {0.0, 0.0});
    const double below = std::nextafter(1e-10, 0.0);
    const auto strict_below_negative =
        fincore::make_series({1, 2}, "returns", {-below, -below});
    const auto strict_below_positive_mu =
        fincore::make_series({1, 2}, "returns", {-below, 1.0});
    const auto exact_threshold =
        fincore::make_series({1, 2}, "returns", {-1e-10, -1e-10});

    const double all_positive_result = fincore::kappa_three_ratio(all_positive);
    EXPECT_TRUE(std::isinf(all_positive_result));
    EXPECT_GT(all_positive_result, 0.0);
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(equal_mar)));
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(strict_below_negative)));
    const double positive_mu_result =
        fincore::kappa_three_ratio(strict_below_positive_mu);
    EXPECT_TRUE(std::isinf(positive_mu_result));
    EXPECT_GT(positive_mu_result, 0.0);
    EXPECT_NEAR(fincore::kappa_three_ratio(exact_threshold),
                -0.9999999999999987, 2e-16);
}

TEST(Ratios, PreservesNonfiniteMarAndSameSignedInfinitySemantics) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns", {0.01, -0.02, 0.03});
    const auto same_positive_infinity =
        fincore::make_series({1, 2}, "returns", {infinity, 0.0});
    const auto same_negative_infinity =
        fincore::make_series({1, 2}, "returns", {-infinity, 0.0});

    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(returns, nan)));
    EXPECT_TRUE(std::isnan(fincore::kappa_three_ratio(returns, infinity)));
    const double negative_infinite_mar =
        fincore::kappa_three_ratio(returns, -infinity);
    EXPECT_TRUE(std::isinf(negative_infinite_mar));
    EXPECT_GT(negative_infinite_mar, 0.0);
    EXPECT_TRUE(std::isnan(
        fincore::kappa_three_ratio(same_positive_infinity, infinity)));
    EXPECT_TRUE(std::isnan(
        fincore::kappa_three_ratio(same_negative_infinity, -infinity)));
}

TEST(Ratios, PreservesNegativeZeroWhenDownsideCubeOverflows) {
    const double maximum = std::numeric_limits<double>::max();
    const auto returns = fincore::make_series({1, 2}, "returns", {-maximum, 0.0});

    const double result = fincore::kappa_three_ratio(returns);

    EXPECT_EQ(result, 0.0);
    EXPECT_TRUE(std::signbit(result));
}

TEST(Ratios, UsesOriginalReturnOrderAndIgnoresDuplicateNonmonotonicLabels) {
    const auto regular_index = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
    const auto duplicate_nonmonotonic = fincore::make_series(
        {3, 1, 3, 2}, "returns", {0.01, -0.02, 0.03, -0.01});
    const auto cancellation_order =
        fincore::make_series({30, 10, 20}, "returns", {1e16, 1.0, -1e16});
    const auto retained_unit_last_order =
        fincore::make_series({30, 10, 20}, "returns", {1e16, -1e16, 1.0});

    EXPECT_NEAR(fincore::kappa_three_ratio(duplicate_nonmonotonic),
                0.19078570709222184, 1e-15);
    EXPECT_DOUBLE_EQ(fincore::kappa_three_ratio(regular_index),
                     fincore::kappa_three_ratio(duplicate_nonmonotonic));
    EXPECT_DOUBLE_EQ(fincore::kappa_three_ratio(cancellation_order), 0.0);
    EXPECT_GT(fincore::kappa_three_ratio(retained_unit_last_order), 0.0);
    EXPECT_NE(fincore::kappa_three_ratio(cancellation_order),
              fincore::kappa_three_ratio(retained_unit_last_order));
}

TEST(Ratios, SupportsNamedAndBoundedColumnSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4}, "strategy", {0.01, -0.02, 0.03, -0.01});
    const auto portfolio = fincore::make_series(
        {1, 2, 3, 4}, "portfolio", {0.01, -0.02, 0.03, -0.01});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    constexpr std::array<char, 9> bounded_portfolio_name{
        'p', 'o', 'r', 't', 'f', 'o', 'l', 'i', 'o'};
    const std::string_view bounded_strategy{bounded_name.data(),
                                            bounded_name.size()};
    const std::string_view bounded_portfolio{bounded_portfolio_name.data(),
                                             bounded_portfolio_name.size()};

    EXPECT_NEAR(fincore::kappa_three_ratio(strategy, 0.0, "strategy"),
                0.19078570709222184, 1e-15);
    EXPECT_DOUBLE_EQ(fincore::kappa_three_ratio(strategy, 0.0, bounded_strategy),
                     fincore::kappa_three_ratio(strategy, 0.0, "strategy"));
    EXPECT_DOUBLE_EQ(
        fincore::kappa_three_ratio(portfolio, 0.0, bounded_portfolio),
        fincore::kappa_three_ratio(portfolio, 0.0, "portfolio"));
}

TEST(Ratios, ValidatesSelectorsAndDataframeStructureBeforeRawCount) {
    fincore::dataframe empty_missing_column;
    empty_missing_column.load_index(std::vector<fincore::timestamp>{});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const std::array<char, 13> embedded_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l', '!'};
    const std::string_view embedded_selector{embedded_storage.data(),
                                             embedded_storage.size()};

    const auto expect_error_message = [](const auto& call,
                                         const char* expected_message) {
        try {
            static_cast<void>(call());
            ADD_FAILURE() << "expected fincore::input_error";
        } catch (const fincore::input_error& error) {
            EXPECT_STREQ(error.what(), expected_message);
        }
    };

    EXPECT_THROW(static_cast<void>(fincore::kappa_three_ratio(empty_missing_column)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::kappa_three_ratio(singleton, 0.0, "")),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::kappa_three_ratio(singleton, 0.0, "missing")),
                 fincore::input_error);

    const auto valid_prefix_frame =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    expect_error_message(
        [&] {
            return fincore::kappa_three_ratio(valid_prefix_frame, 0.0,
                                               embedded_selector);
        },
        "column name must not contain an embedded NUL");

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::kappa_three_ratio(wrong_type)),
                 fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{0.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::kappa_three_ratio(malformed)),
                 fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::kappa_three_ratio(malformed, 0.0,
                                               embedded_selector);
        },
        "column name must not contain an embedded NUL");
    expect_error_message(
        [&] { return fincore::kappa_three_ratio(malformed, nan); },
        "required double column must match the frame index length");
}

TEST(Ratios, ReturnsAnOwnedScalarAfterInputFrameAndSelectorLifetimesEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
        return fincore::kappa_three_ratio(returns, 0.0, selector);
    }();

    EXPECT_NEAR(result, 0.19078570709222184, 1e-15);
}

TEST(Ratios, AnnualizesSharpeFromSampleStandardDeviation) {
    const auto returns = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, -0.01});
    EXPECT_NEAR(fincore::sharpe_ratio(returns, 0.0, 252),
                6.928203230275507, 1e-12);
}

TEST(Ratios, MatchesReferenceSharpeSortinoAndCalmar) {
    const auto returns = fincore::make_series({1, 2, 3, 4}, "returns",
                                              {0.01, -0.02, 0.03, -0.01});
    EXPECT_NEAR(fincore::sharpe_ratio(returns), 1.789801617640478, 1e-12);
    EXPECT_NEAR(fincore::sortino_ratio(returns), 3.549647869859768, 1e-12);
    EXPECT_NEAR(fincore::calmar_ratio(returns), 39.58220278509036, 1e-12);
}

TEST(Ratios, IgnoresNanAndSupportsNamedReturnColumn) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "strategy",
        {0.01, std::numeric_limits<double>::quiet_NaN(), 0.03});
    EXPECT_NEAR(fincore::sharpe_ratio(returns, 0.0, 252, "strategy"),
                22.44994432064365, 1e-12);
}

TEST(Ratios, ReturnsNanForUndefinedRatioAndMissingColumnThrows) {
    const auto flat = fincore::make_series({1, 2}, "returns", {0.01, 0.01});
    const auto gains = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    EXPECT_TRUE(std::isinf(fincore::sharpe_ratio(flat)));
    EXPECT_GT(fincore::sharpe_ratio(flat), 0.0);
    EXPECT_TRUE(std::isnan(fincore::calmar_ratio(gains)));
    EXPECT_THROW(fincore::sortino_ratio(gains, 0.0, 252, "missing"),
                 fincore::input_error);
}

TEST(Ratios, PreservesOriginalLengthForCalmarAnnualizationWithNan) {
    const auto returns = fincore::make_series(
        {1, 2, 3}, "returns",
        {-0.02, std::numeric_limits<double>::quiet_NaN(), 0.01});

    EXPECT_NEAR(fincore::calmar_ratio(returns), -28.86726241438432, 1e-12);
}

TEST(Ratios, ReturnsSignedInfinityForZeroDownsideSortino) {
    const auto returns = fincore::make_series(
        {1, 2}, "returns", {0.01, std::numeric_limits<double>::quiet_NaN()});

    EXPECT_TRUE(std::isinf(fincore::sortino_ratio(returns)));
    EXPECT_GT(fincore::sortino_ratio(returns), 0.0);
}

}  // namespace
