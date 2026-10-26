#include <fincore/attribution/factor_attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
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
                                const std::string_view name = "returns") {
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
    // Returns are synthesised from a known alpha, known betas and a small
    // residual, so every pinned contribution is a recovered coefficient rather
    // than an opaque fitted constant.
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

double arithmetic_mean(const std::span<const double> values) {
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

std::vector<std::string> expected_components(
    const std::span<const std::string_view> names) {
    std::vector<std::string> labels;
    labels.reserve(names.size() + 4U);
    labels.emplace_back(fincore::factor_regression_alpha_name);
    for (const std::string_view name : names) {
        labels.emplace_back(std::string{name} + "_attribution");
    }
    labels.emplace_back("specific_return");
    labels.emplace_back("common_return");
    labels.emplace_back("unexplained");
    return labels;
}

void expect_summary_schema(const fincore::dataframe& summary,
                           const std::span<const std::string_view> names) {
    const auto expected_labels = expected_components(names);
    ASSERT_EQ(summary.shape().second, 2U);

    const auto string_columns = summary.get_columns_info<std::string>();
    ASSERT_EQ(string_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(string_columns[0]).c_str(),
                 std::string{fincore::factor_attribution_component_column}.c_str());
    EXPECT_EQ(std::get<2>(string_columns[0]), typeid(std::string));

    const auto double_columns = summary.get_columns_info<double>();
    ASSERT_EQ(double_columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(double_columns[0]).c_str(),
                 std::string{fincore::factor_attribution_contribution_column}.c_str());
    EXPECT_EQ(std::get<2>(double_columns[0]), typeid(double));

    std::vector<timestamp> expected_index;
    expected_index.reserve(expected_labels.size());
    for (std::size_t row = 0; row < expected_labels.size(); ++row) {
        expected_index.push_back(static_cast<timestamp>(row));
    }
    EXPECT_EQ(summary.get_index(), expected_index);

    const auto& labels = summary.get_column<std::string>(
        fincore::factor_attribution_component_column.data());
    ASSERT_EQ(labels.size(), expected_labels.size());
    for (std::size_t row = 0; row < labels.size(); ++row) {
        EXPECT_EQ(labels[row], expected_labels[row]);
    }
    EXPECT_EQ(summary.get_column<double>(
                  fincore::factor_attribution_contribution_column.data()).size(),
              expected_labels.size());
}

void expect_identities(const fincore::dataframe& summary,
                       const std::span<const std::string_view> names,
                       const double risk_free_rate,
                       const std::span<const double> returns) {
    const auto& values = summary.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(values.size(), names.size() + 4U);

    double factor_sum = 0.0;
    for (std::size_t factor = 0; factor < names.size(); ++factor) {
        factor_sum += values[factor + 1U];
    }
    EXPECT_NEAR(values[names.size() + 1U], factor_sum, 1.0e-12);
    const double common_return = arithmetic_mean(returns) - risk_free_rate;
    EXPECT_NEAR(values[names.size() + 2U], common_return, 1.0e-12);
    EXPECT_NEAR(values[names.size() + 3U],
                values[names.size() + 2U] - values[0] - values[names.size() + 1U],
                1.0e-12);
}

void expect_direct_fit_parity(const fincore::dataframe& summary,
                              const factor_fixture& fixture,
                              const std::span<const std::string_view> names) {
    const auto direct = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, names);
    const auto& direct_coefficients = direct.coefficients.get_column<double>(
        fincore::factor_regression_coefficient_column.data());
    const auto& contributions = summary.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(direct_coefficients.size(), names.size() + 1U);
    ASSERT_EQ(contributions.size(), names.size() + 4U);
    EXPECT_NEAR(contributions[0], direct.alpha, 1.0e-12);
    for (std::size_t factor = 0; factor < names.size(); ++factor) {
        const auto& values = fixture.factors.get_column<double>(names[factor].data());
        EXPECT_NEAR(contributions[factor + 1U],
                    direct_coefficients[factor + 1U] * arithmetic_mean(values),
                    1.0e-12);
    }
}

void expect_same_summary_values(const fincore::dataframe& actual,
                                const fincore::dataframe& expected) {
    EXPECT_EQ(actual.get_index(), expected.get_index());
    EXPECT_EQ(actual.get_column<std::string>(
                  fincore::factor_attribution_component_column.data()),
              expected.get_column<std::string>(
                  fincore::factor_attribution_component_column.data()));
    const auto& actual_values = actual.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    const auto& expected_values = expected.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(actual_values.size(), expected_values.size());
    for (std::size_t row = 0; row < actual_values.size(); ++row) {
        EXPECT_NEAR(actual_values[row], expected_values[row], 1.0e-12);
    }
}

TEST(FactorAttribution, MatchesSourceOracleWithOrderedTypedSummary) {
    const auto fixture = oracle_fixture();
    const auto summary = fincore::factor_attribution_decomposition(
        fixture.returns, fixture.factors, factor_names, .0002);

    expect_summary_schema(summary, factor_names);
    EXPECT_NE(summary.get_index(), fixture.returns.get_index());
    const auto& values = summary.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(values.size(), 7U);
    EXPECT_NEAR(values[0], .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(values[1], .010526060999251869, 1.0e-12);
    EXPECT_NEAR(values[2], -.0031142789575150653, 1.0e-12);
    EXPECT_NEAR(values[3], .0010187735138187518, 1.0e-12);
    EXPECT_NEAR(values[4], .008430555555555556, 1.0e-12);
    EXPECT_NEAR(values[5], .0088, 1.0e-12);
    EXPECT_NEAR(values[6], -.00020000000000002134, 1.0e-12);
    expect_direct_fit_parity(summary, fixture, factor_names);
    expect_identities(summary, factor_names, .0002,
                      fixture.returns.get_column<double>("returns"));
}

TEST(FactorAttribution, PreservesRequestedFactorOrderAndDirectFitParity) {
    const auto fixture = oracle_fixture();
    const auto original = fincore::factor_attribution_decomposition(
        fixture.returns, fixture.factors, factor_names);
    const auto reordered = fincore::factor_attribution_decomposition(
        fixture.returns, fixture.factors, reordered_factor_names);

    expect_summary_schema(original, factor_names);
    expect_summary_schema(reordered, reordered_factor_names);
    expect_direct_fit_parity(original, fixture, factor_names);
    expect_direct_fit_parity(reordered, fixture, reordered_factor_names);

    const auto& original_values = original.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    const auto& reordered_values = reordered.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    EXPECT_NEAR(original_values[0], reordered_values[0], 1.0e-12);
    EXPECT_NEAR(original_values[1], reordered_values[2], 1.0e-12);
    EXPECT_NEAR(original_values[2], reordered_values[3], 1.0e-12);
    EXPECT_NEAR(original_values[3], reordered_values[1], 1.0e-12);
    for (std::size_t row = 4U; row < original_values.size(); ++row) {
        EXPECT_NEAR(original_values[row], reordered_values[row], 1.0e-12);
    }
}

TEST(FactorAttribution, AppliesRiskFreeRateOnlyToCommonAndUnexplained) {
    const auto fixture = oracle_fixture();
    const auto zero = fincore::factor_attribution_decomposition(
        fixture.returns, fixture.factors, factor_names);
    const auto with_rate = fincore::factor_attribution_decomposition(
        fixture.returns, fixture.factors, factor_names, .0002);
    const auto& zero_values = zero.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    const auto& rate_values = with_rate.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(zero_values.size(), rate_values.size());
    for (std::size_t row = 0; row < factor_names.size() + 2U; ++row) {
        EXPECT_NEAR(rate_values[row], zero_values[row], 1.0e-12);
    }
    EXPECT_NEAR(rate_values[5], zero_values[5] - .0002, 1.0e-12);
    EXPECT_NEAR(rate_values[6], zero_values[6] - .0002, 1.0e-12);
    expect_identities(zero, factor_names, .0,
                      fixture.returns.get_column<double>("returns"));
    expect_identities(with_rate, factor_names, .0002,
                      fixture.returns.get_column<double>("returns"));
}

TEST(FactorAttribution, SupportsBoundedNonTerminatedReturnsSelector) {
    const auto fixture = oracle_fixture();
    const auto& source_values = fixture.returns.get_column<double>("returns");
    const auto named_returns = make_returns(
        std::vector<timestamp>{oracle_index.begin(), oracle_index.end()},
        std::vector<double>{source_values.begin(), source_values.end()}, "strategy");
    std::array<char, 8U> selector_storage{'s', 't', 'r', 'a', 't', 'e', 'g', 'y'};
    const std::string_view selector{selector_storage.data(), selector_storage.size()};

    const auto summary = fincore::factor_attribution_decomposition(
        named_returns, fixture.factors, factor_names, .0002, selector);

    expect_summary_schema(summary, factor_names);
    EXPECT_NEAR(summary.get_column<double>(
                    fincore::factor_attribution_contribution_column.data())[0],
                .0005694444444444665, 1.0e-12);
}

TEST(FactorAttribution, RejectsInvalidSelectorsColumnsIndexesAndFiniteInputs) {
    const auto fixture = oracle_fixture();
    constexpr std::array<std::string_view, 0U> no_factors{};
    constexpr std::array<std::string_view, 1U> empty_factor{""};
    constexpr std::array<std::string_view, 2U> duplicate_factor{"MKT", "MKT"};
    constexpr std::array<std::string_view, 1U> alpha_factor{"alpha"};
    constexpr std::array<std::string_view, 1U> missing_factor{"missing"};
    constexpr std::array<char, 8U> embedded_factor_storage{
        'M', 'K', 'T', '\0', 'e', 'v', 'i', 'l'};
    const std::array<std::string_view, 3U> embedded_factor_names{
        std::string_view{embedded_factor_storage.data(), embedded_factor_storage.size()},
        "SMB", "HML"};
    constexpr std::array<char, 12U> embedded_returns_storage{
        'r', 'e', 't', 'u', 'r', 'n', 's', '\0', 'e', 'v', 'i', 'l'};
    const std::string_view embedded_returns_name{embedded_returns_storage.data(),
                                                 embedded_returns_storage.size()};

    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, no_factors),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, empty_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, duplicate_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, alpha_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, missing_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, embedded_factor_names),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, factor_names, .0,
                     embedded_returns_name),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, factor_names, .0, ""),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, factor_names, .0,
                     "missing_returns"),
                 fincore::input_error);

    auto wrong_return_type = fixture.returns;
    wrong_return_type.remove_column<double>("returns");
    wrong_return_type.load_column<int>("returns", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     wrong_return_type, fixture.factors, factor_names),
                 fincore::input_error);

    auto wrong_factor_type = fixture.factors;
    wrong_factor_type.remove_column<double>("MKT");
    wrong_factor_type.load_column<int>("MKT", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, wrong_factor_type, factor_names),
                 fincore::input_error);

    auto short_returns = fixture.returns;
    short_returns.remove_column<double>("returns");
    short_returns.load_column<double>("returns", {.1},
                                      hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     short_returns, fixture.factors, factor_names),
                 fincore::input_error);

    auto short_factor = fixture.factors;
    short_factor.remove_column<double>("SMB");
    short_factor.load_column<double>("SMB", {.1},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, short_factor, factor_names),
                 fincore::input_error);

    auto duplicate_returns_index = fixture.returns;
    duplicate_returns_index.load_index(
        {oracle_index[0], oracle_index[0], oracle_index[2], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     duplicate_returns_index, fixture.factors, factor_names),
                 fincore::input_error);

    auto duplicate_factor_index = fixture.factors;
    duplicate_factor_index.load_index(
        {oracle_index[0], oracle_index[1], oracle_index[1], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, duplicate_factor_index, factor_names),
                 fincore::input_error);

    auto mismatched_index = fixture.factors;
    mismatched_index.load_index(
        {oracle_index[1], oracle_index[0], oracle_index[2], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, mismatched_index, factor_names),
                 fincore::input_error);

    const auto too_short_returns = make_returns({1, 2, 3}, {.01, .02, .03});
    const auto too_short_factors = make_factors(
        {1, 2, 3}, {.01, .02, .03}, {.02, .03, .04}, {.03, .04, .05});
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     too_short_returns, too_short_factors, factor_names),
                 fincore::input_error);

    const auto empty_returns = make_returns({}, {});
    const auto empty_factors = make_factors({}, {}, {}, {});
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     empty_returns, empty_factors, factor_names),
                 fincore::input_error);

    auto nan_returns = fixture.returns;
    nan_returns.get_column<double>("returns")[0] =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     nan_returns, fixture.factors, factor_names),
                 fincore::input_error);

    auto positive_infinite_factor = fixture.factors;
    positive_infinite_factor.get_column<double>("MKT")[0] =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, positive_infinite_factor, factor_names),
                 fincore::input_error);

    auto negative_infinite_factor = fixture.factors;
    negative_infinite_factor.get_column<double>("SMB")[0] =
        -std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, negative_infinite_factor, factor_names),
                 fincore::input_error);

    auto rank_deficient_factors = fixture.factors;
    rank_deficient_factors.get_column<double>("SMB") =
        rank_deficient_factors.get_column<double>("MKT");
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, rank_deficient_factors, factor_names),
                 fincore::input_error);
}

