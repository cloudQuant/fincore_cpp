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
using adjusted_sharpe_ratio_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);
using mar_ratio_signature =
    double (*)(const fincore::dataframe&, double, std::string_view);
using conditional_sharpe_ratio_signature =
    double (*)(const fincore::dataframe&, double, double, double,
               std::string_view);
using deflated_sharpe_ratio_signature =
    double (*)(const fincore::dataframe&, double, int, std::string_view);
using burke_ratio_signature =
    double (*)(const fincore::dataframe&, double, double, std::string_view);
using sterling_ratio_signature =
    double (*)(const fincore::dataframe&, double, double, std::string_view);
using cal_treynor_ratio_signature =
    double (*)(const fincore::dataframe&, const fincore::dataframe&, double,
               double, std::string_view, std::string_view);
using m_squared_signature =
    double (*)(const fincore::dataframe&, const fincore::dataframe&, double,
               double, std::string_view, std::string_view);

static_assert(std::is_same_v<
              decltype(static_cast<kappa_three_ratio_signature>(
                  &fincore::kappa_three_ratio)),
              kappa_three_ratio_signature>);
static_assert(std::is_same_v<
              decltype(&fincore::adjusted_sharpe_ratio),
              adjusted_sharpe_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::mar_ratio),
                             mar_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::conditional_sharpe_ratio),
                             conditional_sharpe_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::deflated_sharpe_ratio),
                             deflated_sharpe_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::burke_ratio),
                             burke_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::sterling_ratio),
                             sterling_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::cal_treynor_ratio),
                             cal_treynor_ratio_signature>);
static_assert(std::is_same_v<decltype(&fincore::m_squared),
                             m_squared_signature>);

TEST(Ratios, PublishesExactKappaThreeSignatureAndDefaultCall) {
    const auto returns =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});

    EXPECT_DOUBLE_EQ(fincore::kappa_three_ratio(returns),
                     fincore::kappa_three_ratio(returns, 0.0, "returns"));
}

TEST(Ratios, AdjustedSharpeRatioHasExactSignatureAndFiniteDefaultCall) {
    const auto returns =
        fincore::make_series({1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});

    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(returns),
                1.8289412324150214, 1e-14);
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

std::vector<double> repeating_pattern(std::initializer_list<double> pattern,
                                      std::size_t count) {
    std::vector<double> values;
    values.reserve(count);
    while (values.size() < count) {
        for (const double value : pattern) {
            if (values.size() == count) {
                break;
            }
            values.push_back(value);
        }
    }
    return values;
}

std::vector<double> lower_clamp_values(std::size_t count) {
    std::vector<double> values(count - 1, -0.001);
    values.push_back(0.04);
    return values;
}

std::vector<fincore::timestamp> sequence_index(std::size_t count) {
    std::vector<fincore::timestamp> index(count);
    for (std::size_t position = 0; position < count; ++position) {
        index[position] = static_cast<fincore::timestamp>(position + 1);
    }
    return index;
}

fincore::dataframe adjusted_series(std::vector<double> values) {
    return fincore::make_series(sequence_index(values.size()), "returns",
                                values);
}

TEST(Ratios, AdjustedSharpeMatchesPythonOracleForOrdinaryAndRiskFree) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(returns), 2.179402814215353,
                1e-15);
    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(returns, 0.001),
                1.637451311586237, 1e-15);
}

TEST(Ratios, AdjustedSharpeGatesRawCountThenCleanSharpeBeforeMoments) {
    const std::vector<double> raw_four{0.01, -0.02, 0.03, -0.01};
    for (std::size_t count = 0; count < 4; ++count) {
        const auto short_frame = adjusted_series(std::vector<double>(
            raw_four.begin(),
            raw_four.begin() + static_cast<std::ptrdiff_t>(count)));
        EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(short_frame)))
            << "raw count " << count;
    }
    const auto four = adjusted_series(raw_four);
    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(four), 1.8289412324150214,
                1e-15);

    const auto all_nan = adjusted_series(std::vector<double>(10, nan));
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(all_nan)));

    const auto none_clean = adjusted_series({nan, nan, nan, nan});
    const auto one_clean = adjusted_series({nan, nan, nan, 0.01});
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(none_clean)));
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(one_clean)));
}

TEST(Ratios, AdjustedSharpeUsesNanOnlyBiasedMomentsNotPublicStatsGates) {
    const auto two_clean = adjusted_series({nan, nan, 0.01, -0.02});
    const auto three_clean = adjusted_series({nan, 0.01, -0.02, 0.03});

    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(two_clean),
                -4.090878742872842, 1e-14);
    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(three_clean),
                4.520724794501948, 1e-14);
}

TEST(Ratios, AdjustedSharpeDampensAndClampsAtAllRawBoundaries) {
    struct boundary_case {
        std::size_t count;
        double upper_expected;
        double lower_expected;
    };
    const std::array<boundary_case, 4> cases{{
        {19, 33.1438443915704, 1.758752806478678},
        {20, 40.22864650967021, 1.4544898588693689},
        {49, 40.44197112618808, -0.3539959942330339},
        {50, 40.85889866357148, -0.3942429246552071},
    }};

    for (const auto& test_case : cases) {
        const auto upper = adjusted_series(
            repeating_pattern({0.01, 0.03}, test_case.count));
        const auto lower =
            adjusted_series(lower_clamp_values(test_case.count));

        EXPECT_NEAR(fincore::adjusted_sharpe_ratio(upper),
                    test_case.upper_expected, 1e-12)
            << "upper raw count " << test_case.count;
        EXPECT_NEAR(fincore::adjusted_sharpe_ratio(lower),
                    test_case.lower_expected, 1e-13)
            << "lower raw count " << test_case.count;
    }
}

