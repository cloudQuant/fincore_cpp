#include <fincore/attribution/fama_french.hpp>
#include <fincore/calendar.hpp>
#include <fincore/error.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

fincore::dataframe return_frame() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day,
        start + 4 * day, start + 7 * day});
    frame.load_column<double>(
        "A", {0.010, 0.020, -0.005, 0.015, 0.004, -0.002},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "B", {0.005, 0.011, 0.002, 0.009, -0.003, 0.004},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "C", {-0.004, 0.006, 0.012, -0.002, 0.008, 0.001},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe factor_frame() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day,
        start + 4 * day, start + 7 * day});
    frame.load_column<double>(
        "MKT", {0.008, 0.012, -0.006, 0.010, 0.002, 0.000},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "SMB", {0.002, -0.001, 0.003, 0.001, -0.002, 0.001},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "HML", {-0.001, 0.002, 0.001, -0.003, 0.002, 0.000},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "RMW", {0.001, 0.000, -0.002, 0.002, 0.001, -0.001},
        hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(
        "CMA", {0.000, 0.001, 0.002, -0.001, 0.000, 0.001},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(AttributionFamaFrench, CalculatesIdiosyncraticRiskWithOracleValues) {
    const auto returns = return_frame();
    const auto factors = factor_frame();
    const auto result =
        fincore::calculate_idiosyncratic_risk(returns, factors);
    ASSERT_EQ(result.assets.size(), 3U);
    EXPECT_EQ(result.assets[0], "A");
    EXPECT_EQ(result.assets[1], "B");
    EXPECT_EQ(result.assets[2], "C");
    // One shared beta, fitted on the first asset column A alone, is applied to B
    // and C too, so their volatilities differ only through their own variance.
    EXPECT_NEAR(result.market_beta, 0.30357142857141906, 1e-10);
    EXPECT_NEAR(result.specific_volatility[0], 0.008701654177846832, 1e-12);
    EXPECT_NEAR(result.specific_volatility[1], 0.0041562412088803, 1e-12);
    EXPECT_NEAR(result.specific_volatility[2], 0.005319973567995668, 1e-12);

    const auto explicit_beta = fincore::calculate_idiosyncratic_risk(
        returns, factors, 0.5);
    EXPECT_DOUBLE_EQ(explicit_beta.market_beta, 0.5);
    EXPECT_NE(explicit_beta.specific_volatility[0],
              result.specific_volatility[0]);
    EXPECT_NEAR(explicit_beta.specific_volatility[0], 0.0083383318342326596,
                1e-12);

    auto without_cma = fincore::dataframe{};
    for (const auto& info : factors.get_columns_info<double>()) {
        const std::string name{std::get<0>(info).c_str()};
        if (name == "CMA") {
            continue;
        }
        const auto& values = factors.get_column<double>(name.c_str());
        if (without_cma.get_index().empty()) {
            without_cma.load_index(
                std::vector<fincore::timestamp>{factors.get_index()});
        }
        without_cma.load_column<double>(
            name.c_str(), std::vector<double>(values.begin(), values.end()),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    // Only the beta fit demands all five columns; with an explicit beta just MKT
    // is read, so this same CMA-less frame flips from rejection to acceptance.
    EXPECT_THROW(
        static_cast<void>(
            fincore::calculate_idiosyncratic_risk(returns, without_cma)),
        fincore::input_error);
    EXPECT_NO_THROW(static_cast<void>(
        fincore::calculate_idiosyncratic_risk(returns, without_cma, 0.3)));
}

}  // namespace