TEST(FactorAttribution, RejectsNonfiniteRiskFreeRate) {
    const auto fixture = oracle_fixture();
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, factor_names,
                     std::numeric_limits<double>::quiet_NaN()),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, factor_names,
                     std::numeric_limits<double>::infinity()),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_attribution_decomposition(
                     fixture.returns, fixture.factors, factor_names,
                     -std::numeric_limits<double>::infinity()),
                 fincore::input_error);
}

TEST(FactorAttribution, IgnoresUnrelatedMetadataAndOwnsResult) {
    const auto result = [] {
        auto fixture = oracle_fixture();
        auto clean_factors = fixture.factors;
        clean_factors.remove_column<int>("unrelated_malformed");
        std::array<std::string, 3U> source_factor_names{"MKT", "SMB", "HML"};
        const std::array<std::string_view, 3U> source_factor_name_views{
            source_factor_names[0], source_factor_names[1], source_factor_names[2]};
        auto output = fincore::factor_attribution_decomposition(
            fixture.returns, fixture.factors, source_factor_name_views, .0002);
        const auto clean_output = fincore::factor_attribution_decomposition(
            fixture.returns, clean_factors, source_factor_name_views, .0002);
        expect_same_summary_values(output, clean_output);
        // The std::strings behind the selector views are rewritten after the
        // call, so the summary's unchanged component labels prove it stored
        // copies rather than views into the caller's storage.
        fixture.returns.get_column<double>("returns")[0] = 123.0;
        fixture.factors.get_column<double>("MKT")[0] = -123.0;
        fixture.returns.load_index({11, 12, 13, 14, 15, 16, 17, 18});
        fixture.factors.load_index({21, 22, 23, 24, 25, 26, 27, 28});
        source_factor_names[0] = "mutated-market";
        return output;
    }();

    expect_summary_schema(result, factor_names);
    const auto& values = result.get_column<double>(
        fincore::factor_attribution_contribution_column.data());
    ASSERT_EQ(values.size(), 7U);
    EXPECT_NEAR(values[0], .0005694444444444665, 1.0e-12);
    EXPECT_NEAR(values[1], .010526060999251869, 1.0e-12);
    EXPECT_NEAR(values[6], -.00020000000000002134, 1.0e-12);
}

}  // namespace