TEST(Ratios, AdjustedSharpeUsesRawCountForDampeningAndClampFamily) {
    std::vector<double> values(18, nan);
    values.push_back(0.01);
    values.push_back(-0.02);
    const auto raw_twenty_clean_two = adjusted_series(values);

    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(raw_twenty_clean_two),
                -4.864154602806124, 1e-14);
}

TEST(Ratios, AdjustedSharpeFallsBackForNaNMomentsOnNearZeroVariance) {
    const double next = std::nextafter(1.0, 2.0);
    const auto near_zero_variance = adjusted_series({1.0, next, 1.0, next});
    const double base = fincore::sharpe_ratio(near_zero_variance);

    EXPECT_NEAR(base, 8.755998418330334e16, 1e2);
    EXPECT_DOUBLE_EQ(fincore::adjusted_sharpe_ratio(near_zero_variance), base);
}

TEST(Ratios, AdjustedSharpePreservesConstantAndNonfiniteClasses) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto zeros = adjusted_series({0.0, 0.0, 0.0, 0.0});
    const auto positive_constant = adjusted_series({0.01, 0.01, 0.01, 0.01});
    const auto negative_constant =
        adjusted_series({-0.01, -0.01, -0.01, -0.01});

    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(zeros)));
    const double positive = fincore::adjusted_sharpe_ratio(positive_constant);
    EXPECT_TRUE(std::isinf(positive));
    EXPECT_GT(positive, 0.0);
    const double negative = fincore::adjusted_sharpe_ratio(negative_constant);
    EXPECT_TRUE(std::isinf(negative));
    EXPECT_LT(negative, 0.0);

    const auto positive_inf = adjusted_series({infinity, 0.01, -0.02, 0.03});
    const auto negative_inf =
        adjusted_series({-infinity, 0.01, -0.02, 0.03});
    const auto both_inf =
        adjusted_series({infinity, -infinity, 0.01, -0.02});
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(positive_inf)));
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(negative_inf)));
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(both_inf)));

    const auto raw_four = adjusted_series({0.01, -0.02, 0.03, -0.01});
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(raw_four, nan)));
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(raw_four, infinity)));
    EXPECT_TRUE(std::isnan(fincore::adjusted_sharpe_ratio(raw_four, -infinity)));
}

TEST(Ratios, AdjustedSharpePreservesOrderAndDuplicateNonmonotonicLabels) {
    const auto ordered = fincore::make_series(
        std::vector<fincore::timestamp>{3, 1, 3, 2}, "returns",
        std::vector<double>{-1e16, 1.0, 1e16, 1.0});
    const auto reordered = fincore::make_series(
        std::vector<fincore::timestamp>{3, 1, 3, 2}, "returns",
        std::vector<double>{1.0, 1e16, 1.0, -1e16});

    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(ordered),
                4.860555523805895e-16, 1e-20);
    EXPECT_DOUBLE_EQ(fincore::adjusted_sharpe_ratio(reordered), 0.0);
    EXPECT_NE(fincore::adjusted_sharpe_ratio(ordered),
              fincore::adjusted_sharpe_ratio(reordered));
}

TEST(Ratios, AdjustedSharpeSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4}, "strategy", {0.01, -0.02, 0.03, -0.01});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};

    EXPECT_NEAR(fincore::adjusted_sharpe_ratio(strategy, 0.0, "strategy"),
                1.8289412324150214, 1e-15);
    EXPECT_DOUBLE_EQ(
        fincore::adjusted_sharpe_ratio(strategy, 0.0, bounded_selector),
        fincore::adjusted_sharpe_ratio(strategy, 0.0, "strategy"));
}

TEST(Ratios, AdjustedSharpeValidatesSelectorsAndStructureBeforeRawCount) {
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

    EXPECT_THROW(
        static_cast<void>(fincore::adjusted_sharpe_ratio(empty_missing_column)),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::adjusted_sharpe_ratio(singleton, 0.0, "")),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(
                     fincore::adjusted_sharpe_ratio(singleton, 0.0, "missing")),
                 fincore::input_error);

    const auto valid_prefix_frame =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    expect_error_message(
        [&] {
            return fincore::adjusted_sharpe_ratio(valid_prefix_frame, 0.0,
                                                  embedded_selector);
        },
        "column name must not contain an embedded NUL");

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::adjusted_sharpe_ratio(wrong_type)),
                 fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{0.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::adjusted_sharpe_ratio(malformed)),
                 fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::adjusted_sharpe_ratio(malformed, 0.0,
                                                  embedded_selector);
        },
        "column name must not contain an embedded NUL");
    expect_error_message(
        [&] { return fincore::adjusted_sharpe_ratio(malformed, nan); },
        "required double column must match the frame index length");
}

TEST(Ratios, AdjustedSharpeReturnsOwnedScalarAfterInputFrameAndSelectorEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4}, "returns", {0.01, -0.02, 0.03, -0.01});
        return fincore::adjusted_sharpe_ratio(returns, 0.0, selector);
    }();

    EXPECT_NEAR(result, 1.8289412324150214, 1e-15);
}

TEST(Ratios, MarRatioMatchesPythonOracleForOrdinaryAndAnnualization) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_NEAR(fincore::mar_ratio(returns), 20.160000000000007, 1e-13);
    EXPECT_NEAR(fincore::mar_ratio(returns, 52.0), 4.160000000000002, 1e-13);
}

