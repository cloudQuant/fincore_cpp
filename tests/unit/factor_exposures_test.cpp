#include <fincore/attribution/factor_exposures.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using fincore::timestamp;

constexpr std::array<std::string_view, 3U> factor_names{"MKT", "SMB", "HML"};
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

void expect_exposure_schema(const fincore::dataframe& exposures,
                            const std::span<const std::string_view> expected_names,
                            const std::vector<timestamp>& expected_index) {
    const auto columns = exposures.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected_names.size() + 1U);
    EXPECT_STREQ(std::get<0>(columns[0]).c_str(),
                 std::string{fincore::factor_regression_alpha_name}.c_str());
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
    for (std::size_t factor = 0; factor < expected_names.size(); ++factor) {
        EXPECT_STREQ(std::get<0>(columns[factor + 1U]).c_str(),
                     std::string{expected_names[factor]}.c_str());
        EXPECT_EQ(std::get<2>(columns[factor + 1U]), typeid(double));
    }
    EXPECT_EQ(exposures.get_index(), expected_index);
}

void expect_row(const fincore::dataframe& exposures,
                const std::size_t row,
                const std::array<double, 4U>& expected) {
    constexpr std::array<std::string_view, 4U> names{
        fincore::factor_regression_alpha_name, "MKT", "SMB", "HML"};
    for (std::size_t column = 0; column < names.size(); ++column) {
        EXPECT_NEAR(exposures.get_column<double>(names[column].data())[row],
                    expected[column], 1.0e-12);
    }
}

void expect_same_values(const fincore::dataframe& actual,
                        const fincore::dataframe& expected,
                        const std::span<const std::string_view> names) {
    EXPECT_EQ(actual.get_index(), expected.get_index());
    const auto expect_named_values = [&actual, &expected](const std::string_view name) {
        const auto& actual_values = actual.get_column<double>(name.data());
        const auto& expected_values = expected.get_column<double>(name.data());
        ASSERT_EQ(actual_values.size(), expected_values.size());
        for (std::size_t row = 0; row < actual_values.size(); ++row) {
            if (std::isnan(expected_values[row])) {
                EXPECT_TRUE(std::isnan(actual_values[row]));
            } else {
                EXPECT_NEAR(actual_values[row], expected_values[row], 1.0e-12);
            }
        }
    };
    expect_named_values(fincore::factor_regression_alpha_name);
    for (const std::string_view name : names) {
        expect_named_values(name);
    }
}

fincore::dataframe materialize_returns_slice(const fincore::dataframe& source,
                                             const std::size_t start,
                                             const std::size_t count) {
    const auto& index = source.get_index();
    const auto& values = source.get_column<double>("returns");
    return make_returns(
        std::vector<timestamp>{index.begin() + start, index.begin() + start + count},
        std::vector<double>{values.begin() + start, values.begin() + start + count});
}

fincore::dataframe materialize_factor_slice(
    const fincore::dataframe& source,
    const std::span<const std::string_view> names,
    const std::size_t start,
    const std::size_t count) {
    const auto& index = source.get_index();
    fincore::dataframe result;
    result.load_index(
        std::vector<timestamp>{index.begin() + start, index.begin() + start + count});
    for (const std::string_view name : names) {
        const auto& values = source.get_column<double>(name.data());
        result.load_column<double>(
            std::string{name}.c_str(),
            std::vector<double>{values.begin() + start, values.begin() + start + count},
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

void expect_direct_fit_parity(
    const fincore::dataframe& exposures,
    const factor_fixture& fixture,
    const std::span<const std::string_view> names,
    const std::optional<std::size_t> rolling_window = std::nullopt) {
    const std::size_t minimum_observations = names.size() + 1U;
    for (std::size_t row = 0; row < fixture.returns.get_index().size(); ++row) {
        const std::size_t end = row + 1U;
        const std::size_t start =
            rolling_window && end > *rolling_window ? end - *rolling_window : 0U;
        const std::size_t count = end - start;
        if (count < minimum_observations) {
            continue;
        }
        const auto direct = fincore::fit_factor_regression(
            materialize_returns_slice(fixture.returns, start, count),
            materialize_factor_slice(fixture.factors, names, start, count), names);
        EXPECT_NEAR(exposures.get_column<double>(
                        fincore::factor_regression_alpha_name.data())[row],
                    direct.alpha, 1.0e-12);
        const auto& coefficients = direct.coefficients.get_column<double>(
            fincore::factor_regression_coefficient_column.data());
        for (std::size_t factor = 0; factor < names.size(); ++factor) {
            EXPECT_NEAR(exposures.get_column<double>(names[factor].data())[row],
                        coefficients[factor + 1U], 1.0e-12);
        }
    }
}

void expect_all_nan(const fincore::dataframe& exposures,
                    const std::span<const std::string_view> names) {
    const auto& alpha = exposures.get_column<double>(
        fincore::factor_regression_alpha_name.data());
    for (const double value : alpha) {
        EXPECT_TRUE(std::isnan(value));
    }
    for (const std::string_view name : names) {
        const auto& values = exposures.get_column<double>(name.data());
        for (const double value : values) {
            EXPECT_TRUE(std::isnan(value));
        }
    }
}

TEST(FactorExposures, MatchesSourceOracleForExpandingAndTrailingWindows) {
    const auto fixture = oracle_fixture();
    const auto expanding = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, factor_names);
    const auto fixed = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, factor_names, 4U);

    expect_exposure_schema(expanding, factor_names, fixture.returns.get_index());
    expect_exposure_schema(fixed, factor_names, fixture.returns.get_index());
    constexpr std::array<std::string_view, 4U> names{
        fincore::factor_regression_alpha_name, "MKT", "SMB", "HML"};
    for (const std::string_view name : names) {
        const auto& expanding_values = expanding.get_column<double>(name.data());
        const auto& fixed_values = fixed.get_column<double>(name.data());
        for (std::size_t row = 0; row < 3U; ++row) {
            EXPECT_TRUE(std::isnan(expanding_values[row]));
            EXPECT_TRUE(std::isnan(fixed_values[row]));
        }
        for (std::size_t row = 3U; row < oracle_index.size(); ++row) {
            EXPECT_TRUE(std::isfinite(expanding_values[row]));
            EXPECT_TRUE(std::isfinite(fixed_values[row]));
        }
    }
    expect_row(expanding, 3U, {.002333333333333203, 1.483333333333333,
                               -.5333333333333329, .13333333333333303});
    expect_row(expanding, 7U, {.0005694444444444665, 1.5310634180729992,
                               -.45298603018400957, .23286251744428613});
    expect_row(fixed, 7U, {.0008977272727272866, 1.4931818181818164,
                           -.4850000000000014, .23818181818181863});
}

TEST(FactorExposures, SupportsAnExplicitSelectedReturnsColumn) {
    const auto fixture = oracle_fixture();
    const auto& source_values = fixture.returns.get_column<double>("returns");
    const auto named_returns = make_returns(
        std::vector<timestamp>{oracle_index.begin(), oracle_index.end()},
        std::vector<double>{source_values.begin(), source_values.end()}, "strategy");

    const auto exposures = fincore::rolling_factor_exposures(
        named_returns, fixture.factors, factor_names, std::nullopt, "strategy");

    expect_exposure_schema(exposures, factor_names, named_returns.get_index());
    expect_row(exposures, 7U, {.0005694444444444665, 1.5310634180729992,
                               -.45298603018400957, .23286251744428613});
}

TEST(FactorExposures, DelegatesEachEligibleRawWindowToPublicFactorRegression) {
    const auto fixture = oracle_fixture();
    const auto expanding = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, factor_names);
    const auto fixed = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, factor_names, 4U);

    expect_direct_fit_parity(expanding, fixture, factor_names);
    expect_direct_fit_parity(fixed, fixture, factor_names, 4U);
}