TEST(Ratios, MarRatioRespectsRawCountAndNonPositiveDrawdownGates) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto monotone =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    const auto flat =
        fincore::make_series({1, 2, 3}, "returns", {0.0, 0.0, 0.0});

    EXPECT_TRUE(std::isnan(fincore::mar_ratio(empty)));
    EXPECT_TRUE(std::isnan(fincore::mar_ratio(singleton)));
    EXPECT_TRUE(std::isnan(fincore::mar_ratio(monotone)));
    EXPECT_TRUE(std::isnan(fincore::mar_ratio(flat)));
}

TEST(Ratios, MarRatioCleansNanOnlyAndRequiresOneCleanObservation) {
    const auto all_nan =
        fincore::make_series({1, 2, 3, 4}, "returns", {nan, nan, nan, nan});
    const auto one_clean =
        fincore::make_series({1, 2, 3, 4}, "returns", {nan, nan, 0.01, nan});
    const auto nan_mixed = fincore::make_series(
        {1, 2, 3, 4, 5, 6}, "returns", {0.01, nan, -0.05, nan, 0.03, 0.01});

    EXPECT_TRUE(std::isnan(fincore::mar_ratio(all_nan)));
    EXPECT_TRUE(std::isnan(fincore::mar_ratio(one_clean)));
    EXPECT_NEAR(fincore::mar_ratio(nan_mixed), -2.185751579730772e-15, 1e-27);
}

TEST(Ratios, MarRatioMapsInfiniteAndNonfiniteAnnualizationToNan) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_TRUE(std::isnan(fincore::mar_ratio(returns, infinity)));
    EXPECT_TRUE(std::isnan(fincore::mar_ratio(returns, nan)));
}

TEST(Ratios, MarRatioPreservesSignedZeroAndNegativeMeanResults) {
    const auto zero_mean = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7}, "returns",
        {0.01, -0.01, 0.01, -0.01, 0.02, -0.02, 0.0});
    const auto negative_mean = fincore::make_series(
        {1, 2, 3}, "returns", {-0.01, -0.02, -0.03});

    const double zero_result = fincore::mar_ratio(zero_mean);
    EXPECT_EQ(zero_result, 0.0);
    EXPECT_FALSE(std::signbit(zero_result));
    EXPECT_NEAR(fincore::mar_ratio(negative_mean), -85.56004481716624, 1e-12);
}

TEST(Ratios, MarRatioMapsNonfiniteReturnSamplesToNan) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto positive_inf = fincore::make_series(
        {1, 2, 3, 4}, "returns", {infinity, 0.01, -0.02, 0.03});
    const auto negative_inf = fincore::make_series(
        {1, 2, 3, 4}, "returns", {-infinity, 0.01, -0.02, 0.03});

    EXPECT_TRUE(std::isnan(fincore::mar_ratio(positive_inf)));
    EXPECT_TRUE(std::isnan(fincore::mar_ratio(negative_inf)));
}

TEST(Ratios, MarRatioSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};

    EXPECT_NEAR(fincore::mar_ratio(strategy, 252.0, "strategy"),
                20.160000000000007, 1e-13);
    EXPECT_DOUBLE_EQ(fincore::mar_ratio(strategy, 252.0, bounded_selector),
                     fincore::mar_ratio(strategy, 252.0, "strategy"));
}

TEST(Ratios, MarRatioValidatesSelectorsAndStructureBeforeRawCount) {
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

    EXPECT_THROW(static_cast<void>(fincore::mar_ratio(empty_missing_column)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::mar_ratio(singleton, 252.0, "")),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::mar_ratio(singleton, 252.0, "missing")),
        fincore::input_error);

    const auto valid_prefix_frame =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    expect_error_message(
        [&] {
            return fincore::mar_ratio(valid_prefix_frame, 252.0,
                                      embedded_selector);
        },
        "column name must not contain an embedded NUL");

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::mar_ratio(wrong_type)),
                 fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{0.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::mar_ratio(malformed)),
                 fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::mar_ratio(malformed, 252.0, embedded_selector);
        },
        "column name must not contain an embedded NUL");
}

TEST(Ratios, MarRatioReturnsOwnedScalarAfterInputFrameAndSelectorEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::mar_ratio(returns, 252.0, selector);
    }();

    EXPECT_NEAR(result, 20.160000000000007, 1e-13);
}

TEST(Ratios, ConditionalSharpeMatchesPythonOracleForFiniteTails) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_NEAR(fincore::conditional_sharpe_ratio(returns, 0.2),
                -44.89988864128729, 1e-12);
    EXPECT_NEAR(fincore::conditional_sharpe_ratio(returns, 0.5),
                -7.227274904005733, 1e-12);
    EXPECT_NEAR(fincore::conditional_sharpe_ratio(returns, 0.2, 0.001),
                -46.02238585731947, 1e-12);
    EXPECT_NEAR(
        fincore::conditional_sharpe_ratio(returns, 0.2, 0.0, 52.0),
        -20.396078054371134, 1e-12);
}

TEST(Ratios, ConditionalSharpeGatesCountCutoffAndTailSize) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto all_equal = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, 0.01, 0.01, 0.01});
    const auto graded = fincore::make_series(
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, "returns",
        {0.0, 0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09});

    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(empty)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(singleton)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(ordinary, 0.0)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(ordinary, 1.0)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(ordinary, -0.1)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(ordinary, nan)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(ordinary)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(all_equal)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(graded, 0.01)));
}