TEST(FactorExposures, PreservesCallerPermutedFactorRequestOrder) {
    constexpr std::array<std::string_view, 3U> permuted_factor_names{
        "HML", "MKT", "SMB"};
    const auto fixture = oracle_fixture();
    const auto expanding = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, permuted_factor_names);
    const auto fixed = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, permuted_factor_names, 4U);

    expect_exposure_schema(expanding, permuted_factor_names,
                           fixture.returns.get_index());
    expect_exposure_schema(fixed, permuted_factor_names, fixture.returns.get_index());
    expect_direct_fit_parity(expanding, fixture, permuted_factor_names);
    expect_direct_fit_parity(fixed, fixture, permuted_factor_names, 4U);
}

TEST(FactorExposures,
     RetainsNonmonotonicUniqueRawOrderAndRowwiseExposureCorrespondence) {
    constexpr std::array<std::string_view, 1U> one_factor{"MKT"};
    const std::vector<timestamp> source_order{
        1704240000000000000LL, 1704067200000000000LL,
        1704412800000000000LL, 1704153600000000000LL,
        1704326400000000000LL};
    const factor_fixture fixture{
        make_returns(source_order, {.071, .028, .1215, .049, .0985}),
        make_factors(source_order, {.03, .01, .05, .02, .04},
                     {.02, .01, .03, .00, .04}, {.00, .02, -.01, .01, -.02})};

    const auto exposures = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, one_factor, 3U);

    expect_exposure_schema(exposures, one_factor, source_order);
    EXPECT_TRUE(std::isnan(exposures.get_column<double>(
        fincore::factor_regression_alpha_name.data())[0]));
    expect_direct_fit_parity(exposures, fixture, one_factor, 3U);
}

TEST(FactorExposures, ReturnsTypedEmptyAndShortTrailingOutputs) {
    const auto empty_returns = make_returns({}, {});
    const auto empty_factors = make_factors({}, {}, {}, {});
    const auto empty = fincore::rolling_factor_exposures(
        empty_returns, empty_factors, factor_names);
    expect_exposure_schema(empty, factor_names, {});
    EXPECT_TRUE(empty.get_column<double>(fincore::factor_regression_alpha_name.data())
                    .empty());
    for (const std::string_view name : factor_names) {
        EXPECT_TRUE(empty.get_column<double>(name.data()).empty());
    }

    const auto fixture = oracle_fixture();
    const auto short_window = fincore::rolling_factor_exposures(
        fixture.returns, fixture.factors, factor_names, 3U);
    expect_exposure_schema(short_window, factor_names, fixture.returns.get_index());
    expect_all_nan(short_window, factor_names);
}