TEST(Ratios, ConditionalSharpeHandlesDuplicateEqualityBoundaries) {
    const auto duplicates = fincore::make_series(
        {1, 2, 3, 4, 5}, "returns", {0.01, 0.01, 0.02, 0.03, 0.04});
    const auto descending = fincore::make_series(
        {1, 2, 3, 4, 5}, "returns", {0.04, 0.03, 0.02, 0.01, 0.01});

    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(duplicates, 0.4)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(duplicates, 0.2)));
    EXPECT_NEAR(fincore::conditional_sharpe_ratio(duplicates, 0.6),
                36.66060555964672, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::conditional_sharpe_ratio(descending, 0.6),
                     fincore::conditional_sharpe_ratio(duplicates, 0.6));
}

TEST(Ratios, ConditionalSharpePropagatesNaNInfinityAndAnnualizationClasses) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto ordinary_values = std::vector<double>{
        0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03};
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns", ordinary_values);
    const auto with_nan = adjusted_series({0.01, nan, -0.05, 0.03});
    const auto all_nan = adjusted_series({nan, nan, nan});
    const auto positive_inf = adjusted_series({infinity, 0.01, -0.02, 0.03});
    const auto negative_inf = adjusted_series({-infinity, 0.01, -0.02, 0.03});
    const auto inf_not_selected = adjusted_series(
        {infinity, 0.01, -0.02, 0.03, -0.04, 0.02});

    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(with_nan)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(all_nan)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(positive_inf)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(negative_inf)));
    EXPECT_NEAR(fincore::conditional_sharpe_ratio(inf_not_selected, 0.2),
                -33.67491648096547, 1e-12);

    const double infinite_annualization =
        fincore::conditional_sharpe_ratio(ordinary, 0.2, 0.0, infinity);
    EXPECT_TRUE(std::isinf(infinite_annualization));
    EXPECT_LT(infinite_annualization, 0.0);
    EXPECT_TRUE(
        std::isnan(fincore::conditional_sharpe_ratio(ordinary, 0.2, 0.0, nan)));
    EXPECT_TRUE(std::isnan(fincore::conditional_sharpe_ratio(ordinary, 0.2, nan)));
    EXPECT_TRUE(std::isnan(
        fincore::conditional_sharpe_ratio(ordinary, 0.2, infinity)));
}

TEST(Ratios, ConditionalSharpeSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};

    EXPECT_NEAR(fincore::conditional_sharpe_ratio(strategy, 0.2, 0.0, 252.0,
                                                  "strategy"),
                -44.89988864128729, 1e-12);
    EXPECT_DOUBLE_EQ(
        fincore::conditional_sharpe_ratio(strategy, 0.2, 0.0, 252.0,
                                          bounded_selector),
        fincore::conditional_sharpe_ratio(strategy, 0.2, 0.0, 252.0,
                                          "strategy"));
}

TEST(Ratios, ConditionalSharpeValidatesSelectorsAndStructureBeforeCount) {
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

    EXPECT_THROW(static_cast<void>(
                     fincore::conditional_sharpe_ratio(empty_missing_column)),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(
            fincore::conditional_sharpe_ratio(singleton, 0.05, 0.0, 252.0, "")),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::conditional_sharpe_ratio(
                     singleton, 0.05, 0.0, 252.0, "missing")),
                 fincore::input_error);

    const auto valid_prefix_frame =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    expect_error_message(
        [&] {
            return fincore::conditional_sharpe_ratio(
                valid_prefix_frame, 0.05, 0.0, 252.0, embedded_selector);
        },
        "column name must not contain an embedded NUL");

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        static_cast<void>(fincore::conditional_sharpe_ratio(wrong_type)),
        fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{0.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        static_cast<void>(fincore::conditional_sharpe_ratio(malformed)),
        fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::conditional_sharpe_ratio(
                malformed, 0.05, 0.0, 252.0, embedded_selector);
        },
        "column name must not contain an embedded NUL");
}

TEST(Ratios, ConditionalSharpeReturnsOwnedScalarAfterInputLifetimesEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::conditional_sharpe_ratio(returns, 0.2, 0.0, 252.0,
                                                 selector);
    }();

    EXPECT_NEAR(result, -44.89988864128729, 1e-12);
}

TEST(Ratios, DeflatedSharpeMatchesPythonOracleForTrialsAndRiskFree) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_NEAR(fincore::deflated_sharpe_ratio(returns),
                0.6548645197338817, 1e-9);
    EXPECT_NEAR(fincore::deflated_sharpe_ratio(returns, 0.0, 10),
                7.615960789032017e-06, 1e-12);
    EXPECT_NEAR(fincore::deflated_sharpe_ratio(returns, 0.0, 100),
                3.161694901787256e-13, 1e-15);
    EXPECT_NEAR(fincore::deflated_sharpe_ratio(returns, 0.001),
                0.6192133374972018, 1e-9);
}

TEST(Ratios, DeflatedSharpeNormalizesTrialCountToOne) {
    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_DOUBLE_EQ(fincore::deflated_sharpe_ratio(returns, 0.0, 0),
                     fincore::deflated_sharpe_ratio(returns));
    EXPECT_DOUBLE_EQ(fincore::deflated_sharpe_ratio(returns, 0.0, -5),
                     fincore::deflated_sharpe_ratio(returns));
}