TEST(FactorExposures, RejectsInvalidNamesWindowsIndexesAndSelectedValues) {
    const auto fixture = oracle_fixture();
    constexpr std::array<std::string_view, 0U> no_factors{};
    constexpr std::array<std::string_view, 1U> empty_factor{""};
    constexpr std::array<std::string_view, 2U> duplicate_factor{"MKT", "MKT"};
    constexpr std::array<std::string_view, 1U> alpha_factor{"alpha"};
    constexpr std::array<std::string_view, 1U> missing_factor{"missing"};
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   no_factors),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   empty_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   duplicate_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   alpha_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   missing_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   factor_names, 0U),
                 fincore::input_error);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, fixture.factors,
                                                   factor_names, std::nullopt,
                                                   "missing_returns"),
                 fincore::input_error);

    auto wrong_return_type = fixture.returns;
    wrong_return_type.remove_column<double>("returns");
    wrong_return_type.load_column<int>("returns", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::rolling_factor_exposures(wrong_return_type,
                                                   fixture.factors, factor_names),
                 fincore::input_error);

    auto wrong_factor_type = fixture.factors;
    wrong_factor_type.remove_column<double>("MKT");
    wrong_factor_type.load_column<int>("MKT", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns,
                                                   wrong_factor_type, factor_names),
                 fincore::input_error);

    auto short_return = fixture.returns;
    short_return.remove_column<double>("returns");
    short_return.load_column<double>("returns", {.1},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::rolling_factor_exposures(short_return, fixture.factors,
                                                   factor_names),
                 fincore::input_error);

    auto short_factor = fixture.factors;
    short_factor.remove_column<double>("SMB");
    short_factor.load_column<double>("SMB", {.1},
                                     hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, short_factor,
                                                   factor_names),
                 fincore::input_error);

    auto duplicate_returns_index = fixture.returns;
    duplicate_returns_index.load_index(
        {oracle_index[0], oracle_index[0], oracle_index[2], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::rolling_factor_exposures(duplicate_returns_index,
                                                   fixture.factors, factor_names),
                 fincore::input_error);

    auto duplicate_factor_index = fixture.factors;
    duplicate_factor_index.load_index(
        {oracle_index[0], oracle_index[1], oracle_index[1], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns,
                                                   duplicate_factor_index, factor_names),
                 fincore::input_error);

    auto mismatched_index = fixture.factors;
    mismatched_index.load_index(
        {oracle_index[1], oracle_index[0], oracle_index[2], oracle_index[3],
         oracle_index[4], oracle_index[5], oracle_index[6], oracle_index[7]});
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns, mismatched_index,
                                                   factor_names),
                 fincore::input_error);

    auto nan_returns = fixture.returns;
    nan_returns.get_column<double>("returns")[0] =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::rolling_factor_exposures(nan_returns, fixture.factors,
                                                   factor_names, 3U),
                 fincore::input_error);

    auto positive_infinite_factor = fixture.factors;
    positive_infinite_factor.get_column<double>("MKT")[0] =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns,
                                                   positive_infinite_factor,
                                                   factor_names, 3U),
                 fincore::input_error);

    auto negative_infinite_factor = fixture.factors;
    negative_infinite_factor.get_column<double>("SMB")[0] =
        -std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns,
                                                   negative_infinite_factor,
                                                   factor_names, 3U),
                 fincore::input_error);

    auto rank_deficient_factors = fixture.factors;
    rank_deficient_factors.get_column<double>("SMB") =
        rank_deficient_factors.get_column<double>("MKT");
    EXPECT_THROW(fincore::rolling_factor_exposures(fixture.returns,
                                                   rank_deficient_factors,
                                                   factor_names),
                 fincore::input_error);
}

TEST(FactorExposures, IgnoresUnrelatedMetadataAndOwnsResult) {
    const auto result = [] {
        auto fixture = oracle_fixture();
        const auto clean_factors = [&fixture] {
            auto copy = fixture.factors;
            copy.remove_column<int>("unrelated_malformed");
            return copy;
        }();
        std::array<std::string, 3U> source_factor_names{"MKT", "SMB", "HML"};
        const std::array<std::string_view, 3U> source_factor_name_views{
            source_factor_names[0], source_factor_names[1], source_factor_names[2]};
        auto output = fincore::rolling_factor_exposures(
            fixture.returns, fixture.factors, source_factor_name_views);
        const auto clean_output = fincore::rolling_factor_exposures(
            fixture.returns, clean_factors, source_factor_name_views);
        expect_same_values(output, clean_output, source_factor_name_views);
        fixture.returns.get_column<double>("returns")[0] = 123.0;
        fixture.factors.get_column<double>("MKT")[0] = -123.0;
        fixture.returns.load_index({11, 12, 13, 14, 15, 16, 17, 18});
        fixture.factors.load_index({21, 22, 23, 24, 25, 26, 27, 28});
        source_factor_names[0] = "mutated-market";
        return output;
    }();

    expect_exposure_schema(result, factor_names,
                           std::vector<timestamp>{oracle_index.begin(), oracle_index.end()});
    expect_row(result, 7U, {.0005694444444444665, 1.5310634180729992,
                            -.45298603018400957, .23286251744428613});
}

}  // namespace