TEST(Ratios, DeflatedSharpeGatesCleanCountBelowThree) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto pair = fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    const auto triple =
        fincore::make_series({1, 2, 3}, "returns", {0.01, -0.02, 0.03});
    const auto nan_below = fincore::make_series(
        {1, 2, 3, 4}, "returns", {0.01, nan, -0.02, nan});
    const auto nan_mixed = fincore::make_series(
        {1, 2, 3, 4, 5}, "returns", {0.01, nan, -0.02, nan, 0.03});
    const auto all_nan =
        fincore::make_series({1, 2, 3, 4}, "returns", {nan, nan, nan, nan});

    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(empty)));
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(singleton)));
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(pair)));
    EXPECT_NEAR(fincore::deflated_sharpe_ratio(triple),
                0.6343497465427029, 1e-9);
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(nan_below)));
    EXPECT_NEAR(fincore::deflated_sharpe_ratio(nan_mixed),
                0.6343497465427029, 1e-9);
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(all_nan)));
}

TEST(Ratios, DeflatedSharpeHandlesZeroVarianceAndNonpositiveVariance) {
    const auto all_equal =
        fincore::make_series({1, 2, 3, 4}, "returns", {0.01, 0.01, 0.01, 0.01});
    const auto nonpositive_negative = adjusted_series(
        {-0.2, -0.2, -0.2, -0.1, -0.1});
    const auto nonpositive_positive = adjusted_series(
        {0.2, 0.2, 0.2, 0.1, 0.1});

    EXPECT_DOUBLE_EQ(fincore::deflated_sharpe_ratio(all_equal), 0.5);
    EXPECT_DOUBLE_EQ(fincore::deflated_sharpe_ratio(nonpositive_negative), 0.0);
    EXPECT_DOUBLE_EQ(fincore::deflated_sharpe_ratio(nonpositive_positive), 1.0);
}

TEST(Ratios, DeflatedSharpePropagatesNonfiniteReturnsAndRiskFree) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto positive_inf = adjusted_series({infinity, 0.01, -0.02, 0.03});
    const auto negative_inf = adjusted_series({-infinity, 0.01, -0.02, 0.03});

    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(positive_inf)));
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(negative_inf)));
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(ordinary, nan)));
    EXPECT_TRUE(std::isnan(fincore::deflated_sharpe_ratio(ordinary, infinity)));
}

TEST(Ratios, DeflatedSharpeSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};

    EXPECT_NEAR(fincore::deflated_sharpe_ratio(strategy, 0.0, 10, "strategy"),
                7.615960789032017e-06, 1e-12);
    EXPECT_DOUBLE_EQ(
        fincore::deflated_sharpe_ratio(strategy, 0.0, 10, bounded_selector),
        fincore::deflated_sharpe_ratio(strategy, 0.0, 10, "strategy"));
}

TEST(Ratios, DeflatedSharpeValidatesSelectorsAndStructureBeforeCount) {
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

    EXPECT_THROW(static_cast<void>(
                     fincore::deflated_sharpe_ratio(empty_missing_column)),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(
            fincore::deflated_sharpe_ratio(singleton, 0.0, 1, "")),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(
            fincore::deflated_sharpe_ratio(singleton, 0.0, 1, "missing")),
        fincore::input_error);

    const auto valid_prefix_frame =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    expect_error_message(
        [&] {
            return fincore::deflated_sharpe_ratio(valid_prefix_frame, 0.0, 1,
                                                  embedded_selector);
        },
        "column name must not contain an embedded NUL");

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        static_cast<void>(fincore::deflated_sharpe_ratio(wrong_type)),
        fincore::input_error);

    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("returns", std::vector<double>{0.01},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        static_cast<void>(fincore::deflated_sharpe_ratio(malformed)),
        fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::deflated_sharpe_ratio(malformed, 0.0, 1,
                                                  embedded_selector);
        },
        "column name must not contain an embedded NUL");
}

TEST(Ratios, DeflatedSharpeReturnsOwnedScalarAfterInputLifetimesEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::deflated_sharpe_ratio(returns, 0.0, 1, selector);
    }();

    EXPECT_NEAR(result, 0.6548645197338817, 1e-9);
}

TEST(Ratios, BurkeRatioMatchesPythonOracleForDrawdownAndFallbackPaths) {
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto monotone =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    const auto flat =
        fincore::make_series({1, 2, 3}, "returns", {0.0, 0.0, 0.0});

    EXPECT_NEAR(fincore::burke_ratio(ordinary), 25.638234713640998, 1e-12);
    EXPECT_NEAR(fincore::burke_ratio(ordinary, 0.001),
                25.62108485512675, 1e-12);
    EXPECT_NEAR(fincore::burke_ratio(ordinary, 0.0, 52.0),
                3.5608068202725582, 1e-12);
    EXPECT_NEAR(fincore::burke_ratio(ordinary, 10.0),
                -145.86035042886795, 1e-12);
    EXPECT_NEAR(fincore::burke_ratio(monotone), 17733.449583883645, 1e-8);
    EXPECT_DOUBLE_EQ(fincore::burke_ratio(flat), 0.0);
}

TEST(Ratios, BurkeRatioPreservesConstantTinyDrawdownAndNonfiniteClasses) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto positive_constant = adjusted_series({0.01, 0.01, 0.01, 0.01});
    const auto negative_constant =
        adjusted_series({-0.01, -0.01, -0.01, -0.01});
    const auto tiny_drawdown = adjusted_series({-1e-12, 0.01});
    const auto nan_mixed = adjusted_series({0.01, nan, -0.05, 0.03, 0.01});
    const auto all_nan = adjusted_series({nan, nan, nan});
    const auto positive_inf = adjusted_series({infinity, 0.01, -0.02, 0.03});
    const auto negative_inf = adjusted_series({-infinity, 0.01, -0.02, 0.03});

    EXPECT_NEAR(fincore::burke_ratio(positive_constant),
                112740020992.40234, 1e2);
    EXPECT_NEAR(fincore::burke_ratio(negative_constant),
                -30.994059065669386, 1e-12);
    EXPECT_TRUE(std::isinf(fincore::burke_ratio(tiny_drawdown)));
    EXPECT_GT(fincore::burke_ratio(tiny_drawdown), 0.0);
    EXPECT_NEAR(fincore::burke_ratio(nan_mixed), -1.7656492431737019,
                1e-12);
    EXPECT_TRUE(std::isnan(fincore::burke_ratio(all_nan)));
    EXPECT_TRUE(std::isinf(fincore::burke_ratio(positive_inf)));
    EXPECT_TRUE(std::isnan(fincore::burke_ratio(negative_inf)));
}

TEST(Ratios, BurkeRatioGatesRawCountAndRiskFreeNonfinite) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_TRUE(std::isnan(fincore::burke_ratio(empty)));
    EXPECT_TRUE(std::isnan(fincore::burke_ratio(singleton)));
    EXPECT_TRUE(std::isnan(fincore::burke_ratio(ordinary, nan)));
}

TEST(Ratios, BurkeRatioSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};

    EXPECT_NEAR(fincore::burke_ratio(strategy, 0.0, 252.0, "strategy"),
                25.638234713640998, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::burke_ratio(strategy, 0.0, 252.0,
                                          bounded_selector),
                     fincore::burke_ratio(strategy, 0.0, 252.0, "strategy"));
}

TEST(Ratios, BurkeRatioValidatesSelectorsAndStructureBeforeRawCount) {
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

    EXPECT_THROW(static_cast<void>(fincore::burke_ratio(
                     empty_missing_column)),
                 fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::burke_ratio(singleton, 0.0, 252.0, "")),
        fincore::input_error);
    EXPECT_THROW(static_cast<void>(
                     fincore::burke_ratio(singleton, 0.0, 252.0, "missing")),
                 fincore::input_error);

    const auto valid_prefix_frame =
        fincore::make_series({1, 2}, "returns", {0.01, -0.02});
    expect_error_message(
        [&] {
            return fincore::burke_ratio(valid_prefix_frame, 0.0, 252.0,
                                        embedded_selector);
        },
        "column name must not contain an embedded NUL");

    fincore::dataframe wrong_type;
    wrong_type.load_index(std::vector<fincore::timestamp>{1});
    wrong_type.load_column<int>("returns", std::vector<int>{1},
                                hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(static_cast<void>(fincore::burke_ratio(wrong_type)),
                 fincore::input_error);
}

TEST(Ratios, BurkeRatioReturnsOwnedScalarAfterInputLifetimesEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::burke_ratio(returns, 0.0, 252.0, selector);
    }();

    EXPECT_NEAR(result, 25.638234713640998, 1e-12);
}

TEST(Ratios, SterlingRatioMatchesPythonOracleForDrawdownAndFallbackPaths) {
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto monotone =
        fincore::make_series({1, 2, 3}, "returns", {0.01, 0.02, 0.03});
    const auto flat =
        fincore::make_series({1, 2, 3}, "returns", {0.0, 0.0, 0.0});

    EXPECT_NEAR(fincore::sterling_ratio(ordinary), 37.373828320998385, 1e-12);
    EXPECT_NEAR(fincore::sterling_ratio(ordinary, 0.001),
                37.348828320998386, 1e-12);
    EXPECT_NEAR(fincore::sterling_ratio(ordinary, 0.0, 52.0),
                5.190723318961585, 1e-12);
    EXPECT_NEAR(fincore::sterling_ratio(monotone), 17733.449583883645, 1e-8);
    EXPECT_DOUBLE_EQ(fincore::sterling_ratio(flat), 0.0);
}

TEST(Ratios, SterlingRatioPreservesConstantTinyDrawdownAndNonfiniteClasses) {
    const double infinity = std::numeric_limits<double>::infinity();
    const auto positive_constant = adjusted_series({0.01, 0.01, 0.01, 0.01});
    const auto negative_constant =
        adjusted_series({-0.01, -0.01, -0.01, -0.01});
    const auto tiny_drawdown = adjusted_series({-1e-12, 0.01});
    const auto nan_mixed = adjusted_series({0.01, nan, -0.05, 0.03, 0.01});
    const auto all_nan = adjusted_series({nan, nan, nan});
    const auto positive_inf = adjusted_series({infinity, 0.01, -0.02, 0.03});
    const auto negative_inf = adjusted_series({-infinity, 0.01, -0.02, 0.03});

    EXPECT_NEAR(fincore::sterling_ratio(positive_constant),
                112740020992.40234, 1e2);
    EXPECT_NEAR(fincore::sterling_ratio(negative_constant),
                -30.994059065669386, 1e-12);
    EXPECT_TRUE(std::isinf(fincore::sterling_ratio(tiny_drawdown)));
    EXPECT_GT(fincore::sterling_ratio(tiny_drawdown), 0.0);
    EXPECT_NEAR(fincore::sterling_ratio(nan_mixed), -1.7656492431737019,
                1e-12);
    EXPECT_TRUE(std::isnan(fincore::sterling_ratio(all_nan)));
    EXPECT_TRUE(std::isinf(fincore::sterling_ratio(positive_inf)));
    const double negative_infinity_result =
        fincore::sterling_ratio(negative_inf);
    EXPECT_EQ(negative_infinity_result, 0.0);
    EXPECT_TRUE(std::signbit(negative_infinity_result));
}

TEST(Ratios, SterlingRatioGatesRawCountAndRiskFreeNonfinite) {
    const auto empty = fincore::make_series({}, "returns", {});
    const auto singleton = fincore::make_series({1}, "returns", {0.01});
    const auto ordinary = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});

    EXPECT_TRUE(std::isnan(fincore::sterling_ratio(empty)));
    EXPECT_TRUE(std::isnan(fincore::sterling_ratio(singleton)));
    EXPECT_TRUE(std::isnan(fincore::sterling_ratio(ordinary, nan)));
}

TEST(Ratios, SterlingRatioSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    constexpr std::array<char, 8> bounded_name{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view bounded_selector{bounded_name.data(),
                                            bounded_name.size()};

    EXPECT_NEAR(fincore::sterling_ratio(strategy, 0.0, 252.0, "strategy"),
                37.373828320998385, 1e-12);
    EXPECT_DOUBLE_EQ(fincore::sterling_ratio(strategy, 0.0, 252.0,
                                             bounded_selector),
                     fincore::sterling_ratio(strategy, 0.0, 252.0, "strategy"));
}

TEST(Ratios, SterlingRatioReturnsOwnedScalarAfterInputLifetimesEnd) {
    const double result = [] {
        const std::string column{"returns"};
        const std::string_view selector{column.data(), column.size()};
        const auto returns = fincore::make_series(
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, "returns",
            {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
        return fincore::sterling_ratio(returns, 0.0, 252.0, selector);
    }();

    EXPECT_NEAR(result, 37.373828320998385, 1e-12);
}

fincore::dataframe treynor_returns() {
    return fincore::make_series(
        sequence_index(10), "returns",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
}

fincore::dataframe plain_benchmark() {
    return fincore::make_series(
        sequence_index(10), "benchmark",
        {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005, 0.01, 0.02, 0.03, 0.04});
}

fincore::dataframe positive_beta_benchmark() {
    return fincore::make_series(
        sequence_index(10), "benchmark",
        {0.014, 0.023, -0.059, 0.034, 0.0135, -0.0255, 0.049, 0.014, 0.023,
         -0.036});
}

TEST(Ratios, CalTreynorMatchesPythonOracleForPositiveBeta) {
    const auto returns = treynor_returns();
    const auto benchmark = positive_beta_benchmark();

    EXPECT_NEAR(fincore::cal_treynor_ratio(returns, benchmark),
                1.7905755807724069, 1e-12);
    EXPECT_NEAR(fincore::cal_treynor_ratio(returns, benchmark, 0.001),
                1.7893778338053357, 1e-12);
    EXPECT_NEAR(fincore::cal_treynor_ratio(returns, benchmark, 0.0, 52.0),
                0.2486869244876499, 1e-12);
}

TEST(Ratios, CalTreynorRejectsNonPositiveAndUndefinedBeta) {
    const auto returns = treynor_returns();
    const auto constant_benchmark = fincore::make_series(
        sequence_index(10), "benchmark",
        {0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01});
    const auto inverse_benchmark = fincore::make_series(
        sequence_index(10), "inverse",
        {0.04, 0.03, 0.02, 0.01, 0.005, -0.005, -0.01, -0.02, -0.03, -0.04});
    const auto plain_benchmark = fincore::make_series(
        sequence_index(10), "benchmark",
        {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005, 0.01, 0.02, 0.03, 0.04});
    const auto short_returns =
        fincore::make_series({1}, "returns", {0.01});
    const auto short_benchmark =
        fincore::make_series({1}, "benchmark", {0.01});

    EXPECT_TRUE(std::isnan(
        fincore::cal_treynor_ratio(returns, constant_benchmark)));
    EXPECT_TRUE(std::isnan(fincore::cal_treynor_ratio(
        inverse_benchmark, plain_benchmark, 0.0, 252.0, "inverse",
        "benchmark")));
    EXPECT_TRUE(std::isnan(
        fincore::cal_treynor_ratio(short_returns, short_benchmark)));
}

TEST(Ratios, CalTreynorAlignsAndToleratesNanPairs) {
    auto returns = treynor_returns();
    auto benchmark = positive_beta_benchmark();
    std::vector<double> return_values{
        0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03};
    std::vector<double> benchmark_values{0.014, 0.023, -0.059, 0.034, 0.0135,
                                         -0.0255, 0.049, 0.014, 0.023, -0.036};
    return_values[2] = nan;
    benchmark_values[0] = nan;
    const auto nan_returns =
        fincore::make_series(sequence_index(10), "returns", return_values);
    const auto nan_benchmark =
        fincore::make_series(sequence_index(10), "benchmark", benchmark_values);

    EXPECT_NEAR(fincore::cal_treynor_ratio(nan_returns, nan_benchmark),
                9.760631792660886, 1e-12);
}

TEST(Ratios, CalTreynorSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        sequence_index(10), "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto factor = fincore::make_series(
        sequence_index(10), "factor",
        {0.014, 0.023, -0.059, 0.034, 0.0135, -0.0255, 0.049, 0.014, 0.023,
         -0.036});
    constexpr std::array<char, 8> bounded_returns{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    constexpr std::array<char, 6> bounded_factor{'f', 'a', 'c', 't', 'o', 'r'};
    const std::string_view returns_selector{bounded_returns.data(),
                                            bounded_returns.size()};
    const std::string_view factor_selector{bounded_factor.data(),
                                           bounded_factor.size()};

    EXPECT_NEAR(fincore::cal_treynor_ratio(strategy, factor, 0.0, 252.0,
                                           "strategy", "factor"),
                1.7905755807724069, 1e-12);
    EXPECT_DOUBLE_EQ(
        fincore::cal_treynor_ratio(strategy, factor, 0.0, 252.0,
                                   returns_selector, factor_selector),
        fincore::cal_treynor_ratio(strategy, factor, 0.0, 252.0, "strategy",
                                   "factor"));
}

TEST(Ratios, CalTreynorValidatesSelectorsAndStructure) {
    const auto returns = treynor_returns();
    const auto benchmark = positive_beta_benchmark();
    const std::array<char, 13> embedded_storage{
        'b', 'e', 'n', 'c', 'h', 'm', 'a', 'r', 'k', ' ', 'x', '!', '!'};
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

    EXPECT_THROW(static_cast<void>(
                     fincore::cal_treynor_ratio(returns, benchmark, 0.0, 252.0,
                                                "missing", "benchmark")),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(
                     fincore::cal_treynor_ratio(returns, benchmark, 0.0, 252.0,
                                                "returns", "missing")),
                 fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::cal_treynor_ratio(returns, benchmark, 0.0, 252.0,
                                              "returns", embedded_selector);
        },
        "column name must not contain an embedded NUL");
}

TEST(Ratios, CalTreynorReturnsOwnedScalarAfterInputLifetimesEnd) {
    const double result = [] {
        const auto returns = treynor_returns();
        const auto benchmark = positive_beta_benchmark();
        return fincore::cal_treynor_ratio(returns, benchmark);
    }();

    EXPECT_NEAR(result, 1.7905755807724069, 1e-12);
}

TEST(Ratios, MSquaredMatchesPythonOracleForAnnualizationAndRiskFree) {
    const auto returns = treynor_returns();
    const auto benchmark = plain_benchmark();

    EXPECT_NEAR(fincore::m_squared(returns, benchmark),
                1.3665828992102713, 1e-12);
    EXPECT_NEAR(fincore::m_squared(returns, benchmark, 0.001),
                1.3666687682792138, 1e-12);
    EXPECT_NEAR(fincore::m_squared(returns, benchmark, 0.0, 52.0),
                0.1898000296169657, 1e-12);
}

TEST(Ratios, MSquaredGatesZeroPortfolioVolatilityAndShortInput) {
    const auto benchmark = plain_benchmark();
    const auto zero_returns = fincore::make_series(
        sequence_index(10), "returns",
        {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    const auto short_returns = fincore::make_series({1}, "returns", {0.01});
    const auto short_benchmark =
        fincore::make_series({1}, "benchmark", {0.01});

    EXPECT_TRUE(std::isnan(fincore::m_squared(zero_returns, benchmark)));
    EXPECT_TRUE(
        std::isnan(fincore::m_squared(short_returns, short_benchmark)));
}

TEST(Ratios, MSquaredAlignsAndToleratesNanPairs) {
    std::vector<double> return_values{
        0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03};
    std::vector<double> benchmark_values{-0.04, -0.03, -0.02, -0.01, -0.005,
                                         0.005, 0.01, 0.02, 0.03, 0.04};
    return_values[2] = nan;
    benchmark_values[0] = nan;
    const auto nan_returns =
        fincore::make_series(sequence_index(10), "returns", return_values);
    const auto nan_benchmark =
        fincore::make_series(sequence_index(10), "benchmark", benchmark_values);

    EXPECT_NEAR(fincore::m_squared(nan_returns, nan_benchmark),
                8.357805155252677, 1e-12);
}

TEST(Ratios, MSquaredSupportsNamedAndBoundedSelectors) {
    const auto strategy = fincore::make_series(
        sequence_index(10), "strategy",
        {0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03});
    const auto factor = fincore::make_series(
        sequence_index(10), "factor",
        {-0.04, -0.03, -0.02, -0.01, -0.005, 0.005, 0.01, 0.02, 0.03, 0.04});
    constexpr std::array<char, 8> bounded_returns{
        's', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    constexpr std::array<char, 6> bounded_factor{'f', 'a', 'c', 't', 'o', 'r'};
    const std::string_view returns_selector{bounded_returns.data(),
                                            bounded_returns.size()};
    const std::string_view factor_selector{bounded_factor.data(),
                                           bounded_factor.size()};

    EXPECT_NEAR(fincore::m_squared(strategy, factor, 0.0, 252.0, "strategy",
                                   "factor"),
                1.3665828992102713, 1e-12);
    EXPECT_DOUBLE_EQ(
        fincore::m_squared(strategy, factor, 0.0, 252.0, returns_selector,
                           factor_selector),
        fincore::m_squared(strategy, factor, 0.0, 252.0, "strategy",
                           "factor"));
}

TEST(Ratios, MSquaredValidatesSelectorsAndStructure) {
    const auto returns = treynor_returns();
    const auto benchmark = plain_benchmark();
    const std::array<char, 13> embedded_storage{
        'b', 'e', 'n', 'c', 'h', 'm', 'a', 'r', 'k', '\0', 'x', '!', '!'};
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

    EXPECT_THROW(
        static_cast<void>(fincore::m_squared(returns, benchmark, 0.0, 252.0,
                                             "missing", "benchmark")),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::m_squared(returns, benchmark, 0.0, 252.0,
                                             "returns", "missing")),
        fincore::input_error);
    expect_error_message(
        [&] {
            return fincore::m_squared(returns, benchmark, 0.0, 252.0,
                                      "returns", embedded_selector);
        },
        "column name must not contain an embedded NUL");
}

TEST(Ratios, MSquaredReturnsOwnedScalarAfterInputLifetimesEnd) {
    const double result = [] {
        const auto returns = treynor_returns();
        const auto benchmark = plain_benchmark();
        return fincore::m_squared(returns, benchmark);
    }();

    EXPECT_NEAR(result, 1.3665828992102713, 1e-12);
}

}  // namespace
